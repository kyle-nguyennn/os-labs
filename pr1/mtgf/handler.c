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


gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
  // check if path in content map
  // if exists, enqueue the fd to thread pool
  int fd;
  if ((fd = content_get(path)) == -1) {
    fprintf(stderr, "File %s not exist\n", path);
    // TODO: send FILE_NOT_FOUND
  } else {
    // enqueue task
    fprintf(stdout, "New task: %d\n", fd);
    task_item* item = malloc(sizeof(*item));
	item->fd = fd;
	item->ctx = *ctx;
    steque_enqueue(&tasks, item);
    pthread_cond_signal(&c_boss);
  }
  *ctx = NULL;

  return gfh_success;
}

