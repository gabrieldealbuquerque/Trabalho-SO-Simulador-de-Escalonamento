
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcb.h"
#include "queue.h"

extern int scheduler_type;
extern int quantum;
extern PCB* pcb_list;
extern int num_processes;
extern ReadyQueue ready_queue;
extern pthread_cond_t scheduler_cv;
extern pthread_mutex_t scheduler_mutex;
extern int generator_done;

void init_scheduler();
void* scheduler_func(void* arg);
void* thread_func(void* arg);
long long current_time_ms();
char* get_policy_string();

#endif // SCHEDULER_H 