#include "gfserver.h"
#include "cache-student.h"

#define BUFSIZE (840)

/*
 __.__
Replace with your implementation
 __.__
*/
ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg){
    size_t bytes_transferred = 0;
	// size_t file_len;
	int *thread_id = arg;
	// ssize_t read_len;
    // ssize_t write_len;
	// char buffer[BUFSIZE];
	// int fildes;
	// struct stat statbuf;
    
    printf("Thread %d handling request for %s\n", *thread_id, path);
    


	return bytes_transferred;
}
