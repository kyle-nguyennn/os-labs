#include "gfserver.h"
#include "cache-student.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <mqueue.h>
#include <semaphore.h>
#include <stdatomic.h>
#include "shm_channel.h"

#define BUFSIZE (840)

/*
 __.__
Replace with your implementation
 __.__
*/

ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg){
    ssize_t total_transferred = 0;
    ssize_t bytes_sent = 0;
	// size_t file_len;
	int *thread_id = arg;
	// ssize_t read_len;
    // ssize_t write_len;
	// int fildes;
	// struct stat statbuf;
    
    printf("Thread %d handling request for %s\n", *thread_id, path);
    // Open queue for cache message queue
    mqd_t cache_mq = mq_open(CACHE_COMMAND_QUEUE_NAME, O_RDWR, 0666, NULL);
    if (cache_mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    cache_command_t cache_request;
    cache_request.thread_id = *thread_id;
    strcpy(cache_request.path, path);
    if (0>mq_send(cache_mq, (const char*)&cache_request, sizeof(cache_request), 0)) {
        perror("mq_send");
    }
    // Listen to thread-specific reply channel for cache server response 
    char cache_reply_queue_name[50];
    sprintf(cache_reply_queue_name, "%s_%d", CACHE_REPLY_QUEUE_PREFIX, *thread_id);
    mqd_t reply_mq = mq_open(cache_reply_queue_name, O_CREAT | O_RDONLY, 0666, NULL);
    if (reply_mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    cache_reply_t cache_reply;
    if (0> mq_receive(reply_mq, (char*)&cache_reply, MAX_CACHE_REQUEST_LEN+1, NULL)) {
        perror("handle_with_cache: mq_receive");
        return -1;
    }
    printf("Response from cache: status=%d, file_len=%ld\n", cache_reply.status, cache_reply.file_len);

    if (cache_reply.status == CACHE_FILE_NOT_FOUND) {
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    } else {
        gfs_sendheader(ctx, GF_OK, cache_reply.file_len);
        // listen on semaphore for signal from cache to read data from shared mem
        char sem_p_name[50];
        char sem_c_name[50];
        sprintf(sem_p_name, "%s_%d", SEM_P_PREFIX, *thread_id);
        sprintf(sem_c_name, "%s_%d", SEM_C_PREFIX, *thread_id);
        sem_t *sem_p = sem_open(sem_p_name, 0); // Open existing semaphore
        if (sem_p == SEM_FAILED) {
            perror("sem_open failed");
        }
        sem_t *sem_c = sem_open(sem_c_name, 0); // Open existing semaphore
        if (sem_c == SEM_FAILED) {
            perror("sem_open failed");
        }
        void* shm_ptr = get_shm_map_for_thread(*thread_id);
        while (total_transferred < cache_reply.file_len) {
            // if (sem_wait(sem_p) == -1) {
            //     perror("sem_wait failed");
            // }
            sem_wait(sem_p); // wait for full
            // send from shm_ptr
            ssize_t buf_size = min(SHM_SEGMENT_SIZE, cache_reply.file_len-total_transferred);
            printf("Sending %ld bytes from shm\n", buf_size);
            bytes_sent = gfs_send(ctx, shm_ptr, buf_size);
            if (bytes_sent < 0) {
                fprintf(stderr, "Error gfs_send: %ld\n", bytes_sent);
            } else {
                printf("Sent %ld bytes to client\n", bytes_sent);
            }
            total_transferred += bytes_sent;
            // Signal back to cache thread to send new data
            // if (sem_post(sem_c) == -1) {
            //     perror("sem_post failed");
            // }
            printf("Sent progress: %ld/%ld\n", total_transferred, cache_reply.file_len);
            sem_post(sem_c); // signal empty
        }

    }
    printf("Transferred %ld bytes to client\n", total_transferred);
	return total_transferred;
}
