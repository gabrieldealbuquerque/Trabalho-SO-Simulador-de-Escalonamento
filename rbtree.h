#ifndef RBTREE_H
#define RBTREE_H

#include <pthread.h>
#include "pcb.h"

#ifdef MULTI_MODE
#include "tcb.h"
typedef TCB* RBItem;
#else
typedef PCB* RBItem;
#endif

typedef enum {
    RED = 0,
    BLACK = 1
} Color;

typedef struct RBNode {
    RBItem item;
    long long vruntime;
    Color color;
    struct RBNode *left;
    struct RBNode *right;
    struct RBNode *parent;
} RBNode;

typedef struct {
    RBNode *root;
    RBNode *minimum;
    int size;
    pthread_mutex_t mutex;
} RBTree;

// Inicialização e destruição
void rb_init(RBTree *tree);
void rb_destroy(RBTree *tree);

// Operações principais
void rb_insert(RBTree *tree, RBItem item, long long vruntime);
RBItem rb_extract_min(RBTree *tree);
RBItem rb_peek_min(RBTree *tree);
int rb_is_empty(RBTree *tree);
int rb_size(RBTree *tree);

// Operações para CFS
void rb_update_vruntime(RBTree *tree, RBItem item, long long new_vruntime);
long long rb_get_min_vruntime(RBTree *tree);

// Funções auxiliares
RBNode* rb_find_node(RBTree *tree, RBItem item);
void rb_remove_node(RBTree *tree, RBNode *node);

#endif
