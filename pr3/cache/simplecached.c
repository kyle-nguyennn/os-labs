#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <printf.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include "cache-student.h"
#include "shm_channel.h"
#include "simplecache.h"
#include "gfserver.h"
#include <mqueue.h>
#include <sys/stat.h>
#include <stdbool.h>

// CACHE_FAILURE
#if !defined(CACHE_FAILURE)
    #define CACHE_FAILURE (-1)
#endif 

unsigned long int cache_delay;
mqd_t cache_mq;

static void _sig_handler(int signo){
	if (signo == SIGTERM || signo == SIGINT){
		// This is where your IPC clean up should occur
        if(mq_unlink(CACHE_COMMAND_QUEUE_NAME) == 0) {
            printf("Message queue %s removed from system.\n", CACHE_COMMAND_QUEUE_NAME);
        }
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default is 8, Range is 1-100)\n"      \
"  -d [delay]          Delay in simplecache_get (Default is 0, Range is 0-2500000 (microseconds)\n "	\
"  -h                  Show this help message\n"

//OPTIONS
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {"delay", 			 required_argument,		 NULL, 			 'd'}, // delay.
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int handle_file(int fd) {
    // TODO: refactor this later
    int bytes_transferred  = 0;
	// strncpy(buffer,data_dir, BUFSIZE);
	// strncat(buffer,path, BUFSIZE);

	// if( 0 > (fildes = open(buffer, O_RDONLY))){
	// 	if (errno == ENOENT)
	// 		//If the file just wasn't found, then send FILE_NOT_FOUND code
	// 		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	// 	else
	// 		//Otherwise, it must have been a server error. gfserver library will handle
	// 		return SERVER_FAILURE;
	// }

	// //Calculating the file size
	// if (fstat(fildes, &statbuf) < 0) {
	// 	return SERVER_FAILURE;
	// }
	// file_len = (size_t) statbuf.st_size;
	// ///

	// gfs_sendheader(ctx, GF_OK, file_len);

	// //Sending the file contents chunk by chunk

	// bytes_transferred = 0;
	// while(bytes_transferred < file_len){
	// 	read_len = read(fildes, buffer, BUFSIZE);
	// 	if (read_len <= 0){
	// 		fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
	// 		return SERVER_FAILURE;
	// 	}
	// 	write_len = gfs_send(ctx, buffer, read_len);
	// 	if (write_len != read_len){
	// 		fprintf(stderr, "handle_with_file write error");
	// 		return SERVER_FAILURE;
	// 	}
	// 	bytes_transferred += write_len;
	// }
    return bytes_transferred;
}

void* handle_cache_get() {
    // Get new message queue
    while (1) {
        cache_command_t cache_command;
        cache_reply_t cache_reply;
        if (0> mq_receive(cache_mq, (char*)&cache_command, MAX_CACHE_REQUEST_LEN+1, NULL)) {
            perror("mq_receive");
            // exit this request
            continue; // TODO: continue in worker loop
        }
        printf("Receive from thread %d, path: %s\n", cache_command.thread_id, cache_command.path);
        int fd = simplecache_get(cache_command.path);
        // reply on thread-specific channel
        char cache_reply_queue_name[50];
        sprintf(cache_reply_queue_name, "%s_%d", CACHE_REPLY_QUEUE_PREFIX, cache_command.thread_id);
        mqd_t reply_mq = mq_open(cache_reply_queue_name, O_WRONLY, 0666, NULL);
        if (reply_mq == (mqd_t)-1) {
            perror("mq_open");
            exit(EXIT_FAILURE);
        }
        if (fd == -1) {
            fprintf(stderr, "File not found\n"); 
            // send reply back to client
            cache_reply.status = CACHE_FILE_NOT_FOUND;
            cache_reply.file_len = -1;
            mq_send(reply_mq, (const char*)&cache_reply, sizeof(cache_reply), 0);
        } else {
            // TODO: open a memory segment to send data back
            struct stat file_info;
            if (fstat(fd, &file_info) == -1) {
                perror("fstat");
            }
            cache_reply.status = CACHE_OK;
            cache_reply.file_len = file_info.st_size; // TODO: fstat(fd) to get file len
            // Open corresponding semaphore for flow control
            char sem_p_name[50];
            char sem_c_name[50];
            sprintf(sem_p_name, "%s_%d", SEM_P_PREFIX, cache_command.thread_id);
            sprintf(sem_c_name, "%s_%d", SEM_C_PREFIX, cache_command.thread_id);
            sem_t *sem_p = sem_open(sem_p_name, 0); // Open existing semaphore
            if (sem_p == SEM_FAILED) {
                perror("sem_open failed");
            }
            sem_t *sem_c = sem_open(sem_c_name, 0); // Open existing semaphore
            if (sem_c == SEM_FAILED) {
                perror("sem_open failed");
            }
            ssize_t total_sent = 0;
            bool in_progress = false;

            // Open corresponding shared memory for data transfer
            void* shm_ptr = get_shm_map_for_thread(cache_command.thread_id);
            // stream fd content to shm by the shm size and signal cache thread
            while (total_sent < file_info.st_size) {
                // if (sem_wait(sem_c) == -1) { // wait for proxy to process data in shm
                //     perror("sem_wait failed");
                // }
                sem_wait(sem_c); // wait for empty
                ssize_t buf_size = min(SHM_SEGMENT_SIZE, cache_reply.file_len-total_sent);
                if (0 > pread(fd, shm_ptr, buf_size, total_sent)) {
                    perror("error reading file");
                    break;
                } else {
                    total_sent += buf_size;
                    // sem_post signal cache thread to process the shm
                    // if (sem_post(sem_p) == -1) {
                    //     perror("sem_post failed");
                    // }
                    sem_post(sem_p); // signal full
                }    
                if (!in_progress) {
                    // Use mq_receive on cache thread as blocker for first send
                    mq_send(reply_mq, (const char*)&cache_reply, sizeof(cache_reply), 0);
                    in_progress = true;
                }
                printf("Sent progress: %ld/%ld\n", total_sent, cache_reply.file_len);
                // DEBUG
                // int sem_p_value;
                // int sem_c_value;
                // sem_getvalue(sem_p, &sem_p_value);
                // sem_getvalue(sem_c, &sem_c_value);
                // printf("Semp Producer value: %d\n", sem_p_value);
                // printf("Semp Consumer value: %d\n", sem_c_value);

            }
            printf("Sent %ld bytes to proxy server\n", total_sent);

        }
    }
}

int main(int argc, char **argv) {
	int nthreads = 6;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "d:ic:hlt:x", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				Usage();
				exit(1);
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;				
			case 'h': // help
				Usage();
				exit(0);
				break;    
            case 'c': //cache directory
				cachedir = optarg;
				break;
            case 'd':
				cache_delay = (unsigned long int) atoi(optarg);
				break;
			case 'i': // server side usage
			case 'o': // do not modify
			case 'a': // experimental
				break;
		}
	}

	if (cache_delay > 2500000) {
		fprintf(stderr, "Cache delay must be less than 2500000 (us)\n");
		exit(__LINE__);
	}

	if ((nthreads>100) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads must be in between 1-100\n");
		exit(__LINE__);
	}
	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}
	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}
	/*Initialize cache*/
	simplecache_init(cachedir);

	// Cache should go here
    // Main thread open up command message queue
    struct mq_attr attr;
    // attr.mq_maxmsg = MAX_SIMPLE_CACHE_QUEUE_SIZE;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;     // Max messages in queue
    attr.mq_msgsize = MAX_CACHE_REQUEST_LEN;
    attr.mq_curmsgs = 0;     // Current messages (ignored for mq_open)
    cache_mq = mq_open(CACHE_COMMAND_QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (cache_mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    printf("Created message queue %d\n", cache_mq);
    // handle mq message in thread, following boss-worker pattern
    pthread_t* thread_pool = (pthread_t*)malloc(nthreads * sizeof(pthread_t));
    fprintf(stdout, "Starting %d threads for cache process\n", nthreads);
    for (int i=0; i<nthreads; i++) {
      pthread_create(&thread_pool[i], NULL, handle_cache_get, NULL); // no need worker_args anymore, TODO: clean up
    }
      
    // clean up thread pool
    for (int i=0; i<nthreads; i++) {
      pthread_join(thread_pool[i], NULL);
    }

	// Line never reached
	return -1;
}
