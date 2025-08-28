
#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "pcb.h"

#ifdef MULTI_MODE
#include "tcb.h"
typedef TCB* QueueItem;
#else
typedef PCB* QueueItem;
#endif

#define MAX_PROCESSES 100

// Forward declaration da RBTree para evitar dependência circular
struct RBTree;

typedef struct {
    QueueItem processes[MAX_PROCESSES];
    int front;
    int rear;
    int size;
    pthread_mutex_t mutex;
    
    // Árvore rubro-negra para CFS
    struct RBTree *cfs_tree;
} ReadyQueue;

void init_queue(ReadyQueue* queue);
void enqueue(ReadyQueue* queue, QueueItem item);
QueueItem dequeue(ReadyQueue* queue);
QueueItem peek(ReadyQueue* queue);
int is_empty(ReadyQueue* queue);

//funcoes para o priority
QueueItem remove_highest_priority(ReadyQueue* queue);
QueueItem peek_highest_priority(ReadyQueue* queue);

// Funções para CFS
void cfs_enqueue(ReadyQueue* queue, QueueItem item);
QueueItem cfs_dequeue(ReadyQueue* queue);
QueueItem cfs_peek(ReadyQueue* queue);
int cfs_is_empty(ReadyQueue* queue);
long long cfs_get_min_vruntime(ReadyQueue* queue);
int cfs_size(ReadyQueue* queue);

#endif 