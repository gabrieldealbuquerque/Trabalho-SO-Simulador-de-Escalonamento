#ifndef PCB_H
#define PCB_H

#include <pthread.h>

typedef enum {
    READY,
    RUNNING,
    FINISHED
} ProcessState;

typedef struct {
    int pid;
    int process_len;
    int remaining_time;
    int priority;
    int num_threads;
    int start_time;
    ProcessState state;
    int logged_finish; // flag pra evitar duplicatas no log
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    pthread_t *thread_ids;
} PCB;

// getters
int pcb_get_pid(PCB* pcb);
int pcb_get_process_len(PCB* pcb);
int pcb_get_remaining_time(PCB* pcb);
int pcb_get_priority(PCB* pcb);
int pcb_get_num_threads(PCB* pcb);
int pcb_get_start_time(PCB* pcb);
ProcessState pcb_get_state(PCB* pcb);
int pcb_get_logged_finish(PCB* pcb);
pthread_mutex_t* pcb_get_mutex(PCB* pcb);
pthread_cond_t* pcb_get_cv(PCB* pcb);
pthread_t* pcb_get_thread_ids(PCB* pcb);
pthread_t pcb_get_thread_id(PCB* pcb, int index);

// setters
void pcb_set_pid(PCB* pcb, int pid);
void pcb_set_process_len(PCB* pcb, int process_len);
void pcb_set_remaining_time(PCB* pcb, int remaining_time);
void pcb_set_priority(PCB* pcb, int priority);
void pcb_set_num_threads(PCB* pcb, int num_threads);
void pcb_set_start_time(PCB* pcb, int start_time);
void pcb_set_state(PCB* pcb, ProcessState state);
void pcb_set_logged_finish(PCB* pcb, int logged_finish);
void pcb_set_thread_ids(PCB* pcb, pthread_t* thread_ids);

// utils
void pcb_decrease_remaining_time(PCB* pcb, int amount);
void pcb_init_mutex(PCB* pcb);
void pcb_init_cv(PCB* pcb);
void pcb_destroy_mutex(PCB* pcb);
void pcb_destroy_cv(PCB* pcb);
void pcb_lock_mutex(PCB* pcb);
void pcb_unlock_mutex(PCB* pcb);
void pcb_wait_cv(PCB* pcb);
void pcb_broadcast_cv(PCB* pcb);
void pcb_free_thread_ids(PCB* pcb);

#endif 