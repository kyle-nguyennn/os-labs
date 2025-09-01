#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/socket.h>

#define BUFSIZE 512

#define USAGE                                                \
  "usage:\n"                                                 \
  "  transferclient [options]\n"                             \
  "options:\n"                                               \
  "  -s                  Server (Default: localhost)\n"      \
  "  -p                  Port (Default: 29345)\n"            \
  "  -o                  Output file (Default cs6200.txt)\n" \
  "  -h                  Show this help message\n"           \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"output", required_argument, NULL, 'o'},
    {"server", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    unsigned short portno = 29345;
    char *hostname = "localhost";
    char *filename = "cs6200.txt";

    setbuf(stdout, NULL);

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:o:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 's': // server
            hostname = optarg;
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'o': // filename
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    if (NULL == hostname) {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }

    // Instead of using sockaddr_in as in echoclient, we are going to use addrinfo
    // to support both IPv4 and IPv6 addresses
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // convert portno to string because getaddrinfo takes in port number as string
    char portstr[6]; // port number is unsigned short (0-65535) -> 5 chars + terminating null so 6 bytes
    snprintf(portstr, sizeof(portstr), "%hu", portno);

    struct addrinfo *res;
    int gai_rc;
    if ((gai_rc = getaddrinfo(hostname, portstr, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_rc));
        close(fd);
        exit(1);
    }

    // Try to connect to server
    if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect");
        freeaddrinfo(res);
        close(fd);
        exit(1);
    }
    // Now we don't need addrinfo anymore, can free up res
    freeaddrinfo(res);

    // Start recv from server
    int total_len = 0;
    ssize_t n_recv;
    char buffer[BUFSIZE];
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "Error opening file %s\n", filename);
        close(fd);
        exit(1);
    }
    while ((n_recv = recv(fd, buffer, sizeof(buffer), 0)) != -1) {
        total_len += n_recv;
        size_t bytes_written = fwrite(buffer, 1, n_recv, fp);
        if (bytes_written != n_recv) {
            perror("Error writing to file");
            fclose(fp);
            close(fd);
        }
    }
    fclose(fp);
    printf("File received.");
    close(fd);
}
