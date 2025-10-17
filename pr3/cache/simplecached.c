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

// CACHE_FAILURE
#if !defined(CACHE_FAILURE)
    #define CACHE_FAILURE (-1)
#endif 

unsigned long int cache_delay;
mqd_t cache_mq;

static void _sig_handler(int signo){
	if (signo == SIGTERM || signo == SIGINT){
		// This is where your IPC clean up should occur
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

void handle_cache_get() {
    // Get new message queue
    while (1) {
        cache_command_t cache_command;
        char cache_reply[MAX_CACHE_REQUEST_LEN+1];
        ssize_t bytes_received = mq_receive(cache_mq, (char*)&cache_command, MAX_CACHE_REQUEST_LEN+1, NULL);
        if (bytes_received < 0) {
            perror("mq_receive");
            // exit this request
            return; // TODO: continue in worker loop
        }
        // Null terminated string
        printf("Receive from thread %d, path: %s\n", cache_command.thread_id, cache_command.path);
        int fd = simplecache_get(cache_command.path);
        if (fd == -1) {
            fprintf(stderr, "File not found\n"); 
            // send reply back to client
            sprintf(cache_reply, "FILE_NOT_FOUND");
            mq_send(cache_mq, cache_reply, strlen(cache_reply), 0);
        } else {
            // TODO: open a memory segment to send data back
            sprintf(cache_reply, "TODO: reply with shared mem name (semaphore name is induced from shared mem name");
            mq_send(cache_mq, cache_reply, strlen(cache_reply), 0);
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
    // handle mq message in thread, following boss-worker pattern
    printf("Created message queue %d\n", cache_mq);
    handle_cache_get();

    // TODO: proper SIGTERM and SIGINT handler to unlink queue
    if(mq_unlink(CACHE_COMMAND_QUEUE_NAME) == 0) {
        printf("Message queue %s removed from system.\n", CACHE_COMMAND_QUEUE_NAME);
    }

	// Line never reached
	return -1;
}
