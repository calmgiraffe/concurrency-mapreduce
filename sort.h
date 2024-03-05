#pragma once

#include <stdint.h>

typedef struct fileinfo_t {
    char *filename;
    int64_t size;
} FileInfo;


int descending_ord_file_compare(const void *a, const void *b);

int ascending_ord_str_compare(const void *a, const void *b);