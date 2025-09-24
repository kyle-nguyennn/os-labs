/*
 *  This file is for use by students to define anything they wish.  It is used by both the gf server and client implementations
 */

#include <stdlib.h>
#include "gf-student.h"

pthread_mutex_t** fdlock_registry;
size_t cap = 0;

pthread_mutex_t* fdlock_get(int fd){
  if (fdlock_registry == NULL) {
    // first time init registry: default to 1024 slots
    cap = 1024;
    printf("Init fdlock_registry to %ld slots\n", cap);
    fdlock_registry = calloc(cap, sizeof(pthread_mutex_t*));
  }
  if (fd > cap) {
    // TODO: re-alloc registry, x2 size
    while (cap <= fd) cap <<= 1;
    void* newm = realloc(fdlock_registry, cap*sizeof(pthread_mutex_t*));
    if (newm != NULL) {
      fdlock_registry = (pthread_mutex_t**) newm;
    } else {
      fprintf(stderr, "Extending fdlock_registry failed, no more space for fd lock\n");
      return NULL;
    }
  }
  if (fdlock_registry[fd] == NULL) {
    // TODO: acquire m_registry to avoid 2 thread init lock at the same time
    fdlock_registry[fd] = malloc(sizeof(pthread_mutex_t**));
    pthread_mutex_init(fdlock_registry[fd], NULL);
  }
  return fdlock_registry[fd];
}
