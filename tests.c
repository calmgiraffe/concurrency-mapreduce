/*
Unit tests for the various components of the framework.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "sort.h"
#include "bst.h"
#include "framework.h"

#define DIR test-inputs
#define TO_STRING(x) #x 
#define CONCAT(path, file) TO_STRING(path) "/" file

extern Node *head;
extern void (*Map)(char *file_name);
extern void (*Reduce)(int partition_number);

/* 
Tests the return value of get_char when you keep calling the function past EOF
*/
void test_get_char_returnval() {
    char *cmpstr = "potato";
    char *filename = CONCAT(DIR, "potato.txt");
    char ch;

    size_t filesize = get_size(filename);
    assert(filesize != -1);
    FILE *f = fopen(filename, "rb");

    // Do chars match?
    for (int i = 0; i < filesize; i++) {
        int ret = get_char(&ch, f);
        assert(cmpstr[i] == ch && ret == 1);
    }
    // Check for 0 ret val an arbitrary number of times
    for (int i = 0; i < 3; i++) {
        assert(get_char(&ch, f) == 0);
    }
    fclose(f);
    printf("%s: pass\n", __func__);
}

/*
Correctness test of get_size() and get_char()
*/
void test_get_char() {
    int read;
    int64_t filesize, readBytes = 0;
    char *files[3], *filename;

    files[0] = CONCAT(DIR, "8kb.txt");
    files[1] = CONCAT(DIR, "0b.txt");
    files[2] = CONCAT(DIR, "4kb.txt");

    char ch;
    for (int i = 0; i < 3; i++) {
        filename = files[i];
        filesize = get_size(filename);

        FILE *f = fopen(filename, "rb");
        if (f == NULL) {
            fprintf(stderr, "%s: fopen err\n", __func__);
            exit(EXIT_FAILURE);
        }
        while ((read = get_char(&ch, f)) != 0) {
            readBytes++;
        }
        fclose(f);

        assert(readBytes == filesize);
        readBytes = 0;
    }
    printf("%s: pass\n", __func__);
}


void test_get_word() {
    char buffer[MAXWORD];
    char *filename = CONCAT(DIR, "123.txt");

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "%s: fopen err\n", __func__);
        exit(EXIT_FAILURE);
    }

    // Check the length of each word in the file.
    while (1) {
        int ret = get_word(buffer, MAXWORD, f);

        assert(ret != -1);
        if (ret == 0) break;
        assert(ret == strlen(buffer));
    }
    fclose(f);
    printf("%s: pass\n", __func__);
}

void test_ascending_ord_str_compare() {
    char buffer1[MAXWORD];
    char buffer2[MAXWORD];
    char *filename = CONCAT(DIR, "ai.txt");

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "%s: fopen err\n", __func__);
        exit(EXIT_FAILURE);
    }

    // Check the length of each word in the file.
    while (get_word(buffer1, MAXWORD, f) > 0 && get_word(buffer2, MAXWORD, f) > 0) {
        int cmp = ascending_ord_str_compare(buffer1, buffer2);
        toLowerCase(buffer1);
        toLowerCase(buffer2);
        printf("%s, %s, %d\n", buffer1, buffer2, cmp);
    }
    fclose(f);
}

void test_bst() {
    char buffer[MAXWORD];
    char *filename = "texts/her.txt";

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "%s: fopen err\n", __func__);
        exit(EXIT_FAILURE);
    }

    // Check the length of each word in the file.
    char *key;
    Node *newnode;
    while (get_word(buffer, MAXWORD, f)) {
        key = malloc(sizeof(char) * MAXWORD);
        if (key == NULL) 
            exit(EXIT_FAILURE);
    
        strcpy(key, buffer);
        toLowerCase(key);

        newnode = malloc(sizeof(Node));
        if (newnode == NULL) 
            exit(EXIT_FAILURE);

        newnode->key = key;
        newnode->value = 1;
        newnode->left = NULL;
        newnode->right = NULL;
        
        BST_insert(&head, newnode, ascending_ord_str_compare);  
    }
    fclose(f);
    BST_print_inorder(head);
    BST_free(head);
}

void test_concurrent_bst() {
    char *strings[] = {"texts/aca.txt", "texts/birdman.txt"};
    MR_Run(2, strings, Map, 2, Reduce, 2, MR_DefaultHashPartition);
}

#ifdef UNIT_TEST
int main(int argc, char *argv[]) {
    //test_get_char_returnval();
    //test_get_char();
    //test_get_word();
    //test_ascending_ord_str_compare();
    //test_bst();
    test_concurrent_bst();
}
#endif