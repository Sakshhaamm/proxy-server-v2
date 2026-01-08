#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // NEW: For inet_ntoa (Client IP)
#include <netdb.h>
#include <pthread.h>
#include <time.h>      // NEW: For timestamps

#define BUFFER_SIZE 4096

// --- CONFIGURATION GLOBALS ---
int SERVER_PORT = 8888; // Default, will be overwritten by config file

// --- HELPER: Load Config ---
void load_config() {
    FILE *file = fopen("config/proxy.conf", "r");
    if (!file) {
        printf("Notice: proxy.conf not found. Using default Port %d\n", SERVER_PORT);
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "PORT=", 5) == 0) {
            SERVER_PORT = atoi(line + 5);
        }
    }
    fclose(file);
    printf("Loaded Configuration: Port %d\n", SERVER_PORT);
}

// --- HELPER: Logging with Timestamp & IP ---
void log_action(char *client_ip, char *url, char *action) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    
    // Format: [Timestamp] [ClientIP] [Action] URL
    printf("[%s] [%s] [%s] %s\n", time_str, client_ip, action, url);
}

// --- BLACKLIST CHECKER ---
int check_forbidden(char *url) {
    FILE *file = fopen("config/blocked.txt", "r");
    if (!file) return 0; // If no file, nothing is blocked

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0; // Remove newline
        if (strlen(line) > 0 && strstr(url, line) != NULL) {
            fclose(file);
            return 1; // Blocked
        }
    }
    fclose(file);
    return 0;
}

// --- HOSTNAME EXTRACTOR ---
void extract_host(char *url, char *host) {
    char *start = strstr(url, "://");
    if (start) start += 3;
    else start = url;
    
    char *end = strchr(start, '/');
    if (end) {
        strncpy(host, start, end - start);
        host[end - start] = '\0';
    } else {
        strcpy(host, start);
    }
    char *colon = strchr(host, ':');
    if (colon) *colon = '\0';
}

// --- THREAD HANDLER ---
void *handle_client(void *args) {
    // Unpack arguments
    int client_socket = ((int*)args)[0];
    struct sockaddr_in client_addr = *((struct sockaddr_in*)((char*)args + sizeof(int)));
    free(args); // Free memory

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    char buffer[BUFFER_SIZE];
    int bytes_read;

    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        return NULL;
    }

    char method[16], url[2048], version[16];
    sscanf(buffer, "%s %s %s", method, url, version);
    
    // 1. Security Check
    if (check_forbidden(url) == 1) {
        log_action(client_ip, url, "BLOCKED"); // LOGGING
        char *error_msg = "HTTP/1.1 403 Forbidden\r\n\r\nACCESS DENIED";
        send(client_socket, error_msg, strlen(error_msg), 0);
        close(client_socket);
        return NULL;
    }
    
    log_action(client_ip, url, "ALLOWED"); // LOGGING

    // 2. Connect to Remote
    char host[1024];
    extract_host(url, host);

    struct hostent *server_info = gethostbyname(host);
    if (server_info == NULL) {
        printf("Error: Unknown host %s\n", host);
        close(client_socket);
        return NULL;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    memcpy(&server_addr.sin_addr.s_addr, server_info->h_addr, server_info->h_length);

    if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(client_socket);
        close(server_socket);
        return NULL;
    }

    send(server_socket, buffer, bytes_read, 0);

    // 3. Relay
    while ((bytes_read = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(server_socket);
    close(client_socket);
    return NULL;
}

int main() {
    setbuf(stdout, NULL); 
    load_config(); // Load Port from file

    int server_fd, new_socket;
    struct sockaddr_in address;
    // int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Proxy Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (new_socket < 0) continue;

        pthread_t thread_id;
        
        // Pass both socket AND address to thread
        void *args = malloc(sizeof(int) + sizeof(struct sockaddr_in));
        ((int*)args)[0] = new_socket;
        *((struct sockaddr_in*)((char*)args + sizeof(int))) = client_addr;

        if (pthread_create(&thread_id, NULL, handle_client, args) < 0) {
            free(args);
        }
        pthread_detach(thread_id);
    }
    return 0;
}