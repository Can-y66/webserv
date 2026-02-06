#ifndef HTTP_H
#define HTTP_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define HTTP_VERSION "HTTP/1.1"
#define SERVER_NAME "WebServ/1.0"
#define BUFFER_SIZE 4096
#define MAX_QUERY_PARAMS 20

typedef struct {
    char key[64];
    char value[256];
} QueryParam;

typedef struct {
    char method[16];
    char path[256];
    char version[16];
    char query_string[512];
    QueryParam params[MAX_QUERY_PARAMS];
    int param_count;
    char body[BUFFER_SIZE];
    size_t body_length;
    char content_type[128];
} HttpRequest;

int http_parse_request(const char *raw_request, HttpRequest *req);

void http_send_response(int client_fd, int status_code, const char *content_type, 
                        const char *body, size_t body_length);

void http_send_file(int client_fd, const char *filepath);

void http_send_error(int client_fd, int status_code);

const char* http_get_content_type(const char *filepath);

const char* http_get_query_param(HttpRequest *req, const char *key);

#endif
