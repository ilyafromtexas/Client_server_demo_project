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
    char client_request[BUFFER_SIZE];

    printf("Client %d connected!\n", new_socket);

    ssize_t bytes_received = recv(new_socket, client_request, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        perror("Error receiving client request");
        close(new_socket);
        pthread_exit(NULL);
    }
    client_request[bytes_received] = '\0';

    // Check if the client requests an update
    if (strncmp(client_request, "UPDATE", strlen("UPDATE")) == 0) {
        // Client requests an update
        char client_file_name[BUFFER_SIZE];
        long client_file_size;
        if (sscanf(client_request, "UPDATE %s %ld", client_file_name, &client_file_size) == 2) {
            // Get the server's file size
            FILE *file = fopen(client_file_name, "rb");
            if (file == NULL) {
                perror("File open error");
                close(new_socket);
                pthread_exit(NULL);
            }

	    printf("Client requested update for the file %s\n", client_file_name);

            fseek(file, 0, SEEK_END);
            long server_file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            fclose(file);

	    // If file size is the same, no update available on the server, sending a message to the client
            if (client_file_size == server_file_size) {
                char no_update_message[] = "NO_UPDATE";
                send(new_socket, no_update_message, sizeof(no_update_message), 0);
                printf("No updates available for the requested file \n");
                
            // If file size is smaller, calculating the difference and sending the update
            } else if (client_file_size < server_file_size) {
                FILE *update_file = fopen(client_file_name, "rb");
                if (update_file == NULL) {
                    perror("File update failed");
                    close(new_socket);
                    pthread_exit(NULL);
                }

                fseek(update_file, client_file_size, SEEK_SET);
                char buffer[BUFFER_SIZE];
                size_t bytes_read;

                while ((bytes_read = fread(buffer, 1, sizeof(buffer), update_file)) > 0) {
                    send(new_socket, buffer, bytes_read, 0);
                }
		
		printf("Updates for the file %s has been sent \n", client_file_name);

                fclose(update_file);
            }
        }
    } else {
        // Client doesn't request an update, sending the entire file. If the file doesn't exist, sending the error message
        FILE *file = fopen(client_request, "rb");
        if (file == NULL) {
            char error_message[] = "File not found";
            printf("Requested file %s not found on the server! \nError message sent to the client.\n", client_request);
            send(new_socket, error_message, sizeof(error_message), 0); // Send error message
            close(new_socket);
            pthread_exit(NULL);
        }

        printf("Client requested file %s\n", client_request);

        char buffer[BUFFER_SIZE];
        size_t bytes_read;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(new_socket, buffer, bytes_read, 0);
        }

 	printf("Requested file has been sent!\n");	
	
        fclose(file);
    }

    // Clean up
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
