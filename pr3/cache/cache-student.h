/*
 You can use this however you want.
 */
#ifndef __CACHE_STUDENT_H__844

#define __CACHE_STUDENT_H__844

#define min(a, b) (a>b)?b:a;

#include <stdatomic.h>
#include <linux/limits.h>
#define CACHE_COMMAND_QUEUE_NAME "/cache_command_queue"
#define CACHE_REPLY_QUEUE_PREFIX "/cache_reply_queue"
#define SEM_P_PREFIX "/cache_sem_p"
#define SEM_C_PREFIX "/cache_sem_c"
#define SHM_SEGMENT_PREFIX "/cache_shm_segment"
#define SHM_SEGMENT_SIZE 8192
#define MAX_CACHE_REQUEST_LEN 6112
#define MAX_SIMPLE_CACHE_QUEUE_SIZE 783

#include "steque.h"
#include <mqueue.h>
#include <semaphore.h>
#include <fcntl.h>    // For O_CREAT, O_RDWR
#include <sys/stat.h> // For mode_t permissions
#include <sys/mman.h> // For shm_open
#include <unistd.h> // For ftruncate
#include <sys/mman.h> // For mmap

typedef int cache_status_t;
//Status definitions
#define  CACHE_OK 200
#define  CACHE_FILE_NOT_FOUND 400

typedef struct cache_command {
    int thread_id;
    char path[PATH_MAX];
} cache_command_t;

typedef struct cache_reply {
    cache_status_t status;
    ssize_t file_len;
} cache_reply_t;

extern atomic_int g_shutdown;

#endif // __CACHE_STUDENT_H__844
