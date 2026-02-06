#include "../include/server.h"

int server_create(Server *srv, int port) {
    int reuse = 1;
    
    srv->port = port;
    
    srv->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->socket_fd < 0) {
        perror("socket");
        return -1;
    }
    
    if (setsockopt(srv->socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        close(srv->socket_fd);
        return -1;
    }
    
    memset(&srv->address, 0, sizeof(srv->address));
    srv->address.sin_family = AF_INET;
    srv->address.sin_addr.s_addr = INADDR_ANY;
    srv->address.sin_port = htons(port);
    
    if (bind(srv->socket_fd, (struct sockaddr *)&srv->address, 
             sizeof(srv->address)) < 0) {
        perror("bind");
        close(srv->socket_fd);
        return -1;
    }
    
    if (listen(srv->socket_fd, MAX_PENDING_CONNECTIONS) < 0) {
        perror("listen");
        close(srv->socket_fd);
        return -1;
    }
    
    printf("Server listening on port %d\n", port);
    
    return 0;
}

int server_accept_connection(Server *srv) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(srv->socket_fd, (struct sockaddr *)&client_addr, 
                           &client_len);
    
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }
    
    return client_fd;
}

void server_close(Server *srv) {
    if (srv->socket_fd >= 0) {
        close(srv->socket_fd);
        printf("\nServer closed\n");
    }
}

void server_log_request(const char *client_ip, const char *method, const char *path) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    printf("[%s] %s - %s %s\n", timestamp, client_ip, method, path);
}
