#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "partition.h"

static BoundedBuffer *p;
static Partitioner hash;
static int num_p;


void MR_InitPartition(int num_mapper, 
        int num_reducer, 
        int num_partitions, 
        Partitioner partitioner) {

    num_p = num_partitions;
    hash = partitioner;

    // Memory for partitions
    p = (BoundedBuffer *) malloc(sizeof(BoundedBuffer) * num_p);
    if (!p) {
        perror("Failed to allocate memory for BoundedBuffers");
        exit(EXIT_FAILURE);
    }

    // Memory for partition internals
    for (int i = 0; i < num_p; i++) {
        if (sem_init(&p[i].full, 0, 0) != 0 || 
            sem_init(&p[i].empty, 0, BOUNDED_BUF_SIZE) != 0 ||
            sem_init(&p[i].mutex, 0, 1) != 0) {

            // Handle semaphore initialization failure
            perror("Failed to initialize semaphores");
            exit(EXIT_FAILURE);
        }

        p[i].buffer = (KeyValue *) malloc(sizeof(KeyValue) * BOUNDED_BUF_SIZE);
        if (!p[i].buffer) {
            perror("Failed to allocate memory for buffer");
            exit(EXIT_FAILURE);
        }

        p[i].fill = 0;
        p[i].use = 0;
    }
}

/*
Takes key/value pairs from the mappers and stores them in a way that reducers 
can access them. Stores items via producer-consumer method.
*/
// TODO: Make the input args more general with *void, and have an argument to pass in a copy function
int MR_Emit(char *key, char *value) {
    // TODO: experiment with consumers that sleep vs. spinwait when no
    // data is in its respective queue.
    // NOTE: The sleeping approach allows for more reducers than available cores
    // but requires scheduling, sleeps & wakeup

    // Copy over the key-val pair into a new block of memory (new entry)
    char *key_cpy = (char*) malloc(sizeof(char) * (strlen(key) + 1));
    char *val_cpy = (char*) malloc(sizeof(char) * (strlen(value) + 1));

    if (!key_cpy || !val_cpy) {
        perror("Failed to allocate memory for new BoundedBuffers entry");
        exit(EXIT_FAILURE);
    }
    strcpy(key_cpy, key);
    strcpy(val_cpy, value);

    // Hash the key to get the partition index
    // Insert the newly allocated entry
    BoundedBuffer *bb = &p[hash(key, num_p)];
    int i;
    sem_wait(&bb->empty);
    sem_wait(&bb->mutex);

    i = bb->fill;
    bb->buffer[i].key = key_cpy;
    bb->buffer[i].value = val_cpy;
    bb->fill = (i + 1) % BOUNDED_BUF_SIZE;

    sem_post(&bb->mutex);
    sem_post(&bb->full);

    return 0; // success
}

/*
Copy over the key,value pair currently pointed to in the assigned BoundedBuffer
into the args passed into the function. Assume
*/
// TODO: Make the input args more general with *void, and have an argument to pass in a copy function
int MR_Get(int partition_i, char **key, char **value) {

    // Index into relevant partition
    BoundedBuffer *bb = &p[partition_i];
    int i;
    sem_wait(&bb->full);
    sem_wait(&bb->mutex);

    /* Critical section BEGIN */
    i = bb->use;
    if (strcmp(bb->buffer[i].key, POISON) == 0) {
        //printf("reducer read poison pill\n");
        sem_post(&bb->mutex);
        return 1;
    };

    // Alloc new memory to store string that parent function wants
    *key = (char*) malloc(sizeof(char) * (strlen(bb->buffer[i].key) + 1));
    *value = (char*) malloc(sizeof(char) * (strlen(bb->buffer[i].value) + 1));

    if (*key == NULL || *value == NULL) {
        perror("Failed to allocate memory for new MR_Get entry");
        exit(EXIT_FAILURE);
    }

    strcpy(*key, bb->buffer[i].key);
    strcpy(*value, bb->buffer[i].value);
    bb->use = (bb->use + 1) % BOUNDED_BUF_SIZE;
    free(bb->buffer[i].key);    //
    free(bb->buffer[i].value);  // free old entry's memory

    /* Critical section END */
    sem_post(&bb->mutex);
    sem_post(&bb->empty);

    return 0;
}

void MR_Mapper_Cleanup() {
    // Thread that executes this section is guaranteed to be last thread.
    // Last thread inserts a poison pill into each bounded buffer
    int i;
    for (int p_index = 0; p_index < num_p; p_index++) {
        BoundedBuffer *bb = &p[p_index];
        sem_wait(&bb->empty);
        sem_wait(&bb->mutex);

        i = bb->fill;
        bb->buffer[i].key = POISON;
        bb->fill = (i + 1) % BOUNDED_BUF_SIZE;

        sem_post(&bb->mutex);
        sem_post(&bb->full);
    }
}

void MR_Reducer_Cleanup() {
    for (int i = 0; i < num_p; i++) {
        sem_destroy(&p[i].full);
        sem_destroy(&p[i].empty);
        sem_destroy(&p[i].mutex);
        free(p[i].buffer);
    }
    free(p);
}