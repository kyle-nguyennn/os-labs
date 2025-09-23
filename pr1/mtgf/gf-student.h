/*
 *  This file is for use by students to define anything they wish.  It is used by both the gf server and client implementations
 */
#ifndef __GF_STUDENT_H__
#define __GF_STUDENT_H__

#include <errno.h>
#include <stdio.h>
#include <regex.h>
#include <string.h>
#include <unistd.h>
#include <resolv.h>
#include <netdb.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <pthread.h>


typedef struct handler_args handler_args_t;
// get mutex for a file descriptor. If new fd, will initialize the mutex and return
pthread_mutex_t* fdlock_get(int fd);

 #endif // __GF_STUDENT_H__
