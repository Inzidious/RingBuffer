#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
 
#define BUFFER_SIZE 8
#define PROFILE_ITERATIONS 10000000
#define BUFFER_MASK (BUFFER_SIZE - 1)
#define MAX_READERS 3

typedef struct RingBuf {
    atomic_size_t head; //  write pointer
    _Atomic size_t readers[MAX_READERS]; // read pointers
    atomic_int num_readers;

    char arr[BUFFER_SIZE];
} RingBuf;

const char charset[] = "abcdefghijklmnopqrstuvwxyz";

#endif