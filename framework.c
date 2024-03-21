#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <semaphore.h>
#include <string.h>
#include "framework.h"
#include "file.h"
#include "sort.h"
#include "partition.h"
#include "bst.h"


/*
optind = index of the next element to be processed in argv. 
The system initializes this value to 1.

By default, getopt() prints an error message on standard
error, places the erroneous option character in optopt, and
returns '?' as the function result.
*/
extern Node *head;
extern char *optarg;
extern int optind, opterr, optopt;
int num_files;
static FileInfo *files;
_Alignas(64) static atomic_int pairs_index;
_Alignas(64) static atomic_int partition_index;
_Alignas(64) static atomic_int global_mthread_id;


/*
Returns the hash of a string using djb2.
The string should end with \0 as usual.
*/
unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}


/*
Function to process the given file and generate intermediate keys.
Parses the file in page size chunks. Generate tokens, and pass these to a 
the partition data structure using MR_Emit. 
*/
void Map(char *file_name) {
    char buffer[MAXWORD];
    char value[2] = "1";
    int ret;

    // Basic error checking
    if (file_name == NULL) {
        perror("Mapper given filename is NULL");
        exit(EXIT_FAILURE);
    }
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        perror("Mapper failed to open file");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Loop through all the words of FILE f and place next word in buffer
        ret = get_word(buffer, MAXWORD, f);
        if (ret == -1) {
            log_error("Mapper get_word() returned error");
            log_error(file_name);
            // Just move on to next file
            break;
        }
        else if (ret == 0) break;

        // Send key, value to partition
        toLowerCase(buffer);
        MR_Emit(buffer, value);

        #ifdef PRINTF_DEBUG
        printf("%s: %s -->\n", file_name, buffer);
        #endif
    }
    fclose(f);
}

/*
One or more threads are mapped to a partition.
Each thread continuously calls MR_Get(partition_num) until there is no more
data in the partition to process.
*/
void Reduce(int partition_number) {
    char *key;
    char *key_cpy, *val_cpy;
    Node *newnode;

    while (1) {
        // Get the next pair from the partition
        if (MR_Get(partition_number, &key_cpy, &val_cpy) == 1) 
            break;

        #ifdef PRINTF_DEBUG
        printf("part%d: --> (%s, %s)\n", partition_number, key_cpy, val_cpy);
        #endif

        key = malloc(sizeof(char) * MAXWORD);
        if (key == NULL) {
            perror("Could not allocate memory for node key");
            exit(EXIT_FAILURE);
        }
        strcpy(key, key_cpy);

        newnode = malloc(sizeof(Node));
        if (newnode == NULL) {
            perror("Could not allocate memory for node");
            exit(EXIT_FAILURE);
        }
            
        newnode->key = key;
        newnode->value = atoi(val_cpy);
        newnode->left = NULL;
        newnode->right = NULL;

        // Tabulate final results
        BST_insert(&head, newnode, ascending_ord_str_compare);  
        
        free(key_cpy);
        free(val_cpy);
    }
}

/*
The function that each mapper thread calls -- does not use locks.
atomic_fetch_add guarantees that each thread will have a unique i value.
If all threads have a unique i value, then only one thread can index into
the very last item in the array. This thread atomically fetches this index,
then increment pairs_index just beyond the array. An arbitrary number of
other threads might call atomic_fetch_add and get a unique value of i that
make the condition false. Thus, this solution is correct.
*/
void *mapper_proc(void *arg) {
    #ifdef PRINTF_DEBUG
    int local_thread_id = atomic_fetch_add(&global_mthread_id, 1);
    #endif

    int i, array_len = num_files;
    Mapper map = (Mapper) arg;

    while ((i = atomic_fetch_add(&pairs_index, 1)) < array_len) {
        #ifdef PRINTF_DEBUG
        printf("m%d: %s START\n", local_thread_id, files[i].filename);
        #endif
        
        // i.e., Map(char *filename)
        // Parses specified file and generates intermediate (key,val)
        map(files[i].filename);

        #ifdef PRINTF_DEBUG
        printf("m%d: %s DONE\n", local_thread_id, files[i].filename);
        #endif
    }
    return NULL;
}

/*
Function for rthreads.
Each reducer thread is assigned one partition.
*/
void *reducer_proc(void *arg) {
    Reducer reduce = (Reducer) arg;
    int partition_index_local = atomic_fetch_add(&partition_index, 1);

    #ifdef PRINTF_DEBUG
    printf("r%d: START\n", partition_index_local);
    #endif

    // rthread accumulates (key,val)'s which were assigned to it
    reduce(partition_index_local);

    #ifdef PRINTF_DEBUG
    printf("r%d: DONE\n", partition_index_local);
    #endif

    return NULL;
}


/*
Main routine that creates mapper and reducer threads.
argv = list of filenames to process
argc = # args, including program name
*/
void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition) {
    
    char *filename;
#ifndef UNIT_TEST
    int optint_cpy = optind;
#else
    int optint_cpy = 0;
#endif
    int i;
    num_files = argc - optint_cpy;
    atomic_init(&pairs_index, 0); // used by mapper proc to index file array
    atomic_init(&partition_index, 0); // used for assigning reducer a partition
    atomic_init(&global_mthread_id, 0); // used for debugging

    // Generate the array of (filename, size) objects.
    files = (FileInfo *) malloc(sizeof(FileInfo) * num_files);
    if (files == NULL) {
        perror("Failed to allocate memory for FileInfos");
        exit(EXIT_FAILURE);
    }

    // Sort in descending size order
    for (i = 0; i < num_files; i++) {
        filename = argv[optint_cpy + i];
        files[i].size = get_size(filename);
        files[i].filename = filename;
        //printf("%s\n", filename);
    }
    qsort(files, num_files, sizeof(FileInfo), descending_ord_file_compare);
    
    // Set up the data structure that the mappers will write to, and
    // specify the hashing function the data structure will use
    MR_InitPartition(num_mappers, num_reducers, num_reducers, partition);

  
    pthread_t *mthreads = (pthread_t*) malloc(sizeof(pthread_t) * num_mappers);
    pthread_t *rthreads = (pthread_t*) malloc(sizeof(pthread_t) * num_reducers);

    if ((mthreads == NULL || rthreads == NULL)) {
        perror("Failed to allocate memory for m and r threads");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < num_mappers; i++) {
        if (pthread_create(&mthreads[i], NULL, mapper_proc, (void*) Map) != 0) {
            perror("Failed to create mthread");
        }
    }
    // NOTE: The current plan is to use a consumer-producer model.
    // Alt approach: wait until all mthreads are done, then have each
    // reducer iterate through their respective partition.
    for (i = 0; i < num_reducers; i++) {
        if (pthread_create(&rthreads[i], NULL, reducer_proc, (void*) Reduce) != 0) {
            perror("Failed to create rthread");
        }
    }
    // Cleanup
    for (i = 0; i < num_mappers; i++) {
        pthread_join(mthreads[i], NULL);
    }
    MR_Mapper_Cleanup();

    for (i = 0; i < num_reducers; i++) {
        pthread_join(rthreads[i], NULL);
    }
    MR_Reducer_Cleanup();

    free(mthreads);
    free(rthreads);
    free(files);
    BST_print_inorder(head);
    BST_free(head);
}


#ifndef UNIT_TEST
/*
Parses cmdline args and does basic error checking -- checks that files are
actually specifies in argv[].
*/
int main(int argc, char *argv[]) {
    struct timeval wall_start, wall_end;
    int opt, num_mappers = 5, num_reducers = 5;

    // NOTE: Can add more complexity to cmdline parsing
    while ((opt = getopt(argc, argv, "m:r:h?")) != -1) {
        switch (opt) {
        case 'm':
            num_mappers = atoi(optarg);
            break;
        case 'r':
            num_reducers = atoi(optarg);
            break;
        case 'h':
        case '?':
        default:
            fprintf(stderr, "Usage: %s [-m mappers] [-r reducers] <files...>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    // optind should now point to the first string in the files list
    if (argc - optind == 0) {
        fprintf(stderr, "Usage: %s [-m mappers] [-r reducers] <files...>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    gettimeofday(&wall_start, NULL);
    MR_Run(argc, argv, Map, num_mappers, Reduce, num_reducers, MR_DefaultHashPartition);
    gettimeofday(&wall_end, NULL);

    double wall_time = (wall_end.tv_sec - wall_start.tv_sec) + 
                       (wall_end.tv_usec - wall_start.tv_usec) / 1000000.0;

    
    printf("Wall clock time: %f seconds\n", wall_time);

    return 0;
}
#endif