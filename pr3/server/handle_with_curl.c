#include "proxy-student.h"
#include "gfserver.h"
#include <stddef.h>



#define MAX_REQUEST_N 512
#define BUFSIZE (6426)

ssize_t handle_with_curl(gfcontext_t *ctx, const char *path, void* arg){
	(void) ctx;
	(void) arg;
	(void) path;
	//Student implementation here
    char* server = (char*) arg;
    size_t full_len = strlen(server) + strlen(path) + 1;
    printf("Server %s\n", server);
    printf("Requesting file %s\n", path);
    char* full_path = malloc(full_len);
    strcpy(full_path, server);
    strcpy(full_path+strlen(server), path);
    CURL * handle;
    CURLcode statusCode;
    handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_URL, full_path);
    statusCode = curl_easy_perform(handle);
    printf("Status = %d\n", statusCode);
    free(full_path);
    curl_easy_cleanup(handle);
    return 0;
}

/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl as a convenience for linking!
 */
ssize_t handle_with_file(gfcontext_t *ctx, const char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}	
