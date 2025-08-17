#include "scheduler.h"
#include "pcb.h"
#include "tcb.h"
#include "queue.h"
#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>

// Global variables
extern ReadyQueue ready_queue;
extern pthread_cond_t scheduler_cv;
extern pthread_mutex_t scheduler_mutex;
extern int generator_done;

int num_processes;
int quantum;
int scheduler_type;
PCB* pcb_list;
long long simulation_start_ms;

// Function to get current time in ms
long long current_time_ms() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        fprintf(stderr, "gettimeofday failed\n");
        return 0;
    }
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
}

// Add generator function
void* generator_func(void* arg) {
    (void)arg;
    long long start_time = current_time_ms();
    simulation_start_ms = start_time;
    int idx = 0;
    while (idx < num_processes) {
        PCB* pcb = &pcb_list[idx];
        long long now = current_time_ms() - start_time;
        if (now >= pcb_get_start_time(pcb)) {
            if (is_multiprocessor_mode()) {
                // In multi, enqueue each thread (TCB) individually
                for (int j = 0; j < pcb_get_num_threads(pcb); j++) {
                    TCB* tcb = malloc(sizeof(TCB));
                    tcb->pcb = pcb;
                    tcb->thread_index = j;
                    tcb->last_processor_id = -1; // Initialize as unassigned
                    tcb->preferred_processor_id = -1; // Will be assigned when enqueued
                    pthread_create(&pcb_get_thread_ids(pcb)[j], NULL, thread_func, tcb);
                    enqueue(&ready_queue, (QueueItem)tcb);
                }
            } else {
                // In mono, create threads but enqueue the process (PCB)
                for (int j = 0; j < pcb_get_num_threads(pcb); j++) {
                    TCB* tcb = malloc(sizeof(TCB));
                    tcb->pcb = pcb;
                    tcb->thread_index = j;
                    tcb->last_processor_id = -1; // Initialize as unassigned
                    tcb->preferred_processor_id = -1; // Will be assigned when enqueued
                    pthread_create(&pcb_get_thread_ids(pcb)[j], NULL, thread_func, tcb);
                }
                enqueue(&ready_queue, (QueueItem)pcb);
            }
            pthread_mutex_lock(&scheduler_mutex);
            pthread_cond_broadcast(&scheduler_cv);
            pthread_mutex_unlock(&scheduler_mutex);
            idx++;
        } else {
            struct timespec ts = {0, 1000000};
            nanosleep(&ts, NULL);
        }
    }
    generator_done = 1;
    pthread_mutex_lock(&scheduler_mutex);
    pthread_cond_broadcast(&scheduler_cv);
    pthread_mutex_unlock(&scheduler_mutex);
    return NULL;
}

int compare_start_time(const void* a, const void* b) {
    PCB* pcb_a = (PCB*)a;
    PCB* pcb_b = (PCB*)b;
    return pcb_get_start_time(pcb_a) - pcb_get_start_time(pcb_b);
}

// Main function
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        fprintf(stderr, "Note: Processor mode (mono/multi) is determined at compile time\n");
        return 1;
    }
    
    // Initialize configuration (mode automatically detected from compile flags)
    init_config();

    FILE* input = fopen(argv[1], "r");
    if (!input) {
        fprintf(stderr, "Failed to open input file\n");
        return 1;
    }

    fscanf(input, "%d", &num_processes);
    pcb_list = malloc(num_processes * sizeof(PCB));

    for (int i = 0; i < num_processes; i++) {
        PCB* pcb = &pcb_list[i];
        int process_len, priority, num_threads, start_time;
        fscanf(input, "%d %d %d %d", &process_len, &priority, &num_threads, &start_time);
        pcb_set_process_len(pcb, process_len);
        pcb_set_priority(pcb, priority);
        pcb_set_num_threads(pcb, num_threads);
        pcb_set_start_time(pcb, start_time);
        pcb_set_pid(pcb, i + 1); // PIDs start from 1
        // Process gets full duration, threads share it
        pcb_set_remaining_time(pcb, process_len);
        pcb_set_state(pcb, READY);
        pcb_set_logged_finish(pcb, 0); // Initialize finish log flag
        pcb_init_mutex(pcb);
        pcb_init_cv(pcb);
        pcb_set_thread_ids(pcb, malloc(num_threads * sizeof(pthread_t)));
    }

    fscanf(input, "%d", &scheduler_type);
    // Assuming quantum is fixed or read if needed; spec seems to have it in input?
    quantum = 500; // Default quantum 500ms

    fclose(input);

    // Set deterministic seed for reproducible results
    srand(12345);
    
    init_log();
    init_scheduler();

    int cpu_count = get_cpu_count();
    pthread_t scheduler_threads[cpu_count];
    for (int i = 0; i < cpu_count; i++) {
        pthread_create(&scheduler_threads[i], NULL, scheduler_func, (void*)(intptr_t)i);
    }

    qsort(pcb_list, num_processes, sizeof(PCB), compare_start_time);

    pthread_t generator_thread;
    pthread_create(&generator_thread, NULL, generator_func, NULL);

    // Wait for all processes to finish
    pthread_join(generator_thread, NULL);
    for (int i = 0; i < cpu_count; i++) {
        pthread_join(scheduler_threads[i], NULL);
    }
    for (int i = 0; i < num_processes; i++) {
        for (int j = 0; j < pcb_get_num_threads(&pcb_list[i]); j++) {
            pthread_join(pcb_get_thread_id(&pcb_list[i], j), NULL);
        }
    }

    save_log("escalonador_log_minikernel.txt");

    // Cleanup
    for (int i = 0; i < num_processes; i++) {
        pcb_free_thread_ids(&pcb_list[i]);
        pcb_destroy_mutex(&pcb_list[i]);
        pcb_destroy_cv(&pcb_list[i]);
    }
    free(pcb_list);

    return 0;
}