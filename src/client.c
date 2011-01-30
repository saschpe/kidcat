/*
    Copyright 2008  Sascha Pelicke <sasch.pe@gmx.de>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _POSIX_SOURCE 1

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SERVER_LISTEN_PORT
#   define SERVER_LISTEN_PORT 9900
#endif
#ifndef BUFFER_SIZE
#   define BUFFER_SIZE 512
#endif
#define IPADDR_SIZE 15

static int client_sock, client_connected = 0;
static char buf[BUFFER_SIZE];

void sig_handler(int sig)
{
    int ret = EXIT_FAILURE;
    if (client_connected) {
        if (write(client_sock, "QUIT\r\n", 6) < 0) {
            fprintf(stderr, "Unable to quit the conversation on the server\n");
        } else {
            if (read(client_sock, buf, BUFFER_SIZE) < 0) {
                fprintf(stderr, "Unable to receive an acknowledge from the server\n");
            } else if (strncmp(buf, "ACK\r\n", 5) == 0) {
                ret = EXIT_SUCCESS;
            }
        }
    }
    shutdown(client_sock, 2); close(client_sock);
    exit(ret);
}

int main(int argc, char **argv)
{
    struct sigaction action; sigset_t sigs;
    struct sockaddr_in addr;
    struct hostent *he;
    int read_bytes;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [host]\n", argv[0]); exit(EXIT_FAILURE);
    }

    sigfillset(&sigs);                                          /* Mostly to keep valgrind happy */
    action.sa_flags = 0;
    action.sa_handler = sig_handler;                            /* Set up signal handler for some cleanup */
    action.sa_mask = sigs;                                      /* and housekeeping before we terminate */
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket()"); exit(EXIT_FAILURE);
    }
    if ((he = gethostbyname(argv[1])) == NULL) {
        perror("gethostbyname()"); exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Connecting to server %s ...\n", argv[1]);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_LISTEN_PORT);                  /* Set up listen socket */
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    if (connect(client_sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1) {
        perror("connect()"); exit(EXIT_FAILURE);
    } else {
        client_connected = 1;
    }

    fprintf(stdout, "Joining the conversation ...\n");
    if (write(client_sock, "JOIN\r\n", 6) < 0) {                /* Try to enter conversation */
        fprintf(stderr, "Unable to join the conversation on the server\n");
    } else {                                                    /* Test if the server responds positively */
        if (read(client_sock, buf, BUFFER_SIZE) < 0) {
            fprintf(stderr, "Unable to receive an acknowledge from the server\n");
            kill(getpid(), SIGTERM);
        } else if (strncmp(buf, "ACK\r\n", 5) != 0) {           /* If not we gently disconnect and shutdown */
            kill(getpid(), SIGTERM);
        }
    }
    fprintf(stdout, "Ready, feel free to start writing ...\n");

    while (2) {
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(client_sock, &fds);

        if (select(client_sock + 1, &fds, NULL, NULL, NULL) == -1) {
            fprintf(stderr, "Error in select() syscall\n");
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            fflush(stdout);
            if ((read_bytes = read(STDIN_FILENO, buf, BUFFER_SIZE)) < 0) {
                fprintf(stderr, "Unable to fetch user input ...\n"); continue;
            } else if (write(client_sock, buf, read_bytes) < 0) {
                fprintf(stderr, "Unable to send the message to the server\n");
            }
        } else if (FD_ISSET(client_sock, &fds)) {
            if ((read_bytes = read(client_sock, buf, BUFFER_SIZE)) < 0) {
                fprintf(stderr, "Unable to fetch server message ...\n"); continue;
            } else if (write(STDOUT_FILENO, buf, read_bytes) < 0) {
                fprintf(stderr, "Unable to send the message to the server\n");
            }
        }
    }
    return EXIT_SUCCESS;
}
