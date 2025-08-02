#include "scheduler.h"
#include "pcb.h"
#include "tcb.h"
#include "queue.h"
#include "log.h"
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
    int idx = 0;
    while (idx < num_processes) {
        PCB* pcb = &pcb_list[idx];
        long long now = current_time_ms() - start_time;
        if (now >= pcb->start_time) {
#ifdef MULTI
            // In multi, enqueue each thread (TCB) individually
            for (int j = 0; j < pcb->num_threads; j++) {
                TCB* tcb = malloc(sizeof(TCB));
                tcb->pcb = pcb;
                tcb->thread_index = j;
                pthread_create(&pcb->thread_ids[j], NULL, thread_func, tcb);
                enqueue(&ready_queue, (QueueItem)tcb);
            }
#else
            // In mono, create threads but enqueue the process (PCB)
            for (int j = 0; j < pcb->num_threads; j++) {
                TCB* tcb = malloc(sizeof(TCB));
                tcb->pcb = pcb;
                tcb->thread_index = j;
                pthread_create(&pcb->thread_ids[j], NULL, thread_func, tcb);
            }
            enqueue(&ready_queue, (QueueItem)pcb);
#endif
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
    return pcb_a->start_time - pcb_b->start_time;
}

// Main function
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE* input = fopen(argv[1], "r");
    if (!input) {
        fprintf(stderr, "Failed to open input file\n");
        return 1;
    }

    fscanf(input, "%d", &num_processes);
    pcb_list = malloc(num_processes * sizeof(PCB));

    for (int i = 0; i < num_processes; i++) {
        PCB* pcb = &pcb_list[i];
        fscanf(input, "%d %d %d %d", &pcb->process_len, &pcb->priority, &pcb->num_threads, &pcb->start_time);
        pcb->pid = i + 1; // PIDs start from 1
        // Process gets full duration, threads share it
        pcb->remaining_time = pcb->process_len;
        pcb->state = READY;
        pcb->logged_finish = 0; // Initialize finish log flag
        pthread_mutex_init(&pcb->mutex, NULL);
        pthread_cond_init(&pcb->cv, NULL);
        pcb->thread_ids = malloc(pcb->num_threads * sizeof(pthread_t));
    }

    fscanf(input, "%d", &scheduler_type);
    // Assuming quantum is fixed or read if needed; spec seems to have it in input?
    quantum = 500; // Default quantum 500ms

    fclose(input);

    init_log();
    init_scheduler();

    #ifdef MULTI
    #define NUM_CPUS 2
    #else
    #define NUM_CPUS 1
    #endif
    pthread_t scheduler_threads[NUM_CPUS];
    for (int i = 0; i < NUM_CPUS; i++) {
        pthread_create(&scheduler_threads[i], NULL, scheduler_func, (void*)(intptr_t)i);
    }

    qsort(pcb_list, num_processes, sizeof(PCB), compare_start_time);

    pthread_t generator_thread;
    pthread_create(&generator_thread, NULL, generator_func, NULL);

    // Wait for all processes to finish
    pthread_join(generator_thread, NULL);
    for (int i = 0; i < NUM_CPUS; i++) {
        pthread_join(scheduler_threads[i], NULL);
    }
    for (int i = 0; i < num_processes; i++) {
        for (int j = 0; j < pcb_list[i].num_threads; j++) {
            pthread_join(pcb_list[i].thread_ids[j], NULL);
        }
    }

    save_log("escalonador_log_minikernel.txt");

    // Cleanup
    for (int i = 0; i < num_processes; i++) {
        free(pcb_list[i].thread_ids);
        pthread_mutex_destroy(&pcb_list[i].mutex);
        pthread_cond_destroy(&pcb_list[i].cv);
    }
    free(pcb_list);

    return 0;
}