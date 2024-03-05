#include <stdatomic.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "bst.h"

Node *head = NULL;

static bool compare_and_swap(Node **addr, Node *oldval, Node *newval) {
    unsigned char ret;
    __asm__ volatile (
        // Implicitly compare the original value at *addr (%1) with the value in
        // oldval (%3), which is stored in %eax. If they are equal, it swaps the 
        // value at %1 with %2 (newval). If not, it leaves %1 unchanged.
        // sete sets ret (%0) to 0x1 is the swap was successful.
        "lock cmpxchg %2, %1\n\t"
        "sete %0"
        : "=q" (ret), "+m" (*addr)
        : "r" (newval), "a" (oldval)
        : "memory", "cc"
    );
    return ret;
}

void BST_insert(Node **head, 
        Node *newnode, 
        int (*compar)(const void *, const void *)) {

    if (*head == NULL) {
        if (!compare_and_swap(head, NULL, newnode))
            // head must have changed in the middle of CAS and logically has to be a node
            BST_insert(head, newnode, compar);
    }
    // head != NULL
    else if (compar(newnode->key, (*head)->key) == -1) {
        if (!compare_and_swap(&(*head)->left, NULL, newnode))
            // head->right must have changed in the middle of CAS and logically has to be a node
            BST_insert(&(*head)->left, newnode, compar);
    } 
    else if (compar(newnode->key, (*head)->key) == 1) {
        if (!compare_and_swap(&(*head)->right, NULL, newnode))
            BST_insert(&(*head)->right, newnode, compar);
    } 
    else {
        // Key already exists
        // 
        atomic_fetch_add(&(*head)->value, newnode->value);
    }
}

void BST_print_inorder(Node *head) {
    if (head == NULL) {
        return;
    }
    BST_print_inorder(head->left);
    printf("%s, %d\n", head->key, head->value);
    BST_print_inorder(head->right);
}

void BST_free(Node *head) {
    if (head == NULL) {
        return;
    }
    // Free the left and right subtrees
    BST_free(head->left);
    BST_free(head->right);

    // Free the key and the node itself
    free(head->key);
    free(head);
}
