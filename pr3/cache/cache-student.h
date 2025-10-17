/*
 You can use this however you want.
 */
#ifndef __CACHE_STUDENT_H__844

#define __CACHE_STUDENT_H__844

#include <stdatomic.h>
#include <linux/limits.h>
#define CACHE_COMMAND_QUEUE_NAME "/cache_command_queue"
#define CACHE_REPLY_QUEUE_PREFIX "/cache_reply_queue"
#define SEM_PREFIX "/cache_sem"
#define SHM_SEGMENT_PREFIX "/cache_shm_segment"
#define SHM_SEGMENT_SIZE 8192
#define MAX_CACHE_REQUEST_LEN 6112
#define MAX_SIMPLE_CACHE_QUEUE_SIZE 783

#include "steque.h"

typedef struct cache_command {
    int thread_id;
    char path[PATH_MAX];
} cache_command_t;

extern atomic_int g_shutdown;

#endif // __CACHE_STUDENT_H__844
