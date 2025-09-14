
#include "gfclient.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "gfclient-student.h"

 // Modify this file to implement the interface specified in
 // gfclient.h.

#define BUFSIZE 512
#define SCHEME "GETFILE" // 7 bytes
#define METHOD "GET"     // 3 bytes

struct gfcrequest_t {
  const char* server;
  unsigned short port;
  const char* path;
  gfstatus_t status;
  int header_len;
  size_t file_len;
  size_t bytesreceived;

  // function pointers for registering callbacks
  void (*headerfunc)(void* data, size_t len, void* arg);
  void* headerarg;

  void (*writefunc)(void* data, size_t len, void* arg);
  void* writearg;

};

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
  free(*gfr);
}

/*
 * Default header func: assume the full header fits within one message
 * After reading header the req->bytesreceived should point to the first byte of the payload
 */ 

void headerfunc(void* data, size_t len, void* arg) {
  gfcrequest_t* req = (gfcrequest_t*) arg;
  printf("header length: %ld\n", len);
  if (!data || (len < strlen(SCHEME)+2)) return;
  // parse status
  int start = strlen(SCHEME) + 1;
  int i;
  for (i=start; i<len && *((char*)data+i)!= ' '; ++i);
  char* strstatus = (char*)malloc(i-start+1);
  strncpy(strstatus, (char*)data + start, i-start);
  strstatus[i-start] = '\0';
  printf("Server status: %s\n", strstatus);
  req->status = gfstatus_from_str(strstatus);
  // parse file len
  start = i+1;
  for (i=start;i<len && *((char*)data+i)!= '\r'; ++i);
  char* len_str = (char*)malloc(i-start+1);
  strncpy(len_str, (char*)data+start, i-start);
  len_str[i-start] = '\0';
  req->file_len = atoi(len_str);
  printf("File length: %ld\n", req->file_len);
  req->header_len = i+4; // plus 4 bytes ending the header
  printf("Header len=%d\n", req->header_len);
  free(strstatus);
  free(len_str);
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  // student implemented
  return (*gfr)->file_len;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  // student implemented
  return (*gfr)->bytesreceived;
}

gfcrequest_t *gfc_create() {
  // student implemented
  gfcrequest_t *req = (gfcrequest_t*)malloc(sizeof(gfcrequest_t));
  req->headerfunc = headerfunc;
  req->headerarg = req;
  req->bytesreceived = 0;
  req->file_len = 0;
  req->header_len = 0;
  return req;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  return (*gfr)->status;
}

void gfc_global_init() {}

void gfc_global_cleanup() {}

int gfc_perform(gfcrequest_t **gfr) {
  // not yet implemented
  size_t req_len = 7 + 1 + 3 + 1 + strlen((*gfr)->path) + 4;
  char* message = (char*)malloc(req_len*sizeof(char));
  snprintf(message, req_len, "%s %s %s\r\n\r\n", SCHEME, METHOD, (*gfr)->path);
  printf("Sending request to server: %s\n", message);
  // TODO: below steps
  // Open socket connection to server
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
      perror("socket");
      exit(1);
  }
  // addrinfo to support both IPv4 and IPv6 addresses
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  // convert portno to string because getaddrinfo takes in port number as string
  char portstr[6]; // port number is unsigned short (0-65535) -> 5 chars + terminating null so 6 bytes
  snprintf(portstr, sizeof(portstr), "%hu", (*gfr)->port);

  struct addrinfo *res;
  int gai_rc;
  if ((gai_rc = getaddrinfo((*gfr)->server, portstr, &hints, &res)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_rc));
      close(server_fd);
      exit(1);
  }

  // Try to connect to server
  if (connect(server_fd, res->ai_addr, res->ai_addrlen) == -1) {
      perror("connect");
      freeaddrinfo(res);
      close(server_fd);
      exit(1);
  }
  // Now we don't need addrinfo anymore, can free up res
  freeaddrinfo(res);
  // Send request through connection and wait for reponse
  if (send(server_fd, message, req_len, 0) < 0) {
    // TODO: could do retry here
    perror("send");
    close(server_fd);
    exit(1);
  }
  free(message);
  // Handle first response by calling (*gfr)->headerfunc
  char buffer[BUFSIZE];
  ssize_t n_recv;
  if ((n_recv=recv(server_fd, buffer, sizeof(buffer), 0)) < 0) {
    perror("recv");
    close(server_fd);
    exit(1);
  }
  (*gfr)->headerfunc(buffer, n_recv, *gfr);
  printf("Processed %d bytes from server header\n", (*gfr)->header_len);

  // Handle the rest of the message if more than header was sent
  if ((*gfr)->header_len < n_recv) {
    printf("Handle the rest of %ld bytes in the first message\n", n_recv-(*gfr)->header_len);
    int offset = (*gfr)->header_len;
    (*gfr)->writefunc(buffer+offset, n_recv-offset, (*gfr)->writearg);
    (*gfr)->bytesreceived = n_recv-(*gfr)->header_len;
  }

  // Handle data stream while server still sending by calling (*gfr)->writefunc
  while ((n_recv=recv(server_fd, buffer, sizeof(buffer), 0)) > 0) {
    printf("Received %ld bytes from server\n", n_recv);
    ((*gfr)->bytesreceived) += n_recv;
    (*gfr)->writefunc(buffer, n_recv, (*gfr)->writearg);
    printf("Processed total of %ld bytes\n", (*gfr)->bytesreceived);
    if ((*gfr)->bytesreceived >= (*gfr)->file_len) break;
  }
  if (n_recv < 0) {
    perror("recv");
    // TODO: how to interpret errno
    close(server_fd);
    exit(1);
  }
  printf("File received.\n");
  close(server_fd);
  return 0;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
  (*gfr)->port = port;
}

void gfc_set_server(gfcrequest_t **gfr, const char *server) {
  (*gfr)->server = server;
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *)) {
  (*gfr)->headerfunc = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {
  (*gfr)->headerarg = headerarg;
}

void gfc_set_path(gfcrequest_t **gfr, const char *path) {
  (*gfr)->path = path;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *)) {
  (*gfr)->writefunc = writefunc;
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
  (*gfr)->writearg = writearg;
}

const char *gfc_strstatus(gfstatus_t status) {
  const char *strstatus = "UNKNOWN";

  switch (status) {

    case GF_OK: {
      strstatus = "OK";
    } break;

    case GF_FILE_NOT_FOUND: {
      strstatus = "FILE_NOT_FOUND";
    } break;

   case GF_INVALID: {
      strstatus = "INVALID";
    } break;
   
   case GF_ERROR: {
      strstatus = "ERROR";
    } break;

  }

  return strstatus;
}

gfstatus_t gfstatus_from_str(const char* str) {
  if (!str) return GF_INVALID;
  if (strcmp(str, "OK") == 0) return GF_OK;
  if (strcmp(str, "FILE_NOT_FOUND") == 0) return GF_FILE_NOT_FOUND;
  if (strcmp(str, "INVALID") == 0) return GF_INVALID;
  if (strcmp(str, "ERROR") == 0) return GF_ERROR;
  return GF_INVALID;
}
