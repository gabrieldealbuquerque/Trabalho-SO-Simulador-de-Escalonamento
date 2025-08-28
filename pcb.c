#include "pcb.h"
#include <stdlib.h>
#include <math.h>

// getters
int pcb_get_pid(PCB* pcb) {
    return pcb->pid;
}

int pcb_get_process_len(PCB* pcb) {
    return pcb->process_len;
}

int pcb_get_remaining_time(PCB* pcb) {
    return pcb->remaining_time;
}

int pcb_get_priority(PCB* pcb) {
    return pcb->priority;
}

int pcb_get_num_threads(PCB* pcb) {
    return pcb->num_threads;
}

int pcb_get_start_time(PCB* pcb) {
    return pcb->start_time;
}

ProcessState pcb_get_state(PCB* pcb) {
    return pcb->state;
}

int pcb_get_logged_finish(PCB* pcb) {
    return pcb->logged_finish;
}

pthread_mutex_t* pcb_get_mutex(PCB* pcb) {
    return &pcb->mutex;
}

pthread_cond_t* pcb_get_cv(PCB* pcb) {
    return &pcb->cv;
}

pthread_t* pcb_get_thread_ids(PCB* pcb) {
    return pcb->thread_ids;
}

pthread_t pcb_get_thread_id(PCB* pcb, int index) {
    return pcb->thread_ids[index];
}

// setters
void pcb_set_pid(PCB* pcb, int pid) {
    pcb->pid = pid;
}

void pcb_set_process_len(PCB* pcb, int process_len) {
    pcb->process_len = process_len;
}

void pcb_set_remaining_time(PCB* pcb, int remaining_time) {
    pcb->remaining_time = remaining_time;
}

void pcb_set_priority(PCB* pcb, int priority) {
    pcb->priority = priority;
}

void pcb_set_num_threads(PCB* pcb, int num_threads) {
    pcb->num_threads = num_threads;
}

void pcb_set_start_time(PCB* pcb, int start_time) {
    pcb->start_time = start_time;
}

void pcb_set_state(PCB* pcb, ProcessState state) {
    pcb->state = state;
}

void pcb_set_logged_finish(PCB* pcb, int logged_finish) {
    pcb->logged_finish = logged_finish;
}

void pcb_set_thread_ids(PCB* pcb, pthread_t* thread_ids) {
    pcb->thread_ids = thread_ids;
}

// utils
void pcb_decrease_remaining_time(PCB* pcb, int amount) {
    pcb->remaining_time -= amount;
}

void pcb_init_mutex(PCB* pcb) {
    pthread_mutex_init(&pcb->mutex, NULL);
}

void pcb_init_cv(PCB* pcb) {
    pthread_cond_init(&pcb->cv, NULL);
}

void pcb_destroy_mutex(PCB* pcb) {
    pthread_mutex_destroy(&pcb->mutex);
}

void pcb_destroy_cv(PCB* pcb) {
    pthread_cond_destroy(&pcb->cv);
}

void pcb_lock_mutex(PCB* pcb) {
    pthread_mutex_lock(&pcb->mutex);
}

void pcb_unlock_mutex(PCB* pcb) {
    pthread_mutex_unlock(&pcb->mutex);
}

void pcb_wait_cv(PCB* pcb) {
    pthread_cond_wait(&pcb->cv, &pcb->mutex);
}

void pcb_broadcast_cv(PCB* pcb) {
    pthread_cond_broadcast(&pcb->cv);
}

void pcb_free_thread_ids(PCB* pcb) {
    free(pcb->thread_ids);
}

// CFS getters
long long pcb_get_vruntime(PCB* pcb) {
    return pcb->vruntime;
}

long long pcb_get_exec_start(PCB* pcb) {
    return pcb->exec_start;
}

long long pcb_get_exec_time(PCB* pcb) {
    return pcb->exec_time;
}

int pcb_get_nice_value(PCB* pcb) {
    return pcb->nice_value;
}

double pcb_get_weight(PCB* pcb) {
    return pcb->weight;
}

// CFS setters
void pcb_set_vruntime(PCB* pcb, long long vruntime) {
    pcb->vruntime = vruntime;
}

void pcb_set_exec_start(PCB* pcb, long long exec_start) {
    pcb->exec_start = exec_start;
}

void pcb_set_exec_time(PCB* pcb, long long exec_time) {
    pcb->exec_time = exec_time;
}

void pcb_set_nice_value(PCB* pcb, int nice_value) {
    pcb->nice_value = nice_value;
}

void pcb_set_weight(PCB* pcb, double weight) {
    pcb->weight = weight;
}

// CFS utils

// Tabela de pesos baseada no nice value (simplificada)
// Nice -20 = peso 88761, Nice 0 = peso 1024, Nice +19 = peso 15
static const double nice_to_weight[] = {
    /* -20 */ 88761.0, 71755.0, 56483.0, 46273.0, 36291.0,
    /* -15 */ 29154.0, 23254.0, 18705.0, 14949.0, 11916.0,
    /* -10 */ 9548.0, 7620.0, 6100.0, 4904.0, 3906.0,
    /*  -5 */ 3121.0, 2501.0, 1991.0, 1586.0, 1277.0,
    /*   0 */ 1024.0, 820.0, 655.0, 526.0, 423.0,
    /*   5 */ 335.0, 272.0, 215.0, 172.0, 137.0,
    /*  10 */ 110.0, 87.0, 70.0, 56.0, 45.0,
    /*  15 */ 36.0, 29.0, 23.0, 18.0, 15.0
};

void pcb_init_cfs_fields(PCB* pcb) {
    pcb->vruntime = 0;
    pcb->exec_start = 0;
    pcb->exec_time = 0;
    // Converte prioridade (1-5) para nice value (-4 a +4)
    // Prioridade 1 (alta) -> nice -4, Prioridade 5 (baixa) -> nice +4
    int nice = (pcb->priority - 3);  // Prioridade 3 -> nice 0
    if (nice < -4) nice = -4;
    if (nice > 4) nice = 4;
    pcb->nice_value = nice;
    pcb->weight = pcb_calc_weight_from_nice(nice);
}

double pcb_calc_weight_from_nice(int nice_value) {
    // Limita nice entre -20 e +19
    if (nice_value < -20) nice_value = -20;
    if (nice_value > 19) nice_value = 19;
    
    return nice_to_weight[nice_value + 20]; // Offset para indexar o array
}

void pcb_update_vruntime(PCB* pcb, long long delta_exec) {
    // vruntime += delta_exec * NICE_0_WEIGHT / weight
    // NICE_0_WEIGHT = 1024
    pcb->vruntime += (long long)((double)delta_exec * 1024.0 / pcb->weight);
}

long long pcb_calc_delta_fair(PCB* pcb, long long delta_exec) {
    // Calcula quanto o vruntime deve avançar baseado na execução real
    return (long long)((double)delta_exec * 1024.0 / pcb->weight);
}
