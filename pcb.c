#include "pcb.h"
#include <stdlib.h>

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
