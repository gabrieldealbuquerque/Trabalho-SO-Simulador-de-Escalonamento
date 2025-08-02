
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
    int logged_finish; // Flag to avoid duplicate finish logs
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    pthread_t *thread_ids;
} PCB;

#endif // PCB_H 