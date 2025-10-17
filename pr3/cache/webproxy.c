#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <printf.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <stdlib.h>
#include <mqueue.h>

#include "cache-student.h"
#include "gfserver.h"

// Note that the -n and -z parameters are NOT used for Part 1 
                        
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 8)\n"                      \
"  -p [listen_port]    Listen port (Default: 25362)\n"                                 \
"  -s [server]         The server to connect to (Default: GitHub test data)\n"     \
"  -t [thread_count]   Num worker threads (Default: 8 Range: 200)\n"              \
"  -z [segment_size]   The segment size (in bytes, Default: 5712).\n"                  \
"  -h                  Show this help message\n"


// Options
static struct option gLongOptions[] = {
  {"server",        required_argument,      NULL,           's'},
  {"segment-count", required_argument,      NULL,           'n'},
  {"listen-port",   required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"segment-size",  required_argument,      NULL,           'z'},         
  {"help",          no_argument,            NULL,           'h'},

  {"hidden",        no_argument,            NULL,           'i'}, // server side 
  {NULL,            0,                      NULL,            0}
};


//gfs
static gfserver_t gfs;
//handles cache
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);
atomic_int g_shutdown = 0;
unsigned short nworkerthreads = 8;

void cleanup_mq() {
  for (int thread_id=0; thread_id < nworkerthreads; thread_id++) {
    char cache_reply_queue_name[50];
    sprintf(cache_reply_queue_name, "%s_%d", CACHE_REPLY_QUEUE_PREFIX, thread_id);
    if(mq_unlink(cache_reply_queue_name) == 0) {
        printf("Message queue %s removed from system.\n", cache_reply_queue_name);
    }
  }
}

static void _sig_handler(int signo){
  if (signo == SIGTERM || signo == SIGINT){
    //cleanup could go here
    gfserver_stop(&gfs);
    atomic_store(&g_shutdown, 1);
    cleanup_mq();
    exit(signo);
  }
}

int main(int argc, char **argv) {
  int option_char = 0;
  char *server = "https://raw.githubusercontent.com/gt-cs6200/image_data";
  unsigned int nsegments = 8;
  unsigned short port = 25362;
  size_t segsize = 5712;

  //disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  // Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "s:qht:xn:p:lz:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;                                          
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;   
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'i':
      //do not modify
      case 'O':
      case 'A':
      case 'N':
            //do not modify
      case 'k':
        break;
    }
  }


  if (server == NULL) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (segsize < 824) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (port > 65332) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }
  if ((nworkerthreads < 1) || (nworkerthreads > 200)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }
  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }



  /* Initialize shared memory set-up here */
  // Prepare 1 reply queue per thread
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;     // Max messages in queue
  attr.mq_msgsize = MAX_CACHE_REQUEST_LEN;
  attr.mq_curmsgs = 0;     // Current messages (ignored for mq_open)
  for (int thread_id=0; thread_id < nworkerthreads; thread_id++) {
    char cache_reply_queue_name[50];
    sprintf(cache_reply_queue_name, "%s_%d", CACHE_REPLY_QUEUE_PREFIX, thread_id);
    if (0> mq_open(cache_reply_queue_name, O_CREAT, 0666, &attr)) {
      perror("webproxy init cache_reply_mq: mq_open");
    }
  }
  // TODO: prepare shared memory segment for each thread

  // Initialize server structure here
  gfserver_init(&gfs, nworkerthreads);

  // Set server options here
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 187);

  // Set up arguments for worker here
  int* thread_ids = malloc(nworkerthreads*sizeof(int));
  for(int i = 0; i < nworkerthreads; i++) {
    // pass thread_id as arg to create control channel for each thread
    thread_ids[i] = i;
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, thread_ids+i);
  }
  
  // Invokethe framework - this is an infinite loop and will not return
  gfserver_serve(&gfs);

  // TODO: On SIGTERM and SIGINT, clean up shared memory and message queue

  // line never reached
  return -1;

}
