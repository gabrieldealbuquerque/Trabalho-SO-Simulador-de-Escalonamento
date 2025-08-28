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
    
    // Campos para CFS (Completely Fair Scheduler)
    long long vruntime;        // Virtual runtime para CFS
    long long exec_start;      // Início da última execução
    long long exec_time;       // Tempo total de execução
    int nice_value;            // Valor nice (padrão 0)
    double weight;             // Peso calculado a partir do nice
    
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

// CFS getters
long long pcb_get_vruntime(PCB* pcb);
long long pcb_get_exec_start(PCB* pcb);
long long pcb_get_exec_time(PCB* pcb);
int pcb_get_nice_value(PCB* pcb);
double pcb_get_weight(PCB* pcb);

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

// CFS setters
void pcb_set_vruntime(PCB* pcb, long long vruntime);
void pcb_set_exec_start(PCB* pcb, long long exec_start);
void pcb_set_exec_time(PCB* pcb, long long exec_time);
void pcb_set_nice_value(PCB* pcb, int nice_value);
void pcb_set_weight(PCB* pcb, double weight);

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

// CFS utils
void pcb_init_cfs_fields(PCB* pcb);
void pcb_update_vruntime(PCB* pcb, long long delta_exec);
double pcb_calc_weight_from_nice(int nice_value);
long long pcb_calc_delta_fair(PCB* pcb, long long delta_exec);

#endif 