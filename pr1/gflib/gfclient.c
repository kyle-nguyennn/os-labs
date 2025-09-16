#include "gfclient.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>

#include "gfclient-student.h"

 // Modify this file to implement the interface specified in
 // gfclient.h.

#define max(a,b) (a>b)?a:b
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

gfcrequest_t *gfc_create() {
  // student implemented
  gfcrequest_t *req = (gfcrequest_t*)malloc(sizeof(gfcrequest_t));
  req->server = NULL;
  req->port = 0;
  req->path = NULL;
  req->status = GF_INVALID;
  req->header_len = 0;
  req->file_len = 0;
  req->bytesreceived = 0;
  req->headerfunc = headerfunc;
  req->headerarg = req;
  req->writefunc = NULL;
  req->writearg = NULL;
  return req;
}

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
  if (!gfr || !*gfr) return;
  free(*gfr);
  *gfr = NULL;
}

/*
 * Default header func: assume the full header fits within one buffer
 */ 
void headerfunc(void* data, size_t len, void* arg) {
  char* header = (char*) data;
  printf("Header: %s\n", header);
  gfcrequest_t* req = (gfcrequest_t*) arg;
  printf("header length: %ld\n", len);
  // Manually remove \r\n\r\n at the end
  header[len-4] = '\0';
  // Server's header must respect specs: GETFILE <status> <file_len>\r\n\r\n
  char* token;
  char* saveptr;
  token = strtok_r(header, " ", &saveptr);
  int cnt = 0;
  char* parsed[3]; // response OK should have exactly 3 parts
  while (token != NULL) {
    if (cnt == 0) {
      if (strcmp(token, SCHEME)) {
        req->status = GF_INVALID;
        return;
      }
    }
    if (cnt == 1) {
      req->status = gfstatus_from_str(token);
    }
    if (cnt == 2) {
      if (req->status != GF_OK) {
        req->status = GF_INVALID;
        return;
      }
    }
    if (cnt == 3) {
      fprintf(stderr, "Header has more than 3 parts\n");
      req->status = GF_INVALID;
      return;
    }
    parsed[cnt++] = token; 
    // printf("token: %s\n", token);
    token = strtok_r(NULL, " ", &saveptr);
  }
  if (req->status == GF_OK){
    if (cnt < 3) {
      fprintf(stderr, "Header OK has less than 3 parts\n");
      req->status = GF_INVALID;
      return;
    }
    char *invalid_ptr;
    req->file_len = strtol(parsed[2], &invalid_ptr, 10);
    if (errno == ERANGE) {
      fprintf(stderr, "Conversion of '%s' resulted in overflow/underflow.\n", parsed[2]);
      req->status = GF_INVALID;
      return;
    }
    if (invalid_ptr < parsed[2]+strlen(parsed[2])){
      req->status = GF_INVALID;
      if (invalid_ptr == parsed[2]) { // No conversion performed
          fprintf(stderr, "Invalid conversion: '%s' -> No digits found.\n", parsed[2]);
      } else {
          fprintf(stderr, "Partial conversion: '%s' -> %ld (stopped at '%s')\n", parsed[2], req->file_len, invalid_ptr);
      }
    }
  }
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  // student implemented
  return (*gfr)->file_len;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  // student implemented
  return (*gfr)->bytesreceived;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  return (*gfr)->status;
}

void gfc_global_init() {}

void gfc_global_cleanup() {}

int gfc_perform(gfcrequest_t **gfr) {
  size_t req_len = 7 + 1 + 3 + 1 + strlen((*gfr)->path) + 4; // SCHEME + space + METHOD + space + path + CRLFCRLF
  char* message = (char*)malloc(req_len+1); // snprintf adds trailing \0
  if (!message) {
    fprintf(stderr, "malloc failed for request message\n");
    return -1;
  }
  snprintf(message, req_len+1, "%s %s %s\r\n\r\n", SCHEME, METHOD, (*gfr)->path);
  printf("Sending request to server: %s\n", message);

  // Prepare hints for dual-stack resolution (IPv4 & IPv6)
  struct addrinfo hints; 
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;      // Allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP
  hints.ai_flags = 0;

  char portstr[6];
  snprintf(portstr, sizeof(portstr), "%hu", (*gfr)->port);

  struct addrinfo *res = NULL, *p = NULL;
  int gai_rc = getaddrinfo((*gfr)->server, portstr, &hints, &res);
  if (gai_rc != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_rc));
    free(message);
    return -1;
  }

  int server_fd = -1;
  for (p = res; p != NULL; p = p->ai_next) {
    server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (server_fd < 0) {
      // Try next address
      continue;
    }

    // If IPv6, attempt to allow IPv4-mapped (dual-stack) when possible.
#ifdef IPPROTO_IPV6
    if (p->ai_family == AF_INET6) {
      int off = 0; // 0 => allow both (if system default is v6-only we clear it)
      (void)setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    }
#endif

    if (connect(server_fd, p->ai_addr, p->ai_addrlen) == 0) {
      break; // success
    }
    perror("connect");
    close(server_fd);
    server_fd = -1;
  }

  if (server_fd < 0) {
    fprintf(stderr, "Failed to connect to any resolved address\n");
    freeaddrinfo(res);
    free(message);
    return -1;
  }
  printf("Connected\n");
  freeaddrinfo(res);

  if (send(server_fd, message, req_len, 0) < 0) {
    perror("send");
    close(server_fd);
    free(message);
    return -1;
  }
  free(message);
  // Handle first response by calling (*gfr)->headerfunc
  char buffer[BUFSIZE];
  char* header;
  int offset=0;
  bool full_header=false;
  char *header_end;
  ssize_t n_recv;
  // Handle fragmented header. Read until detect \r\n\r\n. 
  // Assuming buffer size is large enough for full header, i.e. header len < 512
  while (!full_header) {
    if ((n_recv=recv(server_fd, buffer+offset, sizeof(buffer)-offset-1, 0)) < 0) {
      perror("recv");
      close(server_fd);
      return -1;
    }
    if (n_recv == 0) { // handle server closed connection prematurely
        fprintf(stderr, "Connection closed prematurely: expected %zu bytes, received %zu\n",
                (*gfr)->file_len, (*gfr)->bytesreceived);
        close(server_fd);
        (*gfr)->status=GF_INVALID;
        return -1; // signal error to caller
    }
    buffer[offset+n_recv] = '\0'; // temporarily term the string to search for pattern
    if ((header_end = strstr(buffer+(max(offset-3,0)), "\r\n\r\n")) != NULL) {
      header_end += 3; // at the last \n
      (*gfr)->header_len = (header_end-buffer)+1;
      header = (char*)malloc((*gfr)->header_len+1);
      memcpy(header, buffer, (*gfr)->header_len);
      header[(*gfr)->header_len] = '\0';
      offset = (buffer+offset+n_recv) - 1 - header_end;
      if (offset) memmove(buffer, (header_end+1), offset);
      full_header = true;
    } else {
      offset += n_recv;
    }
    if (offset >= BUFSIZE-2) {
      printf("Header larger than %d, malformed\n", BUFSIZE);
      (*gfr)->status = GF_INVALID;
      return -1;
    }
  }
  (*gfr)->headerfunc(header, (*gfr)->header_len, (*gfr)->headerarg);
  printf("Processed %d bytes from server header. Left with %d bytes unprocessed\n", 
          (*gfr)->header_len, offset);
  free(header);

  if ((*gfr)->status == GF_INVALID) {
    close(server_fd);
    return -1;
  }
  if ((*gfr)->status != GF_OK) return 0; // For ERROR and FILE_NOT_FOUND

  // Handle data stream while server still sending by calling (*gfr)->writefunc
  n_recv=0;
  while ((*gfr)->bytesreceived < ((*gfr)->file_len)) {
    ((*gfr)->bytesreceived) += (offset + n_recv);
    if ((offset + n_recv) > 0) {
      (*gfr)->writefunc(buffer, offset + n_recv, (*gfr)->writearg);
    }
    offset = 0; // reset offset after process all bytes received in buffer
    printf("Processed total of %ld bytes\n", (*gfr)->bytesreceived);
    if ((*gfr)->bytesreceived >= ((*gfr)->file_len)) break;
    if ((n_recv=recv(server_fd, buffer+offset, sizeof(buffer)-offset, 0)) < 0) {
      perror("recv");
      close(server_fd);
      return -1;
    }
    if (n_recv == 0) { // premature close or exactly end-of-stream
      if ((*gfr)->bytesreceived < (*gfr)->file_len) {
        fprintf(stderr, "Connection closed prematurely: expected %zu bytes, received %zu\n",
                (*gfr)->file_len, (*gfr)->bytesreceived);
        close(server_fd);
        return -1; // signal error to caller
      }
      break; // normal EOF after full file
    }
    printf("Received %ld bytes from server\n", n_recv);
  }
  if (n_recv < 0) {
    perror("recv");
    // TODO: how to interpret errno
    close(server_fd);
    return -1;
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
