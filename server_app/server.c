/*
server program sends requested file or update to the file to the client using TCP sockets.
client requests processed in handleReadSocket (either file request or update request)
data sending to the client is done in handleWriteSocket
handleReadSocket and handleWriteSocket use mutex to synchronize access to the shared resources
handleReadSocket use buffer to provide data necessary to send file or update to the file to handleWriteSocket
handleWriteSocket uses function sendFile() to send the requested file or error message if file is not found
on the server and function updateFile() to send the update for the file client requested. If there is no update,
function informs the client that there is no update for the client. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Initialization of mutex to use with threads
char buffer[BUFFER_SIZE];   // Buffer to exchange data between sockets
int buffer_empty = 1;       // Flag to indicate if the buffer is empty

// Function prototypes
void sendFile(const char *file_name, int client_socket);
void updateFile(const char *file_name, ssize_t client_file_size, int client_socket);
void *handleReadSocket(void *arg);
void *handleWriteSocket(void *arg);

//Function sends file to the client
void sendFile(const char *file_name, int client_socket) {
    // Open file
    FILE *file = fopen(file_name, "rb");
    printf("Client requested file %s\n", file_name);
    // checking if file exists on the server
    if (file == NULL) {
        char error_message[] = "File not found";
        send(client_socket, error_message, sizeof(error_message), 0);
        printf("Requested file %s not found on the server!\nError message sent to the client.\n", file_name);
    } else {

        //File exists, calculating the size of the file
        fseek(file, 0, SEEK_END);               // Move pointer to the end of the file
        uint64_t req_file_size = ftell(file);   // Get the current position, this is the file size
        fseek(file, 0, SEEK_SET);               // Move pointer back to the beginning of the file

        //converting the integer value of file size to the big-endian format used with network communications 
        uint64_t network_value = htobe64(req_file_size);

        // Send the file size to the client
        ssize_t bytes_sent = send(client_socket, &network_value, sizeof(network_value), 0);
        if (bytes_sent < 0) {
            perror("Error sending data");
        }
        
        // Send the file to the client
        char send_buffer[BUFFER_SIZE];
        size_t bytes_read;
        size_t total_sent = 0;
        // Cycle to read from the file in chunks of the buffer size and sending it to the client
        while ((bytes_read = fread(send_buffer, 1, sizeof(send_buffer), file)) > 0) {
            ssize_t bytes_sent = send(client_socket, send_buffer, bytes_read, 0);
            if (bytes_sent < 0) {
                perror("Error sending file data");
                break;
            }
            //Checking if we sent all bytes from the entire file
            total_sent += bytes_sent;
            if (total_sent >= req_file_size) {
                // We've sent the entire file, exit from the cycle
                break;
            }
        }

        printf("Requested file has been sent!\n");
        //Closing file
        fclose(file);
    }
}

// Function sends update for the file to the client
void updateFile(const char *file_name, ssize_t client_file_size, int client_socket) {
    // File open
    FILE *file = fopen(file_name, "rb");
    printf("Client requested update for the file %s\n", file_name);
    // Checking for errors when opening file
    if (file == NULL) {
        perror("File open error");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Checking the file size of the requested file on the server
    ssize_t server_file_size = 0;
    fseek(file, 0, SEEK_END);
    server_file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);

    // If file size is the same, no update available on the server, sending a message back to the client
    if (client_file_size == server_file_size) {
        char no_update_message[] = "NO_UPDATE";
        send(client_socket, no_update_message, sizeof(no_update_message), 0);
        printf("No updates available for the requested file\n");
        //Checking if file size on the server is bigger than the size of the file on the client side
    } else if (client_file_size < (size_t)server_file_size) {
        FILE *update_file = fopen(file_name, "rb");
        // Checking errors with opening file
        if (update_file == NULL) {
            perror("File update failed");
            close(client_socket);
            pthread_exit(NULL);
        }
        // Calculating size of the update to send
        ssize_t update_size = server_file_size - client_file_size;
        uint64_t network_bytes = htobe64(update_size);

        // Send the update size to the client
        ssize_t bytes_sent = send(client_socket, &network_bytes, sizeof(network_bytes), 0);
        if (bytes_sent < 0) {
            perror("Error sending data");
        }

        //Preparing to send the update - identifying the portion of the file to send
        fseek(update_file, client_file_size, SEEK_SET); // setting the pointer to the right position
        
        char update_buffer[BUFFER_SIZE];
        size_t bytes_read;
        size_t total_sent = 0;

        // Cycle to send bytes from the file on the server to the client
        while ((bytes_read = fread(update_buffer, 1, sizeof(update_buffer), update_file)) > 0) {
            ssize_t bytes_sent = send(client_socket, update_buffer, bytes_read, 0);
            if (bytes_sent < 0) {
                perror("Error sending file data");
                break;
            }
            total_sent += bytes_sent;
            // Checking if we sent all bytes for the update
            if (total_sent >= update_size) {
                // We've sent the entire file
                break;
            }
        }

        printf("Updates for the file %s have been sent\n", file_name);

        fclose(update_file);
    }
}

// Thread function to receive data from the client
void *handleReadSocket(void *arg) {
    int client_socket = *((int *)arg);
    char receive_buffer[BUFFER_SIZE];
    // Receiving the client request 
    while (1) {
        ssize_t bytes_received = recv(client_socket, receive_buffer, sizeof(receive_buffer), 0);
        if (bytes_received <= 0) {
            break;
        }
        // Null-tterminating the string to correclty process
        receive_buffer[bytes_received] = '\0';
        // Locking the buffer not to allow other threads to make changes to it
        pthread_mutex_lock(&mutex);
        //printf("Received: %s\n", receive_buffer);
        // Checking the flag if buffer for data exchange with handleWriteSocket is empty
        while (!buffer_empty) {
            pthread_mutex_unlock(&mutex);
            sched_yield();              // Let other threads run
            pthread_mutex_lock(&mutex);
        }
        // writing the data from the receive buffer (client request) to the buffer to send to handleWriteSocket
        strcpy(buffer, receive_buffer);
        buffer_empty = 0;
        // Setting the flag that buffer was read (empty) and unlocking the receive buffer
        pthread_mutex_unlock(&mutex);
    }
    // Exit from the receive thread
    pthread_exit(NULL);
}

// Thread function to send data to the client
void *handleWriteSocket(void *arg) {
    int client_socket = *((int *)arg);
    char message[BUFFER_SIZE];
    int continue_processing = 1; // Flag to indicate if there is another request to process

    // We sendin the data if handleReadSocket thansfers us the client request 
    while (continue_processing) {
        pthread_mutex_lock(&mutex);
        // Checking the flag if something is written into the buffer
        while (buffer_empty) {
            pthread_mutex_unlock(&mutex);
            sched_yield(); // Let other threads run
            pthread_mutex_lock(&mutex);
        }
        
        // Processing the request we got from the client
        int request_type;
        char file_name[BUFFER_SIZE];
        size_t file_size;

        // Parsing the string sent through the buffer <requesttype>|<file name>|<file size>
        // 1 - file request, 2 - update request, anything else - invalid request
        sscanf(buffer, "%d|%[^|]|%zu", &request_type, file_name, &file_size);

        // Checking type of request
        if (request_type == 1) {
            // If type is 1, we send file using function sendFile()
            sendFile(file_name, client_socket);
            // Closing socket
            close(client_socket);
            // If type is 2, we send update for the file using function updateFile()
        } else if (request_type == 2) {
            updateFile(file_name, file_size, client_socket);
        } else {
            // Handle invalid request type
            printf("Invalid request type: %d\n", request_type);
        }

        buffer[0] = '\0';               // Clearing the buffer
        buffer_empty = 1;               // Flag to indicate that buffer is empty
        pthread_mutex_unlock(&mutex);

        // Check if there are more requests to process
        continue_processing = !buffer_empty;

    }
    // Closing socket and exiting from the thread
    close(client_socket);
    pthread_exit(NULL);
}

// main function
int main() {
    // init
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);
    pthread_mutex_init(&mutex, NULL);
    
    // Creating server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    // File doesn't exist
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Binding server socket to the server address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding");
        exit(EXIT_FAILURE);
    }
    
    // Listebing for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Error listening");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d\n", PORT);
    
    // Waiting for the connection and accepting client connection
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Creating threads
        // handleReadSocket to receive data from the client
        // handleWriteSocket to send data to the client 
        pthread_t socket_read_thread, socket_write_thread;
        if (pthread_create(&socket_read_thread, NULL, handleReadSocket, (void *)&client_socket) != 0 ||
            pthread_create(&socket_write_thread, NULL, handleWriteSocket, (void *)&client_socket) != 0) {
            perror("Error creating thread");
            close(client_socket);
        }

        // Wait for threads to finish
        pthread_join(socket_read_thread, NULL);
        pthread_join(socket_write_thread, NULL);
        printf("Waiting for the new client request...");
    }
    // Freeing memory aloocated for mutex and closing server socket 
    pthread_mutex_destroy(&mutex);
    close(server_socket);
    return 0;
}
