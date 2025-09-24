#define GFSERVER_PRIVATE
#include <stdlib.h>
#include "gfserver-student.h"
#include <stdbool.h>



#define BUFSIZE 1024
#define min(a, b) (a>b)?b:a;


#define USAGE                                                                                     \
  "usage:\n"                                                                                      \
  "  gfserver_main [options]\n"                                                                   \
  "options:\n"                                                                                    \
  "  -h                  Show this help message.\n"                                               \
  "  -m [content_file]   Content file mapping keys to content files (Default: content.txt\n"      \
  "  -t [nthreads]       Number of threads (Default: 16)\n"                                       \
  "  -d [delay]          Delay in content_get, default 0, range 0-5000000 "                       \
  "  -p [listen_port]    Listen port (Default: 29458)\n"                                          \
  "(microseconds)\n "

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"nthreads", required_argument, NULL, 't'},
    {"content", required_argument, NULL, 'm'},
    {"port", required_argument, NULL, 'p'},
    {"delay", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

extern unsigned long int content_delay;

extern gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void *arg);

static void _sig_handler(int signo) {
  if ((SIGINT == signo) || (SIGTERM == signo)) {
    exit(signo);
  }
}

void* worker(void* arg) {
  // worker_args_t* args = (worker_args_t*)arg;
  struct timespec timeout_time;
  clock_gettime(CLOCK_REALTIME, &timeout_time); // Get current time
  timeout_time.tv_sec += 30; // Add 3 seconds to the current time

  fprintf(stdout, "Thread started\n");

  while (1) {
    pthread_mutex_lock(&m_tasks);
    while (steque_isempty(&tasks)) {
      pthread_cond_wait(&c_boss, &m_tasks);
    }
    // acquired locks to tasks
    steque_item item = steque_pop(&tasks);
    pthread_mutex_unlock(&m_tasks);
    
		task_item* task_i = (task_item*)item;
    int fd = task_i->fd;
		gfcontext_t** ctx = task_i->ctx;
    fprintf(stdout, "Worker processing %d\n", (int)fd);
    // try to acquire lock for fd for 3s;
    // pthread_mutex_t* m_fd = fdlock_get(fd);
    // fprintf(stdout, "Get fdlock %d\n", (int)fd);
    // int ret = pthread_mutex_timedlock(m_fd, &timeout_time);
    if (false) {
      // cant get fd lock
      // move fd to back of the queue and continue waiting for another task
      printf("Cant acquire fdlock\n");
      steque_enqueue(&tasks, item);
      continue;
    } else {
      // acquired lock to fd
      // reset fd to start of file
    // SEEK_SET indicates that the offset is relative to the beginning of the file
      // off_t new_offset = lseek(fd, 0, SEEK_SET);
      off_t new_offset = 0;
      if (new_offset == (off_t)-1) {
        perror("Error seeking to beginning of file");
        steque_enqueue(&tasks, item);
        // pthread_mutex_unlock(m_fd);
        continue;
      }
      // get file stats by calling fstats
      struct stat file_stat;
      if (fstat(fd, &file_stat) == -1) {
          perror("Error getting file status");
          // design decision: not to re-enqueue the request since the fd has problem
          // pthread_mutex_unlock(m_fd);
          continue;
      }
      long file_len = file_stat.st_size;
      printf("File length: %ld bytes\n", file_len);
      // call header to send file len
      gfs_sendheader(ctx, GF_OK, file_len);
      //send all file at once -> easy -> let TCP handle packet fragmentation
      char buf[BUFSIZE];
      ssize_t bytes_sent=0;
      ssize_t total_sent=0;
      while (total_sent < file_len) {
        ssize_t buf_size = min(BUFSIZE, file_len-total_sent);
        if (0 > pread(fd, buf, buf_size, new_offset)) {
          perror("error reading file");
          // design decision: not to re-enqueue the request since the fd has problem
          break;
        } else {
          if ((bytes_sent = gfs_send(ctx, buf, buf_size)) < 0) {
            fprintf(stderr, "error sending file to client. Abort request\n");
            break;
          }
          printf("Sent %ld bytes to client\n", bytes_sent);
					new_offset += buf_size;
          total_sent += bytes_sent;
        }
      }
      printf("Download done. Sent %ld bytes to client\n", total_sent);
      // release fd lock after download complete
      // pthread_mutex_unlock(m_fd);
    }
  }
}


/* Main ========================================================= */
int main(int argc, char **argv) {
  char *content_map = "content.txt";
  int nthreads = 16;
  gfserver_t *gfs = NULL;
  int option_char = 0;
  unsigned short port = 29458;

  setbuf(stdout, NULL);

  if (SIG_ERR == signal(SIGINT, _sig_handler)) {
    fprintf(stderr, "Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (SIG_ERR == signal(SIGTERM, _sig_handler)) {
    fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:d:rhm:t:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {
      case 'h':  /* help */
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
	  case 'p':  /* listen-port */
        port = atoi(optarg);
        break;
      case 'd':  /* delay */
        content_delay = (unsigned long int)atoi(optarg);
        break;
      case 't':  /* nthreads */
        nthreads = atoi(optarg);
        break;
      case 'm':  /* file-path */
        content_map = optarg;
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);


    }
  }

  /* not useful, but it ensures the initial code builds without warnings */
  if (nthreads < 1) {
    nthreads = 1;
  }

  if (content_delay > 5000000) {
    fprintf(stderr, "Content delay must be less than 5000000 (microseconds)\n");
    exit(__LINE__);
  }

  content_init(content_map);

  /* Initialize thread management */

  /*Initializing server*/
  gfs = gfserver_create();

  //Setting options
  gfserver_set_port(&gfs, port);
  gfserver_set_maxpending(&gfs, 24);
  gfserver_set_handler(&gfs, gfs_handler);
  // set handler args
  handler_args_t handler_args;
  handler_args.nthreads = nthreads;

  gfserver_set_handlerarg(&gfs, &handler_args);  // doesn't have to be NULL!
	// init thread pool
	// handler_args_t* args = (handler_args_t*) arg;

  // initialize results
  pthread_mutex_init(&m_tasks, NULL);
  pthread_cond_init(&c_boss, NULL);
  worker_args_t worker_args;
  pthread_t* thread_pool = (pthread_t*)malloc(nthreads * sizeof(pthread_t));
  fprintf(stdout, "Starting %d threads\n", nthreads);
  for (int i=0; i<nthreads; i++) {
    pthread_create(&thread_pool[i], NULL, worker, &worker_args); // no need worker_args anymore, TODO: clean up
  }
	
  /*Loops forever*/
  gfserver_serve(&gfs);
	
	// clean up thread pool
	for (int i=0; i<nthreads; i++) {
    pthread_join(thread_pool[i], NULL);
  }
}
