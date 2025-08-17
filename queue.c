
#include "queue.h"
#include "config.h"
#include "tcb.h"
#include <stdlib.h>

void init_queue(ReadyQueue* queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
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
        // Move front backward circularly
        queue->front = (queue->front - 1 + MAX_PROCESSES) % MAX_PROCESSES;
        queue->processes[queue->front] = item;
        queue->size++;
        // rear stays the same logical position (front + size modulo)
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

// Peek at highest priority item without removing
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

// Remove highest priority item (lowest priority number)
QueueItem remove_highest_priority(ReadyQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // Find the item with highest priority (lowest number)
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
    
    // Get the result
    QueueItem result = queue->processes[best_idx];
    
    // Mark as NULL and compact the array by moving items forward
    queue->processes[best_idx] = NULL;
    
    // Simple approach: rebuild the queue without the removed item
    QueueItem temp[MAX_PROCESSES];
    int temp_count = 0;
    
    for (int i = 0; i < queue->size; i++) {
        int idx = (queue->front + i) % MAX_PROCESSES;
        if (queue->processes[idx] != NULL) {
            temp[temp_count++] = queue->processes[idx];
        }
    }
    
    // Rebuild queue
    queue->front = 0;
    queue->rear = temp_count;
    queue->size = temp_count;
    
    for (int i = 0; i < temp_count; i++) {
        queue->processes[i] = temp[i];
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return result;
} 