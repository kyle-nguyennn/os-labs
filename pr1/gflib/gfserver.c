
#include <stdlib.h>
#include "gfserver.h"
#include <sys/socket.h>
#include <string.h>
#include <stdbool.h>
#include "gfserver-student.h"

// Modify this file to implement the interface specified in
// gfserver.h.

typedef struct gfrequest_t gfrequest_t;
struct gfrequest_t {
    char* scheme;
    char* method;
    char* path;
};


void print_request(struct gfrequest_t* request) {
    printf("Request: [scheme=%s; method=%s; path=%s]\n", request->scheme, request->method, request->path);
}

struct gfserver_t {
    unsigned short port;
    int max_npending;
    
    // function pointer for callback
    gfh_error_t (*handler)(gfcontext_t**, const char*, void*);
    void* handlerarg;
};

struct gfcontext_t {
    int client_fd;
};

void gfs_abort(gfcontext_t **ctx){
}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len){
    // student implemented
    return send((*ctx)->client_fd, data, len, 0);
}

void gfstatus_str(gfstatus_t status, char* strstatus) {
    switch (status) {
        case 200: sprintf(strstatus, "OK"); break;
        case 400: sprintf(strstatus, "FILE_NOT_FOUND"); break;
        case 500: sprintf(strstatus, "ERROR"); break;
        case 600: sprintf(strstatus, "INVALID"); break;
        default: sprintf(strstatus, "INVALID");
    }
}

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len){
    // student implemented
    char strstatus[19];
    gfstatus_str(status, strstatus);
    char file_len_str[21];
    sprintf(file_len_str, "%ld", file_len);
    int header_len = strlen(SCHEME) + 1 + strlen(strstatus) + ((status==GF_OK)?(strlen(file_len_str)+1):0) + 4;
    char* header = (char*)malloc(header_len+1); // since sprintf automatically adds \0
    if (status == GF_OK) sprintf(header, "%s %s %s\r\n\r\n", SCHEME, strstatus, file_len_str);
    else sprintf(header, "%s %s\r\n\r\n", SCHEME, strstatus);
    printf("header: len=%d\n%s\n", header_len, header);
    return send((*ctx)->client_fd, header, header_len, 0);
}

gfserver_t* gfserver_create(){
    // student implemented
    gfserver_t* gfserver = (gfserver_t*)malloc(sizeof(gfserver_t));
    gfserver->port = 8080;
    gfserver->max_npending = 1;
    gfserver->handler = NULL;
    gfserver->handlerarg = NULL;
    return gfserver;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port){
    (*gfs)->port = port;
}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending){
    (*gfs)->max_npending = max_npending;
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void*)){
    (*gfs)->handler = handler;
}

void gfserver_set_handlerarg(gfserver_t **gfs, void* arg){
    (*gfs)->handlerarg = arg;
}

int parse_request(gfrequest_t* req, char* buffer) {
    // Assume client's request respect specs: GETFILE GET <path>\r\n\r\n
    // buffer is a null-terminated string
    printf("Client request: %s\n", buffer);
    // Checking and of request
    char* header_end;
    if ((header_end = strstr(buffer, "\r\n\r\n")) == NULL) {
        fprintf(stderr, "Request header does not end with CR LF CR LF\n");
        return -1;
    }
    if ((header_end - buffer) != strlen(buffer)-4) {
        fprintf(stderr, "File path contains CR LF CR LF\n");
        return -1;
    }
    buffer[strlen(buffer) - 4] = '\0'; // manually removed \r\n\r\n

    char* token;
    char* saveptr;
    token = strtok_r(buffer, " ", &saveptr);
    int cnt = 0;
    char* parsed[3]; // request should have exactly 3 parts
    while (token != NULL) {
        if (cnt == 0) {
            if (strcmp(token, SCHEME)) {
                fprintf(stderr, "Invalid scheme: %s\n", token);
                return -1;
            }
        }
        if (cnt == 1) {
            if (strcmp(token, METHOD)) {
                fprintf(stderr, "Invalid method: %s\n", token);
                return -1;
            }
        }
        if (cnt == 2) {
            if (token[0] != '/') {
                fprintf(stderr, "Invalid path: %s\n", token);
                return -1;
            }
            req->path = token;
        }
        if (cnt == 3) {
            fprintf(stderr, "Header has more than 3 parts\n");
            return -1;
        }
        parsed[cnt++] = token; 
        // printf("token: %s\n", token);
        token = strtok_r(NULL, " ", &saveptr);
    }
    if (cnt < 3) {
        fprintf(stderr, "Header has fewer than 3 parts\n");
        return -1;
    }
    req->scheme = parsed[0];
    req->method = parsed[1];

    return 0;
}

int gfserver_sendheader_not_ok(gfserver_t **gfs, int fd, gfstatus_t status) {
    char strstatus[19];
    gfstatus_str(status, strstatus);
    printf("Status: %s\n", strstatus);
    char* message = (char*)malloc(7 + 1 + strlen(strstatus) + 4 + 1);
    sprintf(message, "%s %s\r\n\r\n", SCHEME, strstatus);

    if (send(fd, message, strlen(message), 0) < 0) {
        perror("recv");
        close(fd);
        free (message);
        return -1;
    }
    free(message);
    return 0;
}

void gfserver_serve(gfserver_t **gfs){
    // start up server and listen
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((*gfs)->port);

    int yes=1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 1) != 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    // Accepting connection
    char buffer[BUFSIZE];
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_addr_len);
        size_t n_recv = recv(client_fd, buffer, BUFSIZE, 0);
        if (n_recv < 0) {
            perror("recv");
            close(client_fd);
        }
        buffer[n_recv] = '\0';
        printf("Received request from client %d: %s\n", client_fd, buffer);
        // parse request, assuming request comes in 1 message -> might be false
        gfrequest_t request;
        if (parse_request(&request, buffer) == -1) {
            if (gfserver_sendheader_not_ok(gfs, client_fd, GF_INVALID) < 0) {
                fprintf(stderr, "Error sending\n");
            }
            close(client_fd);
            continue;
        }
        print_request(&request);
        gfcontext_t* ctx = (gfcontext_t*)malloc(sizeof(gfcontext_t));
        // call handler to get file from path
        ctx->client_fd = client_fd;
        (*gfs)->handler(&ctx, request.path, (*gfs)->handlerarg);
    }
}



