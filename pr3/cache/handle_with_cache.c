#include "gfserver.h"
#include "cache-student.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <mqueue.h>

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
    mqd_t mqd = mq_open(CACHE_COMMAND_QUEUE_NAME, O_RDWR, 0666, NULL);
    if (mqd == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    cache_command_t cache_request;
    cache_request.thread_id = *thread_id;
    strcpy(cache_request.path, path);
    if (0>mq_send(mqd, (const char*)&cache_request, sizeof(cache_request), 0)) {
        perror("mq_send");
    }
    // TODO: Cache reply from cache should be directed to the original thread
    bytes_received = mq_receive(mqd, buffer, MAX_CACHE_REQUEST_LEN+1, NULL);
    if (bytes_received < 0) {
        perror("mq_receive");
        return -1;
    }
    buffer[bytes_received] = '\0';
    printf("Response from cache: %s\n", buffer);

	return bytes_transferred;
}
