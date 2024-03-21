#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include "file.h"


/*
Returns the filesize of the given file. If error, returns -1.
*/
int64_t get_size(char *filename) {
    if (filename == NULL) {
        fprintf(stderr, "%s\n", __func__);
        perror("filename is NULL");
        return -1;
    }
    struct stat statbuf;

    if (stat(filename, &statbuf) != 0) {
        fprintf(stderr, "%s\n", __func__);
        perror("stat");
        return -1;
    }
    return statbuf.st_size;
}

/*
Returns 1 if char read, 0 if EOF and nothing else to read, -1 if read err.
*/
int get_char(char *ch, FILE *f) {
    _Thread_local static char buffer[BUF_SIZE] __attribute__((aligned(BUF_SIZE)));
    _Thread_local static int buffer_ptr = 0;
    _Thread_local static size_t bytes_read = 0;

    if (buffer_ptr >= bytes_read) {
        // Cases:
        // 1) bytes_read = 4K, buffer_ptr = 4K
        // 2) 0 < bytes_read < 4K, buffer_ptr = bytes_read
        // 3) bytes_read = 0, buffer_ptr = bytes_read
        bytes_read = fread(buffer, sizeof(char), BUF_SIZE, f);

        // Check the rare case of read error
        if (ferror(f)) {
            fprintf(stderr, "%s\n", __func__);
            perror("fread");
            return -1;
        }
        if (feof(f) && bytes_read == 0) {
            // EOF and nothing read
            bytes_read = 0;
            buffer_ptr = 0;
            return 0;
            
        } 
        buffer_ptr = 0; 
    }
    *ch = buffer[buffer_ptr];
    buffer_ptr++;
    return 1;
}

static inline int consumeAlpha(char *ch, FILE *f) {
    while (1) {
        int ret = get_char(ch, f);
        if (ret != 1) 
            return ret;
        if (*ch == ' ') 
            return 1;
    }
}

static inline int consumeAlnum(char *ch, FILE *f) {
    while (1) {
        int ret = get_char(ch, f);
        if (ret != 1) 
            return ret;
        if (*ch == ' ')  
            return 1;
    }
}

/*
Return the length of the word that was parsed. If no more words, returns 0.
If file read error from get_char, returns -1.
Words should only be A-Z and a-z.
Words should contain no special characters like apostrophes.
*/
int get_word(char *buffer, int buffer_size, FILE *f) {
    int index = 0, wordlen = 0, ret;
    char ch;

    // Loop until a valid word found.
    while (1) {
        // Return if err or no more chars.
        ret = get_char(&ch, f);
        if (ret != 1) return ret;

        // Is valid letter, add to buffer.
        if (isalpha(ch)) {
            buffer[index] = ch;
            index++;

            // Ignore alphabetical strings too long to be words
            // Consume remainder of string, and prepare for new word
            if (index >= buffer_size) {
                index = 0; 
                consumeAlpha(&ch, f); // TODO: Can I somehow read the return val?
                /*
                while (1) {
                    ret = get_char(&ch, f);
                    if (ret != 1) return ret;
                    if (isalpha(ch)) break;
                }*/
            }
        }
        // Ignore words with numbers
        else if (isdigit(ch)) {
            // Consume remainder of string, and prepare for new word
            index = 0;
            consumeAlnum(&ch, f);
            /*
            while (1) {
                ret = get_char(&ch, f);
                if (ret != 1) return ret;
                if (isalnum(ch)) break;
            } */
        }
        // !isalpha(ch) && !isdigit(ch) && index > 0
        // Valid word is found.
        else if (index > 0) {
            wordlen = index;
            buffer[index] = '\0';
            index = 0;
            if (ch == '\'') {
                consumeAlpha(&ch, f);
            }
            break;
        }
    }
    return wordlen;
}

/*
Converts the passed in string to lowercase.
*/
void toLowerCase(char *str) {
    if (str == NULL) return;

    while (*str) {
        // If c is not an uppercase letter, this function returns c itself.
        *str = tolower((unsigned char) *str);
        str++;
    }
}

void log_error(const char *message) {
    static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&log_mutex); // Lock the mutex

    FILE *log_file = fopen("err.log", "a");
    if (log_file != NULL) {
        fputs(message, log_file);
        fputs("\n", log_file);
        fclose(log_file);
    }

    pthread_mutex_unlock(&log_mutex); // Unlock the mutex
}