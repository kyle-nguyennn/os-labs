#include <asm-generic/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/socket.h>

#define BUFSIZE 512

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: 6200.txt)\n"   \
    "  -p                  Port (Default: 29345)\n"          \
    "  -h                  Show this help message\n"         \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int portno = 29345;             /* port to listen on */
    int option_char;
    char *filename = "6200.txt"; /* file to transfer */

    setbuf(stdout, NULL); // disable buffering

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:x", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'f': // file to transfer
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        }
    }


    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    
    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portno);

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

    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening file %s\n", filename);
        close(server_fd);
        exit(1);
    }

    char s[BUFSIZE];
    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_addr_len);
        memset(s, 0, sizeof(s));
        ssize_t bytes_sent = 0;
        int s_idx = 0;
        char c;
        while ((c = fgetc(fp)) != EOF) {
            s[s_idx++] = c;
            if (s_idx == BUFSIZE) {
                ssize_t n_sent = send(client_fd, &s, s_idx, 0);
                if (n_sent == -1) {
                    perror("send");
                }
                printf("sent %ld bytes to client\n", n_sent);
                printf("%.*s\n", BUFSIZE, s);
                bytes_sent += n_sent;
            }
            s_idx %= BUFSIZE;
        }
        if (s_idx > 0) { // send the partial buffer
            ssize_t n_sent = send(client_fd, &s, s_idx, 0);
            bytes_sent += n_sent;
            printf("sent %ld bytes to client\n", n_sent);
            printf("%.*s\n", (int)n_sent, s);
        }
        printf("Total bytes sent: %ld bytes\n", bytes_sent);
        close(client_fd);
        // Rewind file pointer
        fseek(fp, 0, SEEK_SET);
    }

    fclose(fp);
    close(server_fd);
}
