// Client application. It requests file from the server or update for the file on the client side

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <inttypes.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024
#define UPDATE_REQUEST_SIZE 1024

// Declaration of a strcucture to receive arguments for the thread function clientRequest
typedef struct {
    int request;                // type of request, 1 - file request, 2 - update request
    const char* file_name;      // file name to request
    size_t file_size;           // size of file to send to server if we request the update
    const char* server_ip;      // server IP to request file from
    int server_port;            // port number to connect with the server
} ThreadArgs;

// Function prototypes
void getUserInput(int argc, char *argv[], char *file_name, char *server_ip);
void* client_request(void* arg);
int downloadFile(int client_fd, const char *file_name);
int updateFile(int client_fd, const char *file_name);

// Function to process user inputs when starting the client application
void getUserInput(int argc, char *argv[], char *file_name, char *server_ip) {
    // If file name and server IP are not provided as arguments from CLI, we request the user to enter
    if (argc != 2 && argc != 3) {
        // Getting file name
        printf("Enter file name: ");
        if (fgets(file_name, BUFFER_SIZE, stdin) == NULL) {
            perror("Input error");
            exit(EXIT_FAILURE);
        }
        strtok(file_name, "\n");

        // Getting server IP
        printf("Enter server IP (default: %s): ", DEFAULT_SERVER_IP);
        if (fgets(server_ip, BUFFER_SIZE, stdin) == NULL) {
            perror("Input error");
            exit(EXIT_FAILURE);
        }
        server_ip[strcspn(server_ip, "\n")] = '\0';  // Remove newline character, if present

        if (strlen(server_ip) == 0) {
            strncpy(server_ip, DEFAULT_SERVER_IP, BUFFER_SIZE - 1);
        }
        server_ip[BUFFER_SIZE - 1] = '\0';  // Ensuring null-termination
    } else {
        // Reading file name from CLI
        strncpy(file_name, argv[1], BUFFER_SIZE - 1);
        file_name[BUFFER_SIZE - 1] = '\0';
        // Reading server IP from CLI, nothing provided - using default server IP 127.0.0.1
        if (argc == 3) {
            strncpy(server_ip, argv[2], BUFFER_SIZE - 1);
        } else {
            strncpy(server_ip, DEFAULT_SERVER_IP, BUFFER_SIZE - 1);
        }
        server_ip[BUFFER_SIZE - 1] = '\0'; // Ensuring null-termination
    }
}

// thread function to send client request to the server
void* client_request(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;

    // Creating socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("socket");
        return NULL;
    }

    // Configuring socket parameters
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(args->server_port);
    inet_pton(AF_INET, args->server_ip, &server_address.sin_addr); // Converting IPv4 format into network format

    // Connecting to server
    if(connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("connect");
        return NULL;
    }

     // Constructing the request string
     // request - type of request 1 - file request, 2 - update request
     // file_name - file name to request
     // file_size - 0 if file doesn't exists, file size if requesting update
    char request_str[1024];
    snprintf(request_str, sizeof(request_str), "%d|%s|%zu", args->request, args->file_name, args->file_size);
    // printf("Debug: client request string: %s\n",request_str);

    // Sending request to the server
    ssize_t bytes_sent = send(sockfd, request_str, strlen(request_str), 0);
    if(bytes_sent == -1) {
        perror("send");
        return NULL;
    }

    // Managing server responses
    // If we requested the file, we call function downloadFile
    // If we requested update for the file, we call function updateFile
    if (args->request == 1) {
        downloadFile(sockfd, args->file_name);
    } else {
        updateFile(sockfd, args->file_name);
    }

    // Closing the socket
    close(sockfd);
    free((char*)args->file_name);
    free((char*)args->server_ip);
    free(args);
    return NULL;
}

// Function to receive file from the server
// If file not found on the server, we get an error message
int downloadFile(int client_fd, const char *file_name) {
    char response[BUFFER_SIZE];
    ssize_t response_size;
    
    // Getting server response 
    response_size = recv(client_fd, response, BUFFER_SIZE - 1, 0);
    if (response_size <= 0) {
        perror("Error receiving server response or connection closed");
        close(client_fd);
        return 1;   // Handle the connection error
    } 

    // Checking if response from the server contains error message
    if (strcmp(response, "File not found") == 0) {
        printf("Server response: %s\n", response);
        return 2;   // Handle the "File not found" case
    }

    // Processing data from the response to get file size data
    uint64_t file_size = 0;
    memcpy(&file_size, response, sizeof(uint64_t));
    file_size = be64toh(file_size); // Convert from network byte order to host byte order

    // Opening file to write data received from the server
    FILE *new_file = fopen(file_name, "wb");
    if (new_file == NULL) {
        perror("File creation failed");
        close(client_fd);
        return 3;   // Handle the error with file creation
    }

    ssize_t total_received = 0;
    char buffer[BUFFER_SIZE];

    // Cycle to receive and write the data to the file
    // Every iteration we compare number of received bytes with file size.
    // If the remaining bytes to receive are less than the BUFFER_SIZE, we set bytes_to_receive to the remaining bytes. 
    // Otherwise, we set bytes_to_receive to the BUFFER_SIZE. 
    while (total_received < file_size) {
        ssize_t bytes_to_receive = (file_size - total_received) < BUFFER_SIZE ? (file_size - total_received) : BUFFER_SIZE;
        ssize_t received_bytes = recv(client_fd, buffer, bytes_to_receive, 0);

        if (received_bytes <= 0) {
            perror("Error receiving file data or connection closed");
            fclose(new_file);
            close(client_fd);
            return 4;   // Handle error with invalid response from the server
        }
        // writing received bytes into the file
        fwrite(buffer, 1, received_bytes, new_file);
        total_received += received_bytes;
    }
    // Closing file
    fclose(new_file);
    printf("File '%s' received successfully.\n", file_name);
    return 0;   // Success
}

// Function to receive updates for the file from the server
int updateFile(int client_fd, const char *file_name) {
    char response[BUFFER_SIZE];
    // Getting server response
    ssize_t response_size = recv(client_fd, response, BUFFER_SIZE - 1, 0);
        if (response_size <= 0) {
            perror("Connection closed or error");
            close(client_fd);
            return 1;
        } else {
            response[response_size] = '\0';     // null-terminating the string for correct processing

            // Checking if server tells that no updates available 
            if (strcmp(response, "NO_UPDATE") == 0) {
                printf("No updates available from server\n");
            } else {
                // Copying data from the server response to the variable update_size
                uint64_t update_size = 0;
                memcpy(&update_size, response, sizeof(uint64_t));
                // Converting update_size from network byte order to host byte order, thus we get the size of the update
                update_size = be64toh(update_size); 
                
                // Opening file to append received data to the file
                FILE *update_file = fopen(file_name, "ab");
                if (update_file == NULL) {
                    perror("File update failed");
                    close(client_fd);
                    return 2;
                }

                ssize_t total_received = 0;
                char buffer[BUFFER_SIZE];

                // Cycle to receive and write the data to the file
                // Every iteration we compare number of received bytes with update size.
                // If the remaining bytes to receive are less than the BUFFER_SIZE, we set bytes_to_receive to the remaining bytes. 
                // Otherwise, we set bytes_to_receive to the BUFFER_SIZE.  
                while (total_received < update_size) {
                ssize_t bytes_to_receive = (update_size - total_received) < BUFFER_SIZE ? (update_size - total_received) : BUFFER_SIZE;
                ssize_t received_bytes = recv(client_fd, buffer, bytes_to_receive, 0);

                    if (received_bytes <= 0) {
                        perror("Error receiving file data or connection closed");
                        fclose(update_file);
                        close(client_fd);
                        return 3;
                    }
                    // Writing received bytes to the file
                    fwrite(buffer, 1, received_bytes, update_file);
                    total_received += received_bytes;
                }

                // Closing file
                fclose(update_file);
                printf("File '%s' updated successfully.\n", file_name);
            }
        }
    return 0;
    
}

// Main function
int main(int argc, char *argv[]) {
    char file_name[BUFFER_SIZE];
    char server_ip[BUFFER_SIZE];
    int request = 1;    // request = 1 - file doesn't exist, request = 2 = file exists
    int client_fd;      
    size_t client_file_size = 0;
    struct sockaddr_in server_addr;

    // Calling function to get user input for file name and server IP
    getUserInput(argc, argv, file_name, server_ip);

    // Checking if requested file exists on the client side
    FILE *file = fopen(file_name, "rb");
        if (file == NULL) {
            // File doesn't exist
            request = 1;
            client_file_size = 0;
            printf("Requesting file %s\n", file_name);
        } else {
            // File exists, calculating the file size to check if update is available from the server
            request = 2;
            fseek(file, 0, SEEK_END);
            client_file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            fclose(file);
            printf("Requesting the update for the file %s\n", file_name);
        }

    pthread_t client_thread;
   
    // Allocating memory for the thread function arguments structure 
    ThreadArgs* args = malloc(sizeof(ThreadArgs));
    if (args == NULL) {
        perror("malloc");
        return 1;
    }

    args->request = request;               // request typoe (1 - file download, 2 - file update request)
    args->file_name = strdup(file_name);   // file name
    args->file_size = client_file_size;    //file size
    args->server_ip = strdup(server_ip);   // server IP
    args->server_port = PORT;              // server port

    // Checking if memory was allocated
    if (args == NULL) {
        perror("malloc");
        return 1;
    }

    // Creating thread to call function clientRequest
    int result = pthread_create(&client_thread, NULL, client_request, args);
    if (result) {
        fprintf(stderr, "Error creating thread: %d\n", result);
        return 1;
    }

    pthread_join(client_thread, NULL);
    free(args);
    return 0;
}