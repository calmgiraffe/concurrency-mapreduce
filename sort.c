#include <stdint.h>
#include "sort.h"

/*
Pass in as arg to qsort, sorts in descending order.
*/
int descending_ord_file_compare(const void *a, const void *b) {
    const FileInfo *pair1 = (const FileInfo *) a;
    const FileInfo *pair2 = (const FileInfo *) b;

    if (pair1->size < pair2->size) return 1;
    if (pair1->size > pair2->size) return -1;
    return 0;
}

int ascending_ord_str_compare(const void *a, const void *b) {
    const char *str1 = (const char *) a;
    const char *str2 = (const char *) b;

    while (*str1 || *str2) { // while there is at least one char to process
        if (*str1 < *str2) {
            return -1;
        } else if (*str1 > *str2) {
            return 1;
        } else {
            str1++;
            str2++;
        }
    }

    if (*str1 == '\0' && *str2 == '\0') {
        return 0;
    } else if (*str1 == '\0') {
        return -1;
    }
    return 1;
}