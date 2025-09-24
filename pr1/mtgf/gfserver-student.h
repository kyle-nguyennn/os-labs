/*
 *  This file is for use by students to define anything they wish.  It is used by the gf server implementation
 */
#ifndef __GF_SERVER_STUDENT_H__
#define __GF_SERVER_STUDENT_H__

#include "gf-student.h"
#include "gfserver.h"
#include "content.h"
#include "steque.h"

typedef struct handler_args handler_args_t;

typedef struct {
	int fd;
	gfcontext_t* ctx;
} task_item;


extern steque_t tasks;
extern pthread_mutex_t m_tasks; // protect the tasks
extern pthread_cond_t c_boss;

void init_threads(size_t numthreads);
void cleanup_threads();

#ifdef GFSERVER_PRIVATE
struct handler_args {
  int nthreads;
};
#endif


#endif // __GF_SERVER_STUDENT_H__
