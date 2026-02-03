#include "../include/http.h"
#include <ctype.h>

/* Décode les caractères URL (%20 -> espace, + -> espace). */
static void url_decode(char *dst, const char *src) {
    while (*src) {
        if (*src == '%' && isxdigit(src[1]) && isxdigit(src[2])) {
            /* Convertit deux chiffres hexa en caractère. */
            int value;
            sscanf(src + 1, "%2x", &value);
            *dst++ = (char)value;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* Parse la query string (ex: name=value&age=25). */
static void parse_query_string(const char *query, HttpRequest *req) {
    if (!query || !*query) {
        req->param_count = 0;
        return;
    }
    
    char query_copy[512];
    strncpy(query_copy, query, sizeof(query_copy) - 1);
    query_copy[sizeof(query_copy) - 1] = '\0';
    
    req->param_count = 0;
    char *pair = strtok(query_copy, "&");
    
    while (pair && req->param_count < MAX_QUERY_PARAMS) {
        char *equals = strchr(pair, '=');
        if (equals) {
            *equals = '\0';
            url_decode(req->params[req->param_count].key, pair);
            url_decode(req->params[req->param_count].value, equals + 1);
            req->param_count++;
        }
        pair = strtok(NULL, "&");
    }
}

const char* http_get_query_param(HttpRequest *req, const char *key) {
    for (int i = 0; i < req->param_count; i++) {
        if (strcmp(req->params[i].key, key) == 0) {
            return req->params[i].value;
        }
    }
    return NULL;
}

int http_parse_request(const char *raw_request, HttpRequest *req) {
    char buffer[BUFFER_SIZE];
    char full_path[512];
    
    memset(req, 0, sizeof(HttpRequest));
    
    /* Copie la requête pour pouvoir la manipuler tranquillement. */
    strncpy(buffer, raw_request, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* Coupe la première ligne au CRLF. */
    char *line_end = strstr(buffer, "\r\n");
    if (line_end)
        *line_end = '\0';
    
    /* Parse: METHOD /path HTTP/1.1 */
    if (sscanf(buffer, "%15s %511s %15s", 
               req->method, full_path, req->version) != 3) {
        return -1;
    }
    
    /* Sépare le chemin de la query string. */
    char *query_start = strchr(full_path, '?');
    if (query_start) {
        *query_start = '\0';
        strncpy(req->query_string, query_start + 1, sizeof(req->query_string) - 1);
        parse_query_string(req->query_string, req);
    }
    
    strncpy(req->path, full_path, sizeof(req->path) - 1);
    
    /* "/" devient "/index.html" par défaut. */
    if (strcmp(req->path, "/") == 0) {
        strcpy(req->path, "/index.html");
    }
    
    /* Pour POST, on récupère le body et le Content-Type. */
    if (strcmp(req->method, "POST") == 0) {
        char *body_start = strstr(raw_request, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            size_t len = strlen(body_start);
            if (len > 0 && len < BUFFER_SIZE) {
                strncpy(req->body, body_start, len);
                req->body_length = len;
            }
        }
        
        /* Récupère le header Content-Type si présent. */
        const char *ct = strstr(raw_request, "Content-Type:");
        if (ct) {
            const char *ct_end = strstr(ct, "\r\n");
            if (ct_end) {
                size_t len = ct_end - (ct + 14);
                if (len < sizeof(req->content_type)) {
                    strncpy(req->content_type, ct + 14, len);
                    req->content_type[len] = '\0';
                    
                    /* Supprime les espaces au début. */
                    char *trim = req->content_type;
                    while (*trim == ' ') trim++;
                    if (trim != req->content_type)
                        memmove(req->content_type, trim, strlen(trim) + 1);
                }
            }
        }
    }
    
    return 0;
}

const char* http_get_content_type(const char *filepath) {
    const char *ext = strrchr(filepath, '.');
    
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".json") == 0)
        return "application/json";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".txt") == 0)
        return "text/plain";
    
    return "application/octet-stream";
}

void http_send_response(int client_fd, int status_code, const char *content_type, 
                        const char *body, size_t body_length) {
    char header[BUFFER_SIZE];
    const char *status_text;
    
    /* Associe un texte simple au code HTTP. */
    switch (status_code) {
        case 200: status_text = "OK"; break;
        case 404: status_text = "Not Found"; break;
        case 400: status_text = "Bad Request"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown"; break;
    }
    
    /* Construit et envoie les headers. */
    int header_len = snprintf(header, sizeof(header),
        "%s %d %s\r\n"
        "Server: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        HTTP_VERSION, status_code, status_text,
        SERVER_NAME,
        content_type,
        body_length
    );
    
    /* Envoie les headers puis le body. */
    write(client_fd, header, header_len);
    if (body && body_length > 0) {
        write(client_fd, body, body_length);
    }
}

void http_send_file(int client_fd, const char *filepath) {
    char full_path[512];
    struct stat file_stat;
    char *file_content;
    
    /* Résout le chemin sous www/. */
    snprintf(full_path, sizeof(full_path), "www%s", filepath);
    
    /* Vérifie que le fichier existe et qu'il est régulier. */
    if (stat(full_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
        http_send_error(client_fd, 404);
        return;
    }
    
    /* Ouvre et lit le fichier. */
    int fd = open(full_path, O_RDONLY);
    if (fd < 0) {
        http_send_error(client_fd, 500);
        return;
    }
    
    file_content = malloc(file_stat.st_size);
    if (!file_content) {
        close(fd);
        http_send_error(client_fd, 500);
        return;
    }
    
    ssize_t bytes_read = read(fd, file_content, file_stat.st_size);
    close(fd);
    
    if (bytes_read < 0) {
        free(file_content);
        http_send_error(client_fd, 500);
        return;
    }
    
    /* Envoie le fichier avec le bon Content-Type. */
    const char *content_type = http_get_content_type(filepath);
    http_send_response(client_fd, 200, content_type, file_content, file_stat.st_size);
    
    free(file_content);
}

void http_send_error(int client_fd, int status_code) {
    char body[512];
    const char *message;
    
    switch (status_code) {
        case 404: message = "Page introuvable"; break;
        case 400: message = "Requête invalide"; break;
        case 500: message = "Erreur interne du serveur"; break;
        default: message = "Erreur"; break;
    }
    
    /* Page d'erreur simple. */
    int body_len = snprintf(body, sizeof(body),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <title>Error %d</title>\n"
        "    <style>\n"
        "        body { font-family: Arial; text-align: center; padding: 50px; }\n"
        "        h1 { color: #e74c3c; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Error %d</h1>\n"
        "    <p>%s</p>\n"
        "</body>\n"
        "</html>",
        status_code, status_code, message
    );
    
    http_send_response(client_fd, status_code, "text/html", body, body_len);
}
