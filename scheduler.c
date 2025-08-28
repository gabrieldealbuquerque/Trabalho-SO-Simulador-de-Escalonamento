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
    // Reinicia variáveis de sincronização do multiprocessador
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
        if (pcb_get_state(pcb) == RUNNING && pcb_get_remaining_time(pcb) > 0) {
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

    // Nao liberar tcb aqui para evitar dangling pointers em filas concorrentes.
    // A vida do TCB é gerenciada indiretamente pelo fim do programa.
    return NULL;
}

void* scheduler_func(void* arg) {
    int processor_id = (intptr_t)arg;
#ifndef MULTI
    (void)processor_id;
#endif

    while (1) {
        pthread_mutex_lock(&scheduler_mutex);
        while (is_empty(&ready_queue) && !generator_done) {
            pthread_cond_wait(&scheduler_cv, &scheduler_mutex);
        }
        
        if (is_empty(&ready_queue) && generator_done) {
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
        
        if (is_empty(&ready_queue)) {
            pthread_mutex_unlock(&scheduler_mutex);
            continue;
        }
        
        QueueItem item;
        if (scheduler_type == 3) { // PRIORITY
            item = remove_highest_priority(&ready_queue);
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

        // Se o processo já terminou (pode ocorrer devido a corrida), pule
        if (pcb_get_state(pcb) == FINISHED || pcb_get_remaining_time(pcb) <= 0) {
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
        } else { // PRIORITY - preemptivo
            // fica sempre checando pra ver se um novo processo com maior prioridade chegou
            int finished = 0;
            while (!finished) {
                struct timespec wait_ts = {0, 10000000}; // Check every 10ms
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
    return "PRIORITY";
}