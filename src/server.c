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
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "list.h"

#ifndef SERVER_LISTEN_PORT
#   define SERVER_LISTEN_PORT 9900
#endif
#define SERVER_LISTEN_BACKLOG 5
#ifndef BUFFER_SIZE
#   define BUFFER_SIZE 512
#endif
#define IPADDR_SIZE 15

typedef struct client {
    int sock;                       /* Client socket */
    struct sockaddr_in addr;        /* Socket connection details */
    socklen_t socklen;
    enum client_state {
        CLIENT_ERROR,               /* Error state */
        CLIENT_ACCEPTED,            /* Client was accepted */
        CLIENT_JOINED,              /* Client joined conversation */
        CLIENT_QUIT                 /* Client quitted conversation */
    } state;
    char name[IPADDR_SIZE];         /* Printable name, eg. IP address */
    mqd_t mq;                       /* Message queue */
} client_t;

typedef struct message {
    int sock;                       /* Socket of sender */
    char sender[IPADDR_SIZE];       /* IP address of sender */
    char data[BUFFER_SIZE];         /* Message content */
} message_t;

static int server_sock;
static struct sockaddr_in server_addr;
static list_t client_list;

void client_message_callback(list_item_t *item, void *msg)
{
    client_t *client = (client_t *)item->data;
    message_t *message = (message_t*)msg;
    if (client->sock != message->sock)
        mq_send(client->mq, msg, sizeof(message_t), 1);
}

void client_quit_callback(list_item_t *item, void *data)
{
    mq_send(((client_t *)item->data)->mq, "", 0, 1);
}

void *thread_func(void *arg)
{
    int running = 1, read_bytes;
    struct mq_attr ma;
    char buf[BUFFER_SIZE];
    client_t *cl = (client_t *)((list_item_t *)arg)->data;  /* Extract list client data element */

    if (!arg) {
        fprintf(stderr, "Thread %ld: ERROR No client data argument\n", pthread_self());
        pthread_exit((void *)EXIT_FAILURE);
    }

    mq_getattr(cl->mq, &ma);
    fprintf(stdout, "Thread %ld: Message queue msgsize is %ld and maxmsg is %ld\n", pthread_self(), ma.mq_msgsize, ma.mq_maxmsg);

    while (running) {
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(cl->sock, &fds);                             /* Include client socket */
        FD_SET(cl->mq, &fds);                               /* Include thread message queue */

        if (select((cl->sock > cl->mq ? cl->sock : cl->mq) + 1, &fds, NULL, NULL, NULL) == -1) {
            fprintf(stderr, "Thread %ld: ERROR in select() syscall\n", pthread_self());
            continue;
        }

        switch (cl->state) {                                /* This little state-machine decides on */
            case CLIENT_ACCEPTED:                           /* client status how to handle i/o */
                if (FD_ISSET(cl->sock, &fds)) {
                    if ((read_bytes = read(cl->sock, buf, BUFFER_SIZE)) < 0) {
                        fprintf(stderr, "Thread %ld: ERROR Unable to read from client\n", pthread_self());
                        cl->state = CLIENT_ERROR;
                    } else {
                        buf[read_bytes] = '\0';
                        if (strncmp(buf, "JOIN\r\n", 6) == 0) {
                            cl->state = CLIENT_JOINED;
                            fprintf(stdout, "Thread %ld: Client %s joined conversation\n", pthread_self(), cl->name);
                            if (write(cl->sock, "ACK\r\n", 5) < 0) {
                                fprintf(stderr, "Thread %ld: ERROR Unable to write to client\n", pthread_self());
                                cl->state = CLIENT_ERROR;
                            }
                        } else if (strncmp(buf, "QUIT\r\n", 6) == 0) {
                            cl->state = CLIENT_QUIT;
                            fprintf(stdout, "Thread %ld: Client %s leaves connection\n", pthread_self(), cl->name);
                            if (write(cl->sock, "ACK\r\n", 5) < 0) {
                                fprintf(stderr, "Thread %ld: ERROR Unable to write to client\n", pthread_self());
                                cl->state = CLIENT_ERROR;
                            }
                        } else {
                            fprintf(stderr, "Thread %ld: Client %s sent unknown command '%s'\n", pthread_self(), cl->name, buf);
                        }
                    }
                } else if (FD_ISSET(cl->mq, &fds)) {
                    char *msg_data = (char *)malloc(ma.mq_msgsize);

                    if (mq_receive(cl->mq, msg_data, ma.mq_msgsize, NULL) == -1) {
                        fprintf(stderr, "Thread %ld: Error in mq_receive() syscall\n", pthread_self());
                    } else {
                        fprintf(stderr, "Thread %ld: Received message '%s' in state ACCEPTED...\n", pthread_self(), ((message_t *)msg_data)->data);
                        if (msg_data == 0) {
                            cl->state = CLIENT_QUIT;
                        }
                    }
                    free(msg_data);
                }
                break;
            case CLIENT_JOINED:
                if (FD_ISSET(cl->sock, &fds)) {
                    if ((read_bytes = read(cl->sock, buf, BUFFER_SIZE)) < 0) {
                        fprintf(stderr, "Thread %ld: ERROR Unable to read from client\n", pthread_self());
                        cl->state = CLIENT_ERROR;
                    } else {
                        buf[read_bytes] = '\0';
                        if (strncmp(buf, "QUIT\r\n", 6) == 0) {
                            cl->state = CLIENT_QUIT;
                            fprintf(stdout, "Thread %ld: Client %s leaved conversation\n", pthread_self(), cl->name);
                            if (write(cl->sock, "ACK\r\n", 5) < 0) {
                                fprintf(stderr, "Thread %ld: ERROR Unable to write to client\n", pthread_self());
                                cl->state = CLIENT_ERROR;
                            }
                        } else {
                            message_t msg;
                            strcpy(msg.sender, cl->name);
                            strcpy(msg.data, buf);
                            msg.sock = cl->sock;
                            fprintf(stdout, "Thread %ld: Client %s sent message '%s'\n", pthread_self(), msg.sender, msg.data);
                            list_apply(&client_list, client_message_callback, &msg);
                        }
                    }
                } else if (FD_ISSET(cl->mq, &fds)) {
                    char *msg_data = (char *)malloc(ma.mq_msgsize);

                    if (mq_receive(cl->mq, msg_data, ma.mq_msgsize, NULL) == -1) {
                        fprintf(stderr, "Thread %ld: Error in mq_receive() syscall\n", pthread_self());
                    } else {
                        fprintf(stderr, "Thread %ld: Received message '%s' ...\n", pthread_self(), ((message_t *)msg_data)->data);
                        if (msg_data == 0) {
                            cl->state = CLIENT_QUIT;
                        } else {
                            sprintf(buf, "%s: %s", ((message_t *)msg_data)->sender, ((message_t *)msg_data)->data);
                            fprintf(stdout, "Thread %ld: Send message to clients ...\n", pthread_self());
                            if (write(cl->sock, buf, strlen(buf)) < 0) {
                                fprintf(stderr, "Thread %ld: ERROR Unable to write to client\n", pthread_self());
                                cl->state = CLIENT_ERROR;
                            }
                        }
                    }
                    free(msg_data);
                }
                break;
            case CLIENT_ERROR:
            case CLIENT_QUIT:
                running = 0;
                break;
        }
    }

    list_remove(&client_list, (void *)arg);                         /* Remove this client from the client list */
    free(arg);

    shutdown(cl->sock, 2); close(cl->sock);                         /* Terminate and close client socket */
    sprintf(buf, "/%s:%d", cl->name, cl->sock);                     /* Close and unlink message queue */
    mq_close(cl->mq); mq_unlink(buf);                               /* Close message queue */
    fprintf(stdout, "Thread %ld: Closed connection for %s on socket %d\n", pthread_self(), cl->name, cl->sock);
    free(cl);
    pthread_exit((void *)EXIT_SUCCESS);
}

void sig_handler(int sig)
{
    fprintf(stdout, "Server: Disconnect all clients ...\n");
    list_apply((list_t *)&client_list, client_quit_callback, NULL); /* Call quit callback for every client in list */
    list_destroy((list_t *)&client_list);
    close(server_sock);
    fprintf(stdout, "Server: Shutdown ...\n");
    exit(EXIT_SUCCESS);                                             /* That's it, main thread goes out of business */
}

int main(int argc, char **argv)
{
    struct sigaction action; sigset_t sigs;
    int eins_val = 1;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket()"); exit(EXIT_FAILURE);
    }

    sigfillset(&sigs);                                          /* Mostly to keep valgrind happy */
    action.sa_flags = 0;
    action.sa_handler = sig_handler;                            /* Set up signal handler for some cleanup */
    action.sa_mask = sigs;                                      /* and housekeeping before we terminate */
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &eins_val, sizeof(eins_val));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_LISTEN_PORT);           /* Set up listen socket */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);            /* Accept any incomming address */
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(server_sock); perror("bind()"); exit(EXIT_FAILURE);
    }
    if (listen(server_sock, SERVER_LISTEN_BACKLOG) == -1) {
        close(server_sock); perror("listen()"); exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Server: Listening on %d ...\n", SERVER_LISTEN_PORT);

    list_init(&client_list);

    while (2) {
        client_t *cl = (client_t *)malloc(sizeof(client_t));    /* Allocate some mem for or client struct */
        cl->socklen = sizeof(struct sockaddr);

        if ((cl->sock = accept(server_sock, (struct sockaddr *)&cl->addr, &cl->socklen)) == -1) {
            free(cl);                                           /* Gentle reaction cause we continue looping */
            fprintf(stderr, "Server: ERROR in accept() syscall, bad luck for client\n");
        } else {
            pthread_t tid;
            list_item_t *item;
            char mqname[BUFFER_SIZE];                           /* Used to compose a message queue name in the form "/%s" */

            cl->state = CLIENT_ACCEPTED;                        /* Seems like we have a new client to serve */
            strcpy(cl->name, inet_ntoa(cl->addr.sin_addr));
            sprintf(mqname, "/%s:%d", cl->name, cl->sock);      /* Create a distinct name for the message queue */
            if ((cl->mq = mq_open(mqname, O_RDWR | O_CREAT, S_IRWXU, NULL)) == -1) {
                fprintf(stderr, "Server: Unable to create message queue for client %s on socket %d\n", cl->name, cl->sock);
                kill(getpid(), SIGTERM);
            }
            fprintf(stdout, "Server: Accepted client %s on socket %d\n", cl->name, cl->sock);

            item = (list_item_t *)malloc(sizeof(list_item_t));  /* Wrap all this into a list item and stuff it into */
            item->data = cl;                                    /* our global client list */
            item->_next = 0;
            list_add(&client_list, item);

            if ((pthread_create(&tid, NULL, thread_func, (void*)item)) != 0) {
                fprintf(stderr, "Server: ERROR in pthread_create() syscall\n");
                kill(getpid(), SIGTERM);
            } else {
                pthread_detach(tid);                            /* Shoot and forget the thread*/
            }
        }
    }
    return EXIT_SUCCESS;
}
