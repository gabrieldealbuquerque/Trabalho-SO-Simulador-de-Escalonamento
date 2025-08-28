#define _POSIX_C_SOURCE 199309L
#include "scheduler.h"
#include "tcb.h"
#include "log.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

ReadyQueue ready_queue;
pthread_cond_t scheduler_cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;
int generator_done = 0;

// sincronizacao para multiprocessador
pthread_mutex_t termination_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t termination_cv = PTHREAD_COND_INITIALIZER;
int processors_finished = 0;
int termination_logged = 0;

void init_scheduler() {
    init_queue(&ready_queue);
    // reinicia variaveis de sync do multiprocessador
    processors_finished = 0;
    termination_logged = 0;
}

void* thread_func(void* arg) {
    TCB* tcb = (TCB*)arg;
    PCB* pcb = tcb->pcb;

    while (1) {
        pcb_lock_mutex(pcb);
        while (pcb_get_state(pcb) != RUNNING && pcb_get_remaining_time(pcb) > 0) {
            pcb_wait_cv(pcb);
        }
        if (pcb_get_remaining_time(pcb) <= 0 || pcb_get_state(pcb) == FINISHED) {
            pcb_unlock_mutex(pcb);
            break;
        }
        
        struct timespec ts = {0, 500 * 1000000LL};
        pcb_unlock_mutex(pcb);
        nanosleep(&ts, NULL);
        
        pcb_lock_mutex(pcb);
        // CFS mono, scheduler conta pra evitar dupla contagem
        int skip_account = (scheduler_type == 4) && !is_multiprocessor_mode();
        if (!skip_account && pcb_get_state(pcb) == RUNNING && pcb_get_remaining_time(pcb) > 0) {
            // multi: cada thread colabora para o avanco do processo
            int slice = 500 / pcb_get_num_threads(pcb);
            if (slice < 1) slice = 1;
            pcb_decrease_remaining_time(pcb, slice);
            if (pcb_get_remaining_time(pcb) <= 0) {
                pcb_set_state(pcb, FINISHED);
                pcb_broadcast_cv(pcb);
            }
        }
        pcb_unlock_mutex(pcb);
    }
    // Em modo monoprocessador, o TCB não fica em filas; podemos liberar aqui
    if (!is_multiprocessor_mode()) {
        free(tcb);
    }
    return NULL;
}

void* scheduler_func(void* arg) {
    int processor_id = (intptr_t)arg;
#ifndef MULTI
    (void)processor_id;
#endif

    while (1) {
        pthread_mutex_lock(&scheduler_mutex);
        int queue_empty = (scheduler_type == 4) ? cfs_is_empty(&ready_queue) : is_empty(&ready_queue);
        while (queue_empty && !generator_done) {
            pthread_cond_wait(&scheduler_cv, &scheduler_mutex);
            queue_empty = (scheduler_type == 4) ? cfs_is_empty(&ready_queue) : is_empty(&ready_queue);
        }
        
        if (queue_empty && generator_done) {
            int all_finished = 1;
            for (int i = 0; i < num_processes; i++) {
                PCB* check_pcb = &pcb_list[i];
                pcb_lock_mutex(check_pcb);
                if (pcb_get_state(check_pcb) != FINISHED) {
                    all_finished = 0;
                }
                pcb_unlock_mutex(check_pcb);
            }
            
            if (!all_finished) {
                pthread_mutex_unlock(&scheduler_mutex);
                struct timespec wait_ts = {0, 10000000}; 
                nanosleep(&wait_ts, NULL);
                continue;
            }
            
            pthread_mutex_unlock(&scheduler_mutex);
            break;
        }
        
        queue_empty = (scheduler_type == 4) ? cfs_is_empty(&ready_queue) : is_empty(&ready_queue);
        if (queue_empty) {
            pthread_mutex_unlock(&scheduler_mutex);
            continue;
        }
        
        QueueItem item;
        if (scheduler_type == 3) { // PRIORITY
            item = remove_highest_priority(&ready_queue);
        } else if (scheduler_type == 4) { // CFS
            item = cfs_dequeue(&ready_queue);
        } else {
            item = dequeue(&ready_queue);
        }
        pthread_mutex_unlock(&scheduler_mutex);
        
        if (!item) {
            continue;
        }

        PCB* pcb;
        TCB* tcb = NULL;
        if (is_multiprocessor_mode()) {
            tcb = (TCB*)item;
            pcb = tcb->pcb;
        } else {
            pcb = (PCB*)item;
        }

        // se o processo ja terminou, libera TCB (multi) e pula
        if (pcb_get_state(pcb) == FINISHED || pcb_get_remaining_time(pcb) <= 0) {
            if (is_multiprocessor_mode() && tcb) {
                free(tcb);
            }
            continue;
        }

        pcb_lock_mutex(pcb);
        pcb_set_state(pcb, RUNNING);
        pcb_broadcast_cv(pcb);
        pcb_unlock_mutex(pcb);

        const char* policy = get_policy_string();
        if (scheduler_type == 2) { // RR
            if (is_multiprocessor_mode()) {
                log_event("[%s] Executando processo PID %d com quantum %dms // processador %d\n", policy, pcb_get_pid(pcb), quantum, processor_id);
            } else {
                log_event("[%s] Executando processo PID %d com quantum %dms\n", policy, pcb_get_pid(pcb), quantum);
            }
        } else if (scheduler_type == 3) { // PRIORITY
            if (is_multiprocessor_mode()) {
                log_event("[%s] Executando processo PID %d prioridade %d // processador %d\n", policy, pcb_get_pid(pcb), pcb_get_priority(pcb), processor_id);
            } else {
                log_event("[%s] Executando processo PID %d prioridade %d\n", policy, pcb_get_pid(pcb), pcb_get_priority(pcb));
            }
        } else if (scheduler_type == 4) { // CFS
            if (is_multiprocessor_mode()) {
                log_event("[%s] Executando processo PID %d vruntime %lld // processador %d\n", policy, pcb_get_pid(pcb), pcb_get_vruntime(pcb), processor_id);
            } else {
                log_event("[%s] Executando processo PID %d vruntime %lld\n", policy, pcb_get_pid(pcb), pcb_get_vruntime(pcb));
            }
        } else { // FCFS
            if (is_multiprocessor_mode()) {
                log_event("[%s] Executando processo PID %d // processador %d\n", policy, pcb_get_pid(pcb), processor_id);
            } else {
                log_event("[%s] Executando processo PID %d\n", policy, pcb_get_pid(pcb));
            }
        }

        if (scheduler_type == 1) { // FCFS
            if (is_multiprocessor_mode()) {
                // em multi, executa a thread e espera acabar
                int finished = 0;
                while (!finished) {
                    struct timespec wait_ts = {0, 10000000}; // cheaca a cada 10 sec
                    nanosleep(&wait_ts, NULL);
                    pcb_lock_mutex(pcb);
                    if (pcb_get_state(pcb) == FINISHED) {
                        if (!pcb_get_logged_finish(pcb)) {
                            log_event("[%s] Processo PID %d finalizado\n", policy, pcb_get_pid(pcb));
                            pcb_set_logged_finish(pcb, 1);
                        }
                        finished = 1;
                    }
                    pcb_unlock_mutex(pcb);
                }
                // terminou e nao re-enfileira; libera TCB
                if (tcb) free(tcb);
            } else {
                // em mono, espera o processo terminar
                long long wait_start = current_time_ms();
                int finished = 0;
                while (!finished) {
                    struct timespec wait_ts = {0, 1000000};
                    nanosleep(&wait_ts, NULL);
                    pcb_lock_mutex(pcb);
                    if (pcb_get_state(pcb) == FINISHED) {
                        if (!pcb_get_logged_finish(pcb)) {
                            log_event("[%s] Processo PID %d finalizado\n", policy, pcb_get_pid(pcb));
                            pcb_set_logged_finish(pcb, 1);
                        }
                        finished = 1;
                    }
                    pcb_unlock_mutex(pcb);
                    if (current_time_ms() - wait_start > 60000) {
                        log_event("Timeout waiting for process %d to finish\n", pcb_get_pid(pcb));
                        break;
                    }
                }
            }
    } else if (scheduler_type == 2) { // RR
            // checa constantemente pra ver se o quantum terminou
            long long start_time = current_time_ms();
            int finished = 0;
            while (!finished && (current_time_ms() - start_time) < quantum) {
                struct timespec wait_ts = {0, 10000000};
                nanosleep(&wait_ts, NULL);
                pcb_lock_mutex(pcb);
                if (pcb_get_state(pcb) == FINISHED) {
                    if (!pcb_get_logged_finish(pcb)) {
                        log_event("[%s] Processo PID %d finalizado\n", policy, pcb_get_pid(pcb));
                        pcb_set_logged_finish(pcb, 1);
                    }
                    finished = 1;
                }
                pcb_unlock_mutex(pcb);
            }
            
            pcb_lock_mutex(pcb);
            if (pcb_get_state(pcb) == RUNNING && pcb_get_remaining_time(pcb) > 0) {
                // quantum terminou, preempta e readciona na queue
                pcb_set_state(pcb, READY);
                if (is_multiprocessor_mode()) {
                    enqueue(&ready_queue, (QueueItem)tcb);
                } else {
                    enqueue(&ready_queue, (QueueItem)pcb);
                }
                pthread_mutex_lock(&scheduler_mutex);
                pthread_cond_broadcast(&scheduler_cv);
                pthread_mutex_unlock(&scheduler_mutex);
            }
            pcb_unlock_mutex(pcb);
            // se terminou durante o slice, em multi podemos liberar o TCB
            if (is_multiprocessor_mode() && finished && tcb) {
                free(tcb);
            }
        } else if (scheduler_type == 3) { // PRIORITY - preemptivo
            // fica sempre checando pra ver se um novo processo com maior prioridade chegou
            int finished = 0;
            while (!finished) {
                struct timespec wait_ts = {0, 10000000}; 
                nanosleep(&wait_ts, NULL);
                
                pcb_lock_mutex(pcb);
                if (pcb_get_state(pcb) == FINISHED) {
                    if (!pcb_get_logged_finish(pcb)) {
                        log_event("[%s] Processo PID %d finalizado\n", policy, pcb_get_pid(pcb));
                        pcb_set_logged_finish(pcb, 1);
                    }
                    finished = 1;
                    pcb_unlock_mutex(pcb);
                    break;
                }
                pcb_unlock_mutex(pcb);
                
                // checa a fila
                pthread_mutex_lock(&scheduler_mutex);
                if (!is_empty(&ready_queue)) {
                    QueueItem next_item = peek_highest_priority(&ready_queue);
                    if (next_item) {
                        int next_priority;
                        if (is_multiprocessor_mode()) {
                            next_priority = pcb_get_priority(((TCB*)next_item)->pcb);
                        } else {
                            next_priority = pcb_get_priority((PCB*)next_item);
                        }
                        
                        if (next_priority < pcb_get_priority(pcb)) {
                            // achou um maior, preempta o atual
                            pcb_lock_mutex(pcb);
                            if (pcb_get_state(pcb) == RUNNING) {
                                pcb_set_state(pcb, READY);
                                if (is_multiprocessor_mode()) {
                                    enqueue(&ready_queue, (QueueItem)tcb);
                                } else {
                                    enqueue(&ready_queue, (QueueItem)pcb);
                                }
                                pthread_cond_broadcast(&scheduler_cv);
                                pcb_unlock_mutex(pcb);
                                pthread_mutex_unlock(&scheduler_mutex);
                                break;
                            }
                            pcb_unlock_mutex(pcb);
                        }
                    }
                }
                pthread_mutex_unlock(&scheduler_mutex);
            }
            // se terminou (nao houve re-enfileiramento), libera TCB em multi
            if (is_multiprocessor_mode() && tcb) {
                int fin;
                pcb_lock_mutex(pcb);
                fin = (pcb_get_state(pcb) == FINISHED);
                pcb_unlock_mutex(pcb);
                if (fin) free(tcb);
            }
        } else if (scheduler_type == 4) { // CFS
            // CFS: fatia dinamica
            const int target_latency_ms = 1000;     // tempo para percorrer todas as tasks (menos trocas)
            const int min_granularity_ms = 250;     // aqui é pro log nao ficar tao imenso

            // Calcula fatia: target_latency / nr_running
            int nr_running = cfs_size(&ready_queue) + 1; // inclui a atual
            int timeslice = target_latency_ms / (nr_running > 0 ? nr_running : 1);
            if (timeslice < min_granularity_ms) timeslice = min_granularity_ms;

            long long slice_start = current_time_ms();
            long long last_check = slice_start;
            int finished = 0;

            // inicio do vruntime
            pcb_lock_mutex(pcb);
            pcb_set_exec_start(pcb, slice_start);
            pcb_unlock_mutex(pcb);

            while (!finished) {
                // dorme 1ms para granularidade fina
                struct timespec wait_ts = {0, 1000000};
                nanosleep(&wait_ts, NULL);

                long long now = current_time_ms();
                long long delta = now - last_check;
                last_check = now;

                if (!is_multiprocessor_mode()) {
                    pcb_lock_mutex(pcb);
                    if (pcb_get_state(pcb) == RUNNING && pcb_get_remaining_time(pcb) > 0) {
                        pcb_decrease_remaining_time(pcb, (int)delta);
                        if (pcb_get_remaining_time(pcb) <= 0) {
                            // atualiza vruntime pelo tempo executado nesta fatia
                            long long exec_time = now - pcb_get_exec_start(pcb);
                            pcb_update_vruntime(pcb, exec_time);
                            pcb_set_state(pcb, FINISHED);
                            if (!pcb_get_logged_finish(pcb)) {
                                log_event("[%s] Processo PID %d finalizado\n", policy, pcb_get_pid(pcb));
                                pcb_set_logged_finish(pcb, 1);
                            }
                            finished = 1;
                            pcb_unlock_mutex(pcb);
                            break;
                        }
                    }
                    pcb_unlock_mutex(pcb);
                } else {
                    // em multi, observa termino assincrono
                    pcb_lock_mutex(pcb);
                    if (pcb_get_state(pcb) == FINISHED) {
                        if (!pcb_get_logged_finish(pcb)) {
                            log_event("[%s] Processo PID %d finalizado\n", policy, pcb_get_pid(pcb));
                            pcb_set_logged_finish(pcb, 1);
                        }
                        finished = 1;
                        pcb_unlock_mutex(pcb);
                        break;
                    }
                    pcb_unlock_mutex(pcb);
                }

                // preempcao
                if (!cfs_is_empty(&ready_queue)) {
                    long long min_vruntime = cfs_get_min_vruntime(&ready_queue);
                    long long ran_since_start = now - slice_start;
                    long long projected = pcb_get_vruntime(pcb) + pcb_calc_delta_fair(pcb, ran_since_start);
                    if (projected > min_vruntime + min_granularity_ms) {
                        break; // preempta
                    }
                }

                if (now - slice_start >= timeslice) {
                    break; // fim da fatia
                }
            }

            // encerramento da fatia / preempcao
            pcb_lock_mutex(pcb);
            if (!finished && pcb_get_state(pcb) == RUNNING && pcb_get_remaining_time(pcb) > 0) {
                long long exec_time = current_time_ms() - pcb_get_exec_start(pcb);
                pcb_update_vruntime(pcb, exec_time);
                pcb_set_state(pcb, READY);
                if (is_multiprocessor_mode()) {
                    cfs_enqueue(&ready_queue, (QueueItem)tcb);
                } else {
                    cfs_enqueue(&ready_queue, (QueueItem)pcb);
                }
                pthread_mutex_lock(&scheduler_mutex);
                pthread_cond_broadcast(&scheduler_cv);
                pthread_mutex_unlock(&scheduler_mutex);
            } else if (finished && is_multiprocessor_mode() && tcb) {
                // terminou durante a fatia (sem reenfileirar)
                // pcb->state ja foi setado para FINISHED
                // seguro liberar o TCB
                free(tcb);
            } else if (is_multiprocessor_mode() && tcb && pcb_get_state(pcb) != RUNNING) {
                // Caso o processo finalize exatamente ao fim da fatia sem marcarmos 'finished'
                // (condição de corrida), garanta que o TCB não vaze.
                free(tcb);
            }
            pcb_unlock_mutex(pcb);
        }
    }
    
    if (is_multiprocessor_mode()) {
        // sincronizacao do multiprocessador
        pthread_mutex_lock(&termination_mutex);
        processors_finished++;
        
        if (processors_finished == get_cpu_count()) {
            // o ultimo processador a terminar printa a finalizacao
            if (!termination_logged) {
                log_event("Escalonador terminou execução de todos processos\n");
                termination_logged = 1;
            }
            pthread_cond_broadcast(&termination_cv);
        } else {
            // espera todos terminarem
            while (processors_finished < get_cpu_count()) {
                pthread_cond_wait(&termination_cv, &termination_mutex);
            }
        }
        pthread_mutex_unlock(&termination_mutex);
    } else {
        log_event("Escalonador terminou execução de todos processos\n");
    }
    return NULL;
}

char* get_policy_string() {
    if (scheduler_type == 1) return "FCFS";
    if (scheduler_type == 2) return "RR";
    if (scheduler_type == 3) return "PRIORITY";
    if (scheduler_type == 4) return "CFS";
    return "UNKNOWN";
}