#include "gfserver-student.h"
#include "gf-student.h"
#include "workload.h"
#include "content.h"
#include <asm-generic/errno.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "steque.h"
#include <stdlib.h>

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//

typedef struct worker_args_t{
  gfcontext_t **ctx;
} worker_args_t;


steque_t tasks;
pthread_mutex_t m_tasks; // protect the tasks
pthread_cond_t c_boss;

void* worker(void* arg) {
  worker_args_t* args = (worker_args_t*)arg;
  struct timespec timeout_time;
  clock_gettime(CLOCK_REALTIME, &timeout_time); // Get current time
  timeout_time.tv_sec += 30; // Add 3 seconds to the current time

  while (1) {
    pthread_mutex_lock(&m_tasks);
    while (steque_isempty(&tasks)) {
      pthread_cond_wait(&c_boss, &m_tasks);
    }
    // acquired locks to tasks
    steque_item item = steque_pop(&tasks);
    pthread_mutex_unlock(&m_tasks);
    
    int fd = (int)*((int*)item);
    // try to acquire lock for fd for 3s;
    pthread_mutex_t* m_fd = fdlock_get(fd);
    int ret = pthread_mutex_timedlock(m_fd, &timeout_time);
    if (ret == ETIMEDOUT) {
      // cant get fd lock
      // move fd to back of the queue and continue waiting for another task
      steque_enqueue(&tasks, item);
      continue;
    } else {
      // acquired lock to fd
      // reset fd to start of file
    // SEEK_SET indicates that the offset is relative to the beginning of the file
      off_t new_offset = lseek(fd, 0, SEEK_SET);
      if (new_offset == (off_t)-1) {
        perror("Error seeking to beginning of file");
        steque_enqueue(&tasks, item);
        pthread_mutex_unlock(m_fd);
        continue;
      }
      // get file stats by calling fstats
      struct stat file_stat;
      if (fstat(fd, &file_stat) == -1) {
          perror("Error getting file status");
          // design decision: not to re-enqueue the request since the fd has problem
          pthread_mutex_unlock(m_fd);
          continue;
      }
      long file_len = file_stat.st_size;
      printf("File length: %ld bytes\n", file_len);
      // call header to send file len
      gfs_sendheader(args->ctx, GF_OK, file_len);
      //send all file at once -> easy -> let TCP handle packet fragmentation
      char* buf = (char*) malloc(file_len);
      if (0 > read(fd, buf, (size_t)file_len)) {
        perror("error reading file");
        // design decision: not to re-enqueue the request since the fd has problem
        pthread_mutex_unlock(m_fd);
        continue;
      }
      // Read file succesfully to buffer, send everything to client
      if (0 > gfs_send(args->ctx, buf, file_len)) {
        fprintf(stderr, "error sending file to client. Abort request\n");
      }
      // release fd lock after download complete
      pthread_mutex_unlock(m_fd);
    }
  }

}

gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
  // student implemented.
  // arg must include: nthreads, content_map, 
  handler_args_t* args = (handler_args_t*) arg;
  int fd;
  // initialize results
  pthread_mutex_init(&m_tasks, NULL);
  pthread_cond_init(&c_boss, NULL);
  // initialize worker pool
  worker_args_t worker_args;
  worker_args.ctx = ctx;
  pthread_t* thread_pool = (pthread_t*)malloc(args->nthreads * sizeof(pthread_t));
  fprintf(stdout, "Starting %d threads\n", args->nthreads);
  for (int i=0; i<args->nthreads; i++) {
    pthread_create(&thread_pool[i], NULL, worker, &worker_args);
  }

  // check if path in content map
  // if exists, enqueue the fd to thread pool
  if ((fd = content_get(path)) == -1) {
    fprintf(stderr, "File %s not exist\n", path);
    // TODO: send FILE_NOT_FOUND
  } else {
    // enqueue task
    steque_enqueue(&tasks, &fd);
    pthread_cond_signal(&c_boss);
  }

  return gfh_failure;
}

