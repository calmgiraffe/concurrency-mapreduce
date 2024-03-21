# MapReduce

A simplified version of MapReduce for just a single machine. It currently
tokenizes a text file into its individual words, although this can be changed by
reimplementing the `Map()` and `Reduce()` functions. Pointers to these two functions
are passed into `MR_Run()`, which is the main routine, making the program modular.
Custom functions can be passed into MR_Run() to achieve the desired Map and
Reduce behavior. MR_Run() also takes arguments for the number of mapper and
reducer threads, as well as a pointer to a hash function-- of which, the latter
determines which reducer thread the output of the Map() function should be sent to.

The specific objectives of this assignment were as follows:

- To learn about the MapReduce model.
- To implement a correct and efficient MapReduce framework using threads and
  related functions.
- To gain experience with writing concurrent code that efficiently solves common
  problems like producers/consumers, taking into account disk I/O, an optimal
  algorithmic approach, and more.
- To gain experience with lockless data structures, as demonstrated by the
  lockless BST.

## Architecture of framework.c

### main()

As with all programs, execution begins from `main()`. The default number of mapper
and reducer threads is set to 5 and 5 respectively, but custom values can be set
with the `[-m mappers]` and `[-r reducers]` flags. The list of files to process
is a required argument. For example, a run with 4 mappers and 2 reducers might
look like `./mapreduce -m 4 -r 2 a.txt b.txt c.txt`.

`main()` initializes program variables then calls the below function:

```
MR_Run(argc, argv, Map, num_mappers, Reduce, num_reducers, MR_DefaultHashPartition);
```

Here, `Map`, `Reduce`, and `MR_DefaultHashPartition` are pointers to functions.
The functions need to return and accept specific arguments as defined below:

```
// From framework.c
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(int partition_number);

// From partition.c
typedef unsigned long (*Partitioner)(char *key, int num_partitions);
```

The function calls `MR_Run()` which is the main routine of the MapReduce framework.

### MR_Run()

The list of given input files is sorted in descending
size order so that the largest files are processed by the Mapper threads first.
This allows for an even distribution of workload so that when the program
eventually gets to the last files to be processed, a scenario where the program
is waiting for a single thread to finish processing a large file is unlikely to
arise. The built-in `qsort()` function gives a flexible and efficient way to
sort.

`MR_InitPartition()` sets up an array of bounded buffers. An array of num_p
struct boundedbuffer_t is made, one for each reducer thread, where each entry
has a `buffer` member that points to the actual buffer holding the data.

```
typedef struct boundedbuffer_t {
    sem_t full;
    sem_t empty;
    sem_t mutex;
    KeyValue *buffer;
    int fill;   // ptr for producer
    int use;    // ptr for consumer
} BoundedBuffer;
```

After this data structure is made, the mapper and reducer threads are created
(forked) from the main thread. These threads will start their respective routines
and start doing work. Mapper and Reducer threads are carefully synchronized to
ensure correctness; this is explained in more detail later on.

```
for (i = 0; i < num_mappers; i++) {
    if (pthread_create(&mthreads[i], NULL, mapper_proc, (void*) Map) != 0) {
        perror("Failed to create mthread");
    }
}
for (i = 0; i < num_reducers; i++) {
    if (pthread_create(&rthreads[i], NULL, reducer_proc, (void*) Reduce) != 0) {
        perror("Failed to create rthread");
    }
}
```

### mapper_proc() and reducer_proc()

In `mapper_proc`, each mapper thread atomically fetches and increments `pairs_index`,
then uses the fetched value to index into the files array and process the pointed-to
file. The use of atomic variables guarantees that a file will only be selected once,
and as a plus, is lock-free. A similar approach is used for `reducer_proc` to assign
each reducer a unique partition.

```
void *mapper_proc(void *arg) {
    int i, array_len = num_files;
    Mapper map = (Mapper) arg;

    while ((i = atomic_fetch_add(&pairs_index, 1)) < array_len) {
        // i.e., Map(char *filename)
        // Parses specified file and generates intermediate (key,val)
        map(files[i].filename);
    }
    return NULL;
}

void *reducer_proc(void *arg) {
    Reducer reduce = (Reducer) arg;
    int partition_index_local = atomic_fetch_add(&partition_index, 1);

    // rthread accumulates (key,val)'s which were assigned to it
    reduce(partition_index_local);

    return NULL;
}
```

### Map() and Reduce()

These are the custom functions that can be passed in from `main()`. In the code,
an example of a word count implementation is given.

`Map()` relies on the functions provided by file.c to parse the contents of the
specified text file. Here, the main function to note is `get_word()`, which
returns the next word in the file. The word is converted to lowercase and passed
to a partition via `MR_Emit()`. The determination of partition number (i.e.,
hashing) happens within MR_Emit().

```
void Map(char *file_name) {
    // ...

    while (1) {
        // Loop through all the words of FILE f and place next word in buffer
        ret = get_word(buffer, MAXWORD, f);
        if (ret == -1) {
            log_error("Mapper get_word() returned error");
            log_error(file_name);
            break; // Just move on to next file
        }
        else if (ret == 0) break; // no more words to get

        // Send key, value to partition
        toLowerCase(buffer);
        MR_Emit(buffer, value);
    }
    // ...
}
```

The opposite function that is paired with `MR_Emit()` is found within `Reduce()`
and is named `MR_Get()`. Reduce() continuously calls MR_Get() until MR_Get()
returns 1, at which point the while-loop terminates. Each valid entry retrieved
from MR_Get() is made into a node for a BST, and is inserted into this BST to
be sorted.

```
void Reduce(int partition_number) {
    char *key;
    char *key_cpy, *val_cpy;
    Node *newnode;

    while (1) {
        // Get the next pair from the partition
        if (MR_Get(partition_number, &key_cpy, &val_cpy) == 1) 
            break;

        // memory for node->key
        key = malloc(sizeof(char) * MAXWORD);
        strcpy(key, key_cpy);

        // memory for node
        newnode = malloc(sizeof(Node));
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
```

## Architecture of file.c

The main optimization to point out here is the reading of the file in 4K chunks
as opposed to invoking a I/O call for each `get_char()` call. This aligns with the
4K sector size of modern disks.

## Architecture of partition.c

The synchronization problem presented by my framework is the classic producer-consumer
problem, and it is solved using semaphores in `MR_Emit()` and `MR_Get()`. One section
in `MR_Get()` is notable:

```
i = bb->use;
if (strcmp(bb->buffer[i].key, POISON) == 0) {
    //printf("reducer read poison pill\n");
    sem_post(&bb->mutex);
    return 1;
};
```

If the entry retrieved from the buffer is a "poison pill", `MR_Get()` returns 1 to
its parent `Reduce()`, thereby making Reduce() break out of its while-loop. This
poison pill is inserted into all partitions when all producers are done with their
work because we can guarantee at this point that no more valid (key,value) entries
will be inserted in the partitions. The reducer threads terminates and are joined
as soon as possible, which is the time optimal approach. The insertion of the
poison entries occurs in `MR_Mapper_Cleanup()` as shown:

```
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
```

## Results
