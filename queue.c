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
    // Inicializa a árvore rubro-negra para CFS
    queue->cfs_tree = (RBTree*)malloc(sizeof(RBTree));
    if (queue->cfs_tree) {
        rb_init(queue->cfs_tree);
    }
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

// Implementações CFS usando árvore rubro-negra (ordenada por vruntime)

static long long item_vruntime(QueueItem item) {
    if (is_multiprocessor_mode()) {
        return pcb_get_vruntime(((TCB*)item)->pcb);
    } else {
        return pcb_get_vruntime((PCB*)item);
    }
}

void cfs_enqueue(ReadyQueue* queue, QueueItem item) {
    if (!queue->cfs_tree) {
        // tenta inicializar sob demanda
        queue->cfs_tree = (RBTree*)malloc(sizeof(RBTree));
        if (queue->cfs_tree) rb_init(queue->cfs_tree);
    }
    if (!queue->cfs_tree) return;
    long long vr = item_vruntime(item);
    rb_insert(queue->cfs_tree, (RBItem)item, vr);
}

QueueItem cfs_dequeue(ReadyQueue* queue) {
    if (!queue->cfs_tree) return NULL;
    return (QueueItem)rb_extract_min(queue->cfs_tree);
}

QueueItem cfs_peek(ReadyQueue* queue) {
    if (!queue->cfs_tree) return NULL;
    return (QueueItem)rb_peek_min(queue->cfs_tree);
}

int cfs_is_empty(ReadyQueue* queue) {
    if (!queue->cfs_tree) return 1;
    return rb_is_empty(queue->cfs_tree);
}

long long cfs_get_min_vruntime(ReadyQueue* queue) {
    if (!queue->cfs_tree) return 0;
    return rb_get_min_vruntime(queue->cfs_tree);
}

int cfs_size(ReadyQueue* queue) {
    if (!queue->cfs_tree) return 0;
    return rb_size(queue->cfs_tree);
}

void destroy_queue(ReadyQueue* queue) {
    if (queue->cfs_tree) {
        // Se houver itens restantes na CFS tree (ex.: TCBs não processados), drena e libera
        if (is_multiprocessor_mode()) {
            while (!rb_is_empty(queue->cfs_tree)) {
                RBItem it = rb_extract_min(queue->cfs_tree);
                if (it) {
                    // Em MULTI, os itens são TCB*
                    free((TCB*)it);
                }
            }
        }
        rb_destroy(queue->cfs_tree);
        free(queue->cfs_tree);
        queue->cfs_tree = NULL;
    }
    pthread_mutex_destroy(&queue->mutex);
}