#include "../include/server.h"
#include "../include/http.h"
#include <signal.h>
#include <sys/wait.h>

static Server g_server;

static void handle_shutdown(int sig) {
    (void)sig;
    printf("\n\nArrêt du serveur...\n");
    server_close(&g_server);
    exit(0);
}

/* Ramasse les processus enfants terminés pour éviter les zombies. */
static void handle_child_exit(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* Réponse dynamique pour /hello?name=... */
static void respond_hello(int client_fd, HttpRequest *request) {
    const char *name = http_get_query_param(request, "name");
    
    if (!name) {
        name = "Guest";
    }
    
    char response[1024];
    snprintf(response, sizeof(response),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><meta charset='UTF-8'><title>Bonjour</title></head>\n"
        "<body style='font-family: Arial; text-align: center; padding: 50px;'>\n"
        "<h1>Bonjour %s !</h1>\n"
        "<p>Les paramètres GET sont bien reçus.</p>\n"
        "<a href='/'>Retour à l'accueil</a>\n"
        "</body>\n"
        "</html>", name);
    
    http_send_response(client_fd, 200, "text/html", response, strlen(response));
}

/* Réponse simple pour /test */
static void respond_test(int client_fd, const HttpRequest *request) {
    char response[512];
    snprintf(response, sizeof(response),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<body>\n"
        "<h1>Test OK</h1>\n"
        "<p>Paramètres reçus : %d</p>\n"
        "</body>\n"
        "</html>",
        request->param_count);
    
    http_send_response(client_fd, 200, "text/html", response, strlen(response));
}

/* Page de confirmation pour les requêtes POST */
static void respond_post(int client_fd, const HttpRequest *request) {
    char response[2048];
    snprintf(response, sizeof(response),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><meta charset='UTF-8'><title>POST Reçu</title></head>\n"
        "<body style='font-family: Arial; padding: 50px; background: #f0f8ff;'>\n"
        "<h1 style='color: #28a745;'>Formulaire reçu avec succès</h1>\n"
        "<div style='background: white; padding: 20px; border-radius: 10px;'>\n"
        "<h2>Données reçues :</h2>\n"
        "<pre style='background: #f5f5f5; padding: 15px;'>%s</pre>\n"
        "<p><strong>Content-Type :</strong> %s</p>\n"
        "<p><strong>Taille :</strong> %zu bytes</p>\n"
        "</div>\n"
        "<a href='/'>Retour à l'accueil</a>\n"
        "</body>\n"
        "</html>",
        request->body_length > 0 ? request->body : "(empty)",
        request->content_type[0] ? request->content_type : "not specified",
        request->body_length);
    
    http_send_response(client_fd, 200, "text/html", response, strlen(response));
}

/* Traite une requête client dans un processus enfant. */
static void handle_client_request(int client_fd, const struct sockaddr_in *client_addr) {
    char buffer[BUFFER_SIZE];
    HttpRequest request;
    
    memset(buffer, 0, sizeof(buffer));
    
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    if (http_parse_request(buffer, &request) < 0) {
        http_send_error(client_fd, 400);
        close(client_fd);
        return;
    }
    
    /* Log simple de la requête */
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    
    if (request.param_count > 0) {
        char log_path[512];
        snprintf(log_path, sizeof(log_path), "%s?%s", request.path, request.query_string);
        server_log_request(client_ip, request.method, log_path);
    } else {
        server_log_request(client_ip, request.method, request.path);
    }
    
    /* Routage selon la méthode et le chemin. */
    if (strcmp(request.method, "GET") == 0) {
        if (strncmp(request.path, "/hello", 6) == 0) {
            respond_hello(client_fd, &request);
        } else if (strncmp(request.path, "/test", 5) == 0) {
            respond_test(client_fd, &request);
        } else if (strncmp(request.path, "/api", 4) == 0 || strcmp(request.path, "/submit") == 0) {
            /* Pour les appels API, on retourne du JSON. */
            char json[256];
            snprintf(json, sizeof(json),
                "{\"status\":\"ok\",\"message\":\"GET request\",\"params\":%d}",
                request.param_count);
            http_send_response(client_fd, 200, "application/json", json, strlen(json));
        } else {
            /* Fichiers statiques */
            http_send_file(client_fd, request.path);
        }
    } else if (strcmp(request.method, "POST") == 0) {
        printf("  Corps POST (%zu bytes): %s\n", request.body_length, request.body);
        respond_post(client_fd, &request);
    } else {
        http_send_error(client_fd, 400);
    }
    
    close(client_fd);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            return 1;
        }
    }
    
    /* Configure les signaux */
    signal(SIGINT, handle_shutdown);
    signal(SIGCHLD, handle_child_exit);
    
    /* Crée le serveur */
    if (server_create(&g_server, port) < 0) {
        return 1;
    }
    
    printf("Serveur disponible sur: http://localhost:%d\n", port);
    printf("Ctrl+C pour arrêter\n\n");
    
    /* Boucle principale : accepte les connexions et fork pour chacune */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(g_server.socket_fd, 
                              (struct sockaddr *)&client_addr, 
                              &client_len);
        
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }
        
        /* Fork pour gérer le client en parallèle */
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            close(client_fd);
            continue;
        }
        
        if (pid == 0) {
            /* Processus enfant : traite la requête puis termine */
            close(g_server.socket_fd);
            handle_client_request(client_fd, &client_addr);
            exit(0);
        } else {
            /* Processus parent : ferme le socket client et continue */
            close(client_fd);
        }
    }
    
    server_close(&g_server);
    return 0;
}
