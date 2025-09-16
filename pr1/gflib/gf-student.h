/*
 *  This file is for use by students to define anything they wish.  It is used by both the gf server and client implementations
 */
#ifndef __GF_STUDENT_H__
#define __GF_STUDENT_H__

#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <regex.h>
#include <resolv.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define max(a,b) (a>b)?a:b
#define BUFSIZE 512
#define SCHEME "GETFILE" // 7 bytes
#define METHOD "GET"     // 3 bytes

 #endif // __GF_STUDENT_H__
