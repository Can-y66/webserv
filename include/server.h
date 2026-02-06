#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 4096
#define MAX_PENDING_CONNECTIONS 10

typedef struct {
    int socket_fd;
    int port;
    struct sockaddr_in address;
} Server;

int server_create(Server *srv, int port);

int server_accept_connection(Server *srv);

void server_close(Server *srv);

void server_log_request(const char *client_ip, const char *method, const char *path);

#endif
