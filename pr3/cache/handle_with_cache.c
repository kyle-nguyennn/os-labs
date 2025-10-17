#include "gfserver.h"
#include "cache-student.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <mqueue.h>
#include <semaphore.h>
#include <stdatomic.h>

#define BUFSIZE (840)

/*
 __.__
Replace with your implementation
 __.__
*/

ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg){
    ssize_t bytes_transferred = 0;
    ssize_t bytes_received = 0;
	// size_t file_len;
	int *thread_id = arg;
	// ssize_t read_len;
    // ssize_t write_len;
	char buffer[MAX_CACHE_REQUEST_LEN];
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
    bytes_received = mq_receive(reply_mq, buffer, MAX_CACHE_REQUEST_LEN+1, NULL);
    if (bytes_received < 0) {
        perror("handle_with_cache: mq_receive");
        return -1;
    }
    buffer[bytes_received] = '\0';
    printf("Response from cache: %s\n", buffer);

    if (strcmp(buffer, "FILE_NOT_FOUND") == 0) {
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    } else {
        // listen on semaphore for signal from cache to read data from shared mem
    }

	return bytes_transferred;
}
