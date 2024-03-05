#pragma once

#include "partition.h"

//#define PRINTF_DEBUG
#define MAXWORD 45 + 1

// According to MapReduce API, should be user defined.
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(int partition_number);

// Default hashing function that is passed to MR_Run.
// Can be user defined.
unsigned long MR_DefaultHashPartition(char *key, int num_partitions);

void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition);