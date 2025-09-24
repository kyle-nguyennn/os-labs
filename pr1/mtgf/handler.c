#define GFSERVER_PRIVATE
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
#include <stdbool.h>

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//


//typedef struct worker_args_t{
//  gfcontext_t **ctx;
//} worker_args_t;


//steque_t tasks;
//pthread_mutex_t m_tasks; // protect the tasks
//pthread_cond_t c_boss;


gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
  // student implemented.
  // arg must include: nthreads, content_map, 
  // handler_args_t* args = (handler_args_t*) arg;
  // int fd;
  // initialize results
  //pthread_mutex_init(&m_tasks, NULL);
  //pthread_cond_init(&c_boss, NULL);
  //worker_args_t worker_args;
  //worker_args.ctx = ctx;
  //pthread_t* thread_pool = (pthread_t*)malloc(args->nthreads * sizeof(pthread_t));
  //fprintf(stdout, "Starting %d threads\n", args->nthreads);
  //for (int i=0; i<args->nthreads; i++) {
  //  pthread_create(&thread_pool[i], NULL, worker, &worker_args);
  //}

  // check if path in content map
  // if exists, enqueue the fd to thread pool
	int fd;
  if ((fd = content_get(path)) == -1) {
    fprintf(stderr, "File %s not exist\n", path);
    // TODO: send FILE_NOT_FOUND
  } else {
    // enqueue task
    fprintf(stdout, "New task: %d\n", fd);
		task_item item;
		item.fd = fd;
		item.ctx = ctx;
    steque_enqueue(&tasks, &item);
    pthread_cond_signal(&c_boss);
    //for (int i=0; i<args->nthreads; i++) {
    //  pthread_join(thread_pool[i], NULL);
    //}
  }

  return gfh_failure;
}

