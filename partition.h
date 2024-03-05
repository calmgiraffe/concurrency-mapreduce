#pragma once

#include <semaphore.h>

#define BOUNDED_BUF_SIZE 256
#define POISON "POISON_PILL"

typedef struct keyvalue_t {
    char *key;
    char *value;
} KeyValue;

typedef struct boundedbuffer_t {
    sem_t full;
    sem_t empty;
    sem_t mutex;
    KeyValue *buffer;
    int fill;   // ptr for producer
    int use;    // ptr for consumer
} BoundedBuffer;

typedef unsigned long (*Partitioner)(char *key, int num_partitions);


void MR_InitPartition(int num_mapper, 
        int num_reducer, 
        int num_partitions, 
        Partitioner partitioner);

/*
Function called within map() that generates intermediate (key,val) pair and
send this to respective partition, where each reducer is mapper to a partition.
*/
int MR_Emit(char *key, char *value);

/*
Function called within reduce() that gets a ptr to the next intermediate
(key,val) pair that is available for the reducer to process.
*/
int MR_Get(int partition_i, char **key, char **value);

/* Cleanup tasks for mapper thread (may be blank )*/
void MR_Mapper_Cleanup();

/* Cleanup tasks for reducer thread (may be blank )*/
void MR_Reducer_Cleanup();