#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8888
#define BUFFER_SIZE 4096  // Size of our "Notepad" to write down requests

// --- NEW FUNCTION: The Conversation Handler ---
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // 1. CLEAR THE NOTEPAD
    // We fill the buffer with zeros so there is no junk data from before.
    memset(buffer, 0, BUFFER_SIZE);

    // 2. LISTEN (Receive Data)
    // recv() attempts to read data from the phone line.
    // It puts the data into 'buffer' and tells us how many letters it read.
    bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read < 0) {
        perror("Error reading from socket");
        return;
    }
    
    if (bytes_read == 0) {
        printf("Client disconnected unexpectedly.\n");
        return;
    }

    // 3. SHOW ME WHAT THEY SAID
    printf("\n--- NEW REQUEST RECEIVED ---\n");
    printf("%s\n", buffer);
    printf("----------------------------\n");

    // 4. PARSE (Read the handwriting)
    // We want to find the first line: e.g., "GET http://google.com HTTP/1.1"
    char method[16], url[2048], version[16];
    
    // sscanf is a tool that scans a string and extracts words.
    // It looks for 3 words separated by spaces.
    sscanf(buffer, "%s %s %s", method, url, version);

    printf("Parsed Data:\n");
    printf("Method:  %s\n", method); // e.g., GET
    printf("URL:     %s\n", url);    // e.g., http://google.com
    
    // 5. SEND A FAKE REPLY (Just for now)
    // We need to be polite and say something back, or the browser will hang.
    char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello! I am your Proxy.";
    send(client_socket, response, strlen(response), 0);

    // 6. HANG UP
    close(client_socket);
}

// --- MAIN SETUP ---
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Prepare Address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // --- INFINITE LOOP ---
    // The server never sleeps. It waits for a call, handles it, then waits for the next.
    while (1) {
        // Accept a call
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue; // If one call fails, just wait for the next one
        }

        // Handle the call using our new function
        handle_client(new_socket);
    }

    return 0;
}