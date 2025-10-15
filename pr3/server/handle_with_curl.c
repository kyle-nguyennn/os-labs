#include "proxy-student.h"
#include "gfserver.h"
#include <curl/curl.h>
#include <stddef.h>



#define MAX_REQUEST_N 512
#define BUFSIZE (6426)


static size_t forward_callback(void *ptr, size_t size, size_t nmemb, void *ctx)
{
    size_t realsize = size * nmemb;
    ssize_t bytes_sent = gfs_send((gfcontext_t*) ctx, ptr, realsize);

    return bytes_sent;
}

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
    printf("Requesting full path: %s\n", full_path);
    CURL * handle;
    CURLcode statusCode;
    handle = curl_easy_init();
    // get header first
    curl_easy_setopt(handle, CURLOPT_URL, full_path);
    curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
    statusCode = curl_easy_perform(handle);
    long http_code = 0;
    curl_off_t content_length = -1;

    if (statusCode != CURLE_OK) {
        // Handle error
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(statusCode));
        curl_easy_cleanup(handle);
    } else {
        statusCode = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
        printf("http code = %ld\n", http_code);
        if (statusCode == CURLE_OK) {
            if (http_code == 404 || http_code == 400) {
                printf("FILE NOT FOUND.\n");
                gfs_sendheader(ctx, GF_FILE_NOT_FOUND, -1);
                curl_easy_cleanup(handle);
                return -1;
            } else {
                statusCode = curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
                if (statusCode != CURLE_OK) {
                    fprintf(stderr, "curl_easy_getinfo() failed: %s\n", curl_easy_strerror(statusCode));
                    curl_easy_cleanup(handle);
                }
                printf("File size: %lld bytes\n", (long long)content_length);
                gfs_sendheader(ctx, GF_OK, (size_t) content_length);
            }
        } else {
            // Handle error getting info
            fprintf(stderr, "curl_easy_getinfo() failed: %s\n", curl_easy_strerror(statusCode));
            curl_easy_cleanup(handle);
        }
    }
    
    handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_URL, full_path);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, forward_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, ctx);

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
