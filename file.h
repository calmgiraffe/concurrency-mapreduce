#pragma once

#include <ctype.h>
#include <stdint.h>
#include <unistd.h>

#define BUF_SIZE 4096

int64_t get_size(char * filename);
int get_word(char *buffer, int buffer_size, FILE *f);
int get_char(char *ch, FILE *f);
void toLowerCase(char *str);
void log_error(const char *message);