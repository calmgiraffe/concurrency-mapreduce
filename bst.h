#pragma once

#include <stdatomic.h>

typedef struct node_t {
    char *key;
    atomic_int value;
    struct node_t *left;
    struct node_t *right;
} Node;


void BST_insert(Node **head, 
        Node *newnode, 
        int (*compar)(const void *, const void *));

void BST_print_inorder(Node *head);

void BST_free(Node *head);