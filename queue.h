
#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "pcb.h"

#ifdef MULTI
#include "tcb.h"
typedef TCB* QueueItem;
#else
typedef PCB* QueueItem;
#endif

#define MAX_PROCESSES 100

typedef struct {
    QueueItem processes[MAX_PROCESSES];
    int front;
    int rear;
    int size;
    pthread_mutex_t mutex;
} ReadyQueue;

void init_queue(ReadyQueue* queue);
void enqueue(ReadyQueue* queue, QueueItem item);
QueueItem dequeue(ReadyQueue* queue);
// Enfileira no início (alta prioridade imediata)
void enqueue_front(ReadyQueue* queue, QueueItem item);
QueueItem peek(ReadyQueue* queue);
int is_empty(ReadyQueue* queue);

// For priority: find and remove highest priority
QueueItem remove_highest_priority(ReadyQueue* queue);
QueueItem peek_highest_priority(ReadyQueue* queue);

#endif // QUEUE_H 