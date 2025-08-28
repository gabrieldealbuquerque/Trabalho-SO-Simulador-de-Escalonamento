#include "queue.h"
#include "config.h"
#include "tcb.h"
#include "rbtree.h"
#include <stdlib.h>

void init_queue(ReadyQueue* queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    // Para CFS com fila ordenada simples
    queue->cfs_tree = NULL;
}

void enqueue(ReadyQueue* queue, QueueItem item) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size < MAX_PROCESSES) {
        queue->processes[queue->rear] = item;
        queue->rear = (queue->rear + 1) % MAX_PROCESSES;
        queue->size++;
    }
    pthread_mutex_unlock(&queue->mutex);
}

void enqueue_front(ReadyQueue* queue, QueueItem item) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size < MAX_PROCESSES) {
        queue->front = (queue->front - 1 + MAX_PROCESSES) % MAX_PROCESSES;
        queue->processes[queue->front] = item;
        queue->size++;
        queue->rear = (queue->front + queue->size) % MAX_PROCESSES;
    }
    pthread_mutex_unlock(&queue->mutex);
}

QueueItem dequeue(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    QueueItem item = queue->processes[queue->front];
    queue->front = (queue->front + 1) % MAX_PROCESSES;
    queue->size--;
    pthread_mutex_unlock(&queue->mutex);
    return item;
}

QueueItem peek(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    QueueItem item = queue->processes[queue->front];
    pthread_mutex_unlock(&queue->mutex);
    return item;
}

// retorna item de maior prio
QueueItem peek_highest_priority(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    QueueItem best_item = NULL;
    int best_priority = 999;
    
    for (int i = 0; i < queue->size; i++) {
        int idx = (queue->front + i) % MAX_PROCESSES;
        QueueItem item = queue->processes[idx];
        int priority;
        
        if (is_multiprocessor_mode()) {
            priority = pcb_get_priority(((TCB*)item)->pcb);
        } else {
            priority = pcb_get_priority((PCB*)item);
        }
        
        if (priority < best_priority) {
            best_priority = priority;
            best_item = item;
        }
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return best_item;
}

int is_empty(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    int empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->mutex);
    return empty;
}

QueueItem remove_highest_priority(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // acha item com maior prio
    int best_idx = -1;
    int best_priority = 999;
    
    for (int i = 0; i < queue->size; i++) {
        int idx = (queue->front + i) % MAX_PROCESSES;
        QueueItem item = queue->processes[idx];
        int priority;
        
        if (is_multiprocessor_mode()) {
            priority = pcb_get_priority(((TCB*)item)->pcb);
        } else {
            priority = pcb_get_priority((PCB*)item);
        }
        
        if (priority < best_priority) {
            best_priority = priority;
            best_idx = idx;
        }
    }
    
    QueueItem result = queue->processes[best_idx];
    
    // marca como nulo na fila
    queue->processes[best_idx] = NULL;
    
    // reconstroi fila sem o item removido
    QueueItem temp[MAX_PROCESSES];
    int temp_count = 0;
    
    for (int i = 0; i < queue->size; i++) {
        int idx = (queue->front + i) % MAX_PROCESSES;
        if (queue->processes[idx] != NULL) {
            temp[temp_count++] = queue->processes[idx];
        }
    }
    
    queue->front = 0;
    queue->rear = temp_count;
    queue->size = temp_count;
    
    for (int i = 0; i < temp_count; i++) {
        queue->processes[i] = temp[i];
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return result;
}

// Implementações CFS com fila ordenada por vruntime

static long long item_vruntime(QueueItem item) {
    if (is_multiprocessor_mode()) {
        return pcb_get_vruntime(((TCB*)item)->pcb);
    } else {
        return pcb_get_vruntime((PCB*)item);
    }
}

void cfs_enqueue(ReadyQueue* queue, QueueItem item) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size >= MAX_PROCESSES) {
        pthread_mutex_unlock(&queue->mutex);
        return;
    }
    long long vr = item_vruntime(item);
    int pos = 0;
    for (int i = 0; i < queue->size; i++) {
        if (item_vruntime(queue->processes[i]) <= vr) pos = i + 1; else break;
    }
    for (int i = queue->size; i > pos; i--) queue->processes[i] = queue->processes[i - 1];
    queue->processes[pos] = item;
    queue->size++;
    queue->front = 0;
    queue->rear = queue->size;
    pthread_mutex_unlock(&queue->mutex);
}

QueueItem cfs_dequeue(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0) { pthread_mutex_unlock(&queue->mutex); return NULL; }
    QueueItem it = queue->processes[0];
    for (int i = 1; i < queue->size; i++) queue->processes[i - 1] = queue->processes[i];
    queue->size--;
    queue->front = 0;
    queue->rear = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    return it;
}

QueueItem cfs_peek(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    QueueItem it = (queue->size > 0) ? queue->processes[0] : NULL;
    pthread_mutex_unlock(&queue->mutex);
    return it;
}

int cfs_is_empty(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    int empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->mutex);
    return empty;
}

long long cfs_get_min_vruntime(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    long long v = (queue->size > 0) ? item_vruntime(queue->processes[0]) : 0;
    pthread_mutex_unlock(&queue->mutex);
    return v;
}

int cfs_size(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    int s = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    return s;
}