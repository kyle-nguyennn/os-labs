/*
 You can use this however you want.
 */
#ifndef __CACHE_STUDENT_H__844

#define __CACHE_STUDENT_H__844

#include <linux/limits.h>
#define CACHE_COMMAND_QUEUE_NAME "/cache_command_queue"
#define MAX_CACHE_REQUEST_LEN 6112
#define MAX_SIMPLE_CACHE_QUEUE_SIZE 783

#include "steque.h"

typedef struct cache_command {
    int thread_id;
    char path[PATH_MAX];
} cache_command_t;

#endif // __CACHE_STUDENT_H__844
