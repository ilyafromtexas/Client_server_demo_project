// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 12345
#define BUFFER_SIZE 1024

// Function to handle each client connection
void *handle_connection(void *arg) {
    int new_socket = *((int *)arg);
    char file_name[BUFFER_SIZE];
    
    printf("Client %d connected! \n", new_socket);
    
    ssize_t bytes_received = recv(new_socket, file_name, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        perror("Error receiving file name");
        close(new_socket);
        pthread_exit(NULL);
    }
    file_name[bytes_received] = '\0';

    printf("Requested file name: %s\n", file_name);

    // Open and send file
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        char error_message[] = "File not found";
        printf("File %s not found! Error message sent to the client. \n", file_name);
        send(new_socket, error_message, sizeof(error_message), 0); // Send error message
        close(new_socket);
        pthread_exit(NULL);
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(new_socket, buffer, bytes_read, 0);
    }

    printf("File %s has been sent! \n", file_name);

    // Clean up
    fclose(file);
    close(new_socket);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Creating socket
    if ((server_fd =socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Binding socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listening for incoming connections
    if (listen(server_fd, 5) == -1) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for the client connection\n");

    while (1) {
        // Accepting incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
            perror("Accepting connection failed");
            exit(EXIT_FAILURE);
        }

        // Create a new thread to handle the connection
        pthread_t tid;
        
 //       printf("Processing the request \n");
        
        if (pthread_create(&tid, NULL, handle_connection, &new_socket) != 0) {
            perror("Thread creation failed");
            close(new_socket);
            continue;
        }

        // Detach the thread to allow it to clean up itself when done
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
