// LLRBT (Left-Leaning Red-Black Tree) simplificada para CFS
#include "rbtree.h"
#include <stdlib.h>

static int is_red(RBNode *x) {
    return x != NULL && x->color == RED;
}

static RBNode* new_node(RBItem item, long long vruntime) {
    RBNode *n = (RBNode*)malloc(sizeof(RBNode));
    if (!n) return NULL;
    n->item = item;
    n->vruntime = vruntime;
    n->color = RED; // novo nó sempre vermelho
    n->left = n->right = n->parent = NULL;
    return n;
}

static RBNode* rotate_left(RBNode *h) {
    RBNode *x = h->right;
    h->right = x->left;
    x->left = h;
    x->color = h->color;
    h->color = RED;
    return x;
}

static RBNode* rotate_right(RBNode *h) {
    RBNode *x = h->left;
    h->left = x->right;
    x->right = h;
    x->color = h->color;
    h->color = RED;
    return x;
}

static void color_flip(RBNode *h) {
    h->color = (h->color == RED ? BLACK : RED);
    if (h->left)  h->left->color  = (h->left->color  == RED ? BLACK : RED);
    if (h->right) h->right->color = (h->right->color == RED ? BLACK : RED);
}

static int less_key(RBItem a_item, long long a_vr, RBItem b_item, long long b_vr) {
    if (a_vr < b_vr) return 1;
    if (a_vr > b_vr) return 0;
    // empate: usa endereço do item como critério secundário
    return (a_item < b_item);
}

static RBNode* fix_up(RBNode *h) {
    if (is_red(h->right) && !is_red(h->left))      h = rotate_left(h);
    if (is_red(h->left)  && is_red(h->left->left)) h = rotate_right(h);
    if (is_red(h->left)  && is_red(h->right))      color_flip(h);
    return h;
}

static RBNode* move_red_left(RBNode *h) {
    color_flip(h);
    if (h->right && is_red(h->right->left)) {
        h->right = rotate_right(h->right);
        h = rotate_left(h);
        color_flip(h);
    }
    return h;
}

static RBNode* move_red_right(RBNode *h) {
    color_flip(h);
    if (h->left && is_red(h->left->left)) {
        h = rotate_right(h);
        color_flip(h);
    }
    return h;
}

static RBNode* put(RBNode *h, RBItem item, long long vruntime) {
    if (h == NULL) return new_node(item, vruntime);
    if (less_key(item, vruntime, h->item, h->vruntime)) {
        h->left = put(h->left, item, vruntime);
    } else {
        h->right = put(h->right, item, vruntime);
    }
    return fix_up(h);
}

static RBNode* min_node(RBNode *h) {
    while (h && h->left) h = h->left;
    return h;
}

static RBNode* delete_min(RBNode *h) {
    if (h->left == NULL) {
        free(h);
        return NULL;
    }
    if (!is_red(h->left) && !is_red(h->left->left)) {
        h = move_red_left(h);
    }
    h->left = delete_min(h->left);
    return fix_up(h);
}

static void destroy_recursive(RBNode *node) {
    if (!node) return;
    destroy_recursive(node->left);
    destroy_recursive(node->right);
    free(node);
}

static void update_minimum(RBTree *tree) {
    tree->minimum = min_node(tree->root);
}

void rb_init(RBTree *tree) {
    tree->root = NULL;
    tree->minimum = NULL;
    tree->size = 0;
    pthread_mutex_init(&tree->mutex, NULL);
}

void rb_destroy(RBTree *tree) {
    pthread_mutex_lock(&tree->mutex);
    destroy_recursive(tree->root);
    tree->root = NULL;
    tree->minimum = NULL;
    tree->size = 0;
    pthread_mutex_unlock(&tree->mutex);
    pthread_mutex_destroy(&tree->mutex);
}

void rb_insert(RBTree *tree, RBItem item, long long vruntime) {
    pthread_mutex_lock(&tree->mutex);
    tree->root = put(tree->root, item, vruntime);
    if (tree->root) tree->root->color = BLACK;
    tree->size++;
    update_minimum(tree);
    pthread_mutex_unlock(&tree->mutex);
}

RBItem rb_extract_min(RBTree *tree) {
    pthread_mutex_lock(&tree->mutex);
    
    if (tree->root == NULL) {
        pthread_mutex_unlock(&tree->mutex);
        return NULL;
    }
    // pega item do mínimo
    RBNode *mn = min_node(tree->root);
    RBItem result = mn ? mn->item : NULL;

    // garante que a raiz não viole invariantes para deleteMin
    if (!is_red(tree->root->left) && !is_red(tree->root->right)) {
        tree->root->color = RED;
    }
    tree->root = delete_min(tree->root);
    if (tree->root) tree->root->color = BLACK;
    tree->size--;
    update_minimum(tree);
    
    pthread_mutex_unlock(&tree->mutex);
    return result;
}

RBItem rb_peek_min(RBTree *tree) {
    pthread_mutex_lock(&tree->mutex);
    
    if (tree->minimum == NULL) {
        pthread_mutex_unlock(&tree->mutex);
        return NULL;
    }
    
    RBItem result = tree->minimum->item;
    pthread_mutex_unlock(&tree->mutex);
    return result;
}

int rb_is_empty(RBTree *tree) {
    pthread_mutex_lock(&tree->mutex);
    int empty = (tree->size == 0);
    pthread_mutex_unlock(&tree->mutex);
    return empty;
}

int rb_size(RBTree *tree) {
    pthread_mutex_lock(&tree->mutex);
    int size = tree->size;
    pthread_mutex_unlock(&tree->mutex);
    return size;
}

long long rb_get_min_vruntime(RBTree *tree) {
    pthread_mutex_lock(&tree->mutex);
    
    if (tree->minimum == NULL) {
        pthread_mutex_unlock(&tree->mutex);
        return 0;
    }
    
    long long min_vruntime = tree->minimum->vruntime;
    pthread_mutex_unlock(&tree->mutex);
    return min_vruntime;
}

RBNode* rb_find_node(RBTree *tree, RBItem item) {
    (void)tree; (void)item;
    return NULL; // não utilizado na versão atual
}

void rb_update_vruntime(RBTree *tree, RBItem item, long long new_vruntime) {
    (void)tree; (void)item; (void)new_vruntime;
    // não utilizado – reinserção é feita externamente pela fila CFS
}

void rb_remove_node(RBTree *tree, RBNode *node) {
    (void)tree; (void)node; // não utilizado na versão atual
}
