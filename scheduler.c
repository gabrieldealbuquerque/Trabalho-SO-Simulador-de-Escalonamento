#define _POSIX_C_SOURCE 199309L
#include "scheduler.h"
#include "tcb.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

#ifdef MULTI
#define NUM_CPUS 2
#else
#define NUM_CPUS 1
#endif

ReadyQueue ready_queue;
pthread_cond_t scheduler_cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;
int generator_done = 0;

// For multiprocessor termination synchronization
#ifdef MULTI
pthread_mutex_t termination_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t termination_cv = PTHREAD_COND_INITIALIZER;
int processors_finished = 0;
int termination_logged = 0;
#endif

void init_scheduler() {
    init_queue(&ready_queue);
}

void* thread_func(void* arg) {
    TCB* tcb = (TCB*)arg;
    PCB* pcb = tcb->pcb;

    while (1) {
        pthread_mutex_lock(&pcb->mutex);
        while (pcb->state != RUNNING && pcb->remaining_time > 0) {
            pthread_cond_wait(&pcb->cv, &pcb->mutex);
        }
        if (pcb->remaining_time <= 0 || pcb->state == FINISHED) {
            pthread_mutex_unlock(&pcb->mutex);
            break;
        }
        
        // Execute 500ms slice
        struct timespec ts = {0, 500 * 1000000LL};
        pthread_mutex_unlock(&pcb->mutex);
        nanosleep(&ts, NULL);
        
        pthread_mutex_lock(&pcb->mutex);
        if (pcb->state == RUNNING && pcb->remaining_time > 0) {
            // In multi, each thread contributes independently to the process execution
            int slice = 500 / pcb->num_threads;
            if (slice < 1) slice = 1;
            pcb->remaining_time -= slice;
            if (pcb->remaining_time <= 0) {
                pcb->state = FINISHED;
                pthread_cond_broadcast(&pcb->cv);
            }
        }
        pthread_mutex_unlock(&pcb->mutex);
    }

    free(tcb);
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
            // Check if all processes are finished before terminating
            int all_finished = 1;
            for (int i = 0; i < num_processes; i++) {
                PCB* check_pcb = &pcb_list[i];
                pthread_mutex_lock(&check_pcb->mutex);
                if (check_pcb->state != FINISHED) {
                    all_finished = 0;
                }
                pthread_mutex_unlock(&check_pcb->mutex);
            }
            
            if (!all_finished) {
                pthread_mutex_unlock(&scheduler_mutex);
                struct timespec wait_ts = {0, 10000000}; // Wait 10ms
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
#ifdef MULTI
        TCB* tcb = (TCB*)item;
        pcb = tcb->pcb;
#else
        pcb = (PCB*)item;
#endif

        pthread_mutex_lock(&pcb->mutex);
        pcb->state = RUNNING;
        pthread_cond_broadcast(&pcb->cv);
        pthread_mutex_unlock(&pcb->mutex);

        const char* policy = get_policy_string();
        if (scheduler_type == 2) { // RR
#ifdef MULTI
            log_event("[%s] Executando processo PID %d com quantum %dms // processador %d\n", policy, pcb->pid, quantum, processor_id);
#else
            log_event("[%s] Executando processo PID %d com quantum %dms\n", policy, pcb->pid, quantum);
#endif
        } else if (scheduler_type == 3) { // PRIORITY
#ifdef MULTI
            log_event("[%s] Executando processo PID %d prioridade %d // processador %d\n", policy, pcb->pid, pcb->priority, processor_id);
#else
            log_event("[%s] Executando processo PID %d prioridade %d\n", policy, pcb->pid, pcb->priority);
#endif
        } else { // FCFS
#ifdef MULTI
            log_event("[%s] Executando processo PID %d // processador %d\n", policy, pcb->pid, processor_id);
#else
            log_event("[%s] Executando processo PID %d\n", policy, pcb->pid);
#endif
        }

        if (scheduler_type == 1) { // FCFS
#ifdef MULTI
            // In multiprocessor FCFS, execute the thread and wait for completion
            int finished = 0;
            while (!finished) {
                struct timespec wait_ts = {0, 10000000}; // Check every 10ms
                nanosleep(&wait_ts, NULL);
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED) {
                    if (!pcb->logged_finish) {
                        log_event("[%s] Processo PID %d finalizado\n", policy, pcb->pid);
                        pcb->logged_finish = 1;
                    }
                    finished = 1;
                }
                pthread_mutex_unlock(&pcb->mutex);
            }
#else
            // FCFS mono - wait for process to complete naturally
            long long wait_start = current_time_ms();
            int finished = 0;
            while (!finished) {
                struct timespec wait_ts = {0, 1000000};
                nanosleep(&wait_ts, NULL);
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED) {
                    if (!pcb->logged_finish) {
                        log_event("[%s] Processo PID %d finalizado\n", policy, pcb->pid);
                        pcb->logged_finish = 1;
                    }
                    finished = 1;
                }
                pthread_mutex_unlock(&pcb->mutex);
                if (current_time_ms() - wait_start > 60000) {
                    log_event("Timeout waiting for process %d to finish\n", pcb->pid);
                    break;
                }
            }
#endif
        } else if (scheduler_type == 2) { // RR
            // Round Robin - check periodically if process finishes or quantum expires
            long long start_time = current_time_ms();
            int finished = 0;
            while (!finished && (current_time_ms() - start_time) < quantum) {
                struct timespec wait_ts = {0, 10000000}; // Check every 10ms
                nanosleep(&wait_ts, NULL);
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED) {
                    if (!pcb->logged_finish) {
                        log_event("[%s] Processo PID %d finalizado\n", policy, pcb->pid);
                        pcb->logged_finish = 1;
                    }
                    finished = 1;
                }
                pthread_mutex_unlock(&pcb->mutex);
            }
            
            pthread_mutex_lock(&pcb->mutex);
            if (pcb->state == RUNNING && pcb->remaining_time > 0) {
                // Quantum expired, preempt and reenqueue
                pcb->state = READY;
#ifdef MULTI
                enqueue(&ready_queue, (QueueItem)tcb);
#else
                enqueue(&ready_queue, (QueueItem)pcb);
#endif
                pthread_mutex_lock(&scheduler_mutex);
                pthread_cond_broadcast(&scheduler_cv);
                pthread_mutex_unlock(&scheduler_mutex);
            }
            pthread_mutex_unlock(&pcb->mutex);
        } else { // PRIORITY - preemptive
            // Check continuously for higher priority processes arriving
            int finished = 0;
            while (!finished) {
                struct timespec wait_ts = {0, 10000000}; // Check every 10ms
                nanosleep(&wait_ts, NULL);
                
                pthread_mutex_lock(&pcb->mutex);
                if (pcb->state == FINISHED) {
                    log_event("[%s] Processo PID %d finalizado\n", policy, pcb->pid);
                    finished = 1;
                    pthread_mutex_unlock(&pcb->mutex);
                    break;
                }
                pthread_mutex_unlock(&pcb->mutex);
                
                // Check if there's a higher priority process in queue
                pthread_mutex_lock(&scheduler_mutex);
                if (!is_empty(&ready_queue)) {
                    // Peek at highest priority in queue
                    QueueItem next_item = peek_highest_priority(&ready_queue);
                    if (next_item) {
                        int next_priority;
#ifdef MULTI
                        next_priority = ((TCB*)next_item)->pcb->priority;
#else
                        next_priority = ((PCB*)next_item)->priority;
#endif
                        
                        if (next_priority < pcb->priority) {
                            // Higher priority process available, preempt current
                            pthread_mutex_lock(&pcb->mutex);
                            if (pcb->state == RUNNING) {
                                pcb->state = READY;
#ifdef MULTI
                                enqueue(&ready_queue, (QueueItem)tcb);
#else
                                enqueue(&ready_queue, (QueueItem)pcb);
#endif
                                pthread_cond_broadcast(&scheduler_cv);
                                pthread_mutex_unlock(&pcb->mutex);
                                pthread_mutex_unlock(&scheduler_mutex);
                                break;
                            }
                            pthread_mutex_unlock(&pcb->mutex);
                        }
                    }
                }
                pthread_mutex_unlock(&scheduler_mutex);
            }
        }
    }
    
#ifdef MULTI
    // Synchronize termination in multiprocessor
    pthread_mutex_lock(&termination_mutex);
    processors_finished++;
    
    if (processors_finished == NUM_CPUS) {
        // Last processor to finish logs termination
        if (!termination_logged) {
            log_event("Escalonador terminou execução de todos processos\n");
            termination_logged = 1;
        }
        pthread_cond_broadcast(&termination_cv);
    } else {
        // Wait for all processors to finish
        while (processors_finished < NUM_CPUS) {
            pthread_cond_wait(&termination_cv, &termination_mutex);
        }
    }
    pthread_mutex_unlock(&termination_mutex);
#else
    log_event("Escalonador terminou execução de todos processos\n");
#endif
    return NULL;
}

char* get_policy_string() {
    if (scheduler_type == 1) return "FCFS";
    if (scheduler_type == 2) return "RR";
    return "PRIORITY";
}