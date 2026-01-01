#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h> // NEW: Library for threading

#define PORT 8888
#define BUFFER_SIZE 4096

// --- BLACKLIST CHECKER ---
int check_forbidden(char *url) {
    char *banned_list[] = {"google", "youtube", "facebook"};
    int list_size = 3;
    for (int i = 0; i < list_size; i++) {
        if (strstr(url, banned_list[i]) != NULL) return 1;
    }
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

// --- THE CLONED WORKER (Thread Function) ---
// This function now returns void* and takes void* because threads require it
void *handle_client(void *socket_desc) {
    // Get the socket ID from the argument
    int client_socket = *(int*)socket_desc;
    free(socket_desc); // Free the memory we allocated in main

    char buffer[BUFFER_SIZE];
    int bytes_read;

    // 1. Read Request
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        return NULL;
    }

    // 2. Parse URL
    char method[16], url[2048], version[16];
    char host[1024];
    sscanf(buffer, "%s %s %s", method, url, version);
    
    // 3. Security Check
    if (check_forbidden(url) == 1) {
        printf("THREAD %ld: Blocked access to %s\n", pthread_self(), url);
        char *error_msg = "HTTP/1.1 403 Forbidden\r\n\r\nACCESS DENIED";
        send(client_socket, error_msg, strlen(error_msg), 0);
        close(client_socket);
        return NULL;
    }
    
    // 4. Connect to Real Server
    extract_host(url, host);
    printf("THREAD %ld: Handling %s\n", pthread_self(), host);

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
        perror("Connection failed");
        close(client_socket);
        close(server_socket);
        return NULL;
    }

    send(server_socket, buffer, bytes_read, 0);

    // 5. Relay Data
    while ((bytes_read = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(server_socket);
    close(client_socket);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) { // Increased backlog to 10
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Multi-Threaded Proxy listening on port %d...\n", PORT);

    while (1) {
        // Wait for a connection
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // --- THE MAGIC: Create a new thread ---
        pthread_t thread_id;
        
        // We allocate memory for the socket ID so the thread gets its own copy
        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void*)pclient) < 0) {
            perror("Could not create thread");
            free(pclient);
        }
        
        // detach means "Fire and Forget". The main program doesn't wait for the thread to finish.
        pthread_detach(thread_id);
    }
    return 0;
}