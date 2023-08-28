// client.c
// Program requests file name and server IP to get the file from
// After getting the file name, it checks if the file already exists on the client side
// If there is no file, it sends the file from the server
// If the file already exists, it checks the size of the file and sends a request to the server if an update is available
// If an update is available, the client gets the update to append to the existing file
// If no update is available, the client receives a message that no update is available

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024
#define UPDATE_REQUEST_SIZE 2048

//Function to connect to the server
int connectToServer(const char *server_ip){
    int socket_fd;
    struct sockaddr_in server_addr;

    // Creating TCP socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Connecting to the server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(socket_fd);        
        exit(EXIT_FAILURE);
    }

    return socket_fd;

}


// Function to handle file download
void downloadFile(int client_fd, const char *file_name) {
    char response[BUFFER_SIZE];
    ssize_t response_size;
    
    // Receive the server's response
    response_size = recv(client_fd, response, BUFFER_SIZE, 0);
    if (response_size < 0) {
        perror("Error receiving server response");
        return; // Exit the function on error
    }

    response[response_size] = '\0';

    // Checking if there is an error message from the server
    if (strcmp(response, "File not found") == 0) {
        printf("Server response: %s\n", response);
        return; // Exit the function without creating a file
    }

    // Getting the data from the server and writing into the file
    FILE *new_file = fopen(file_name, "wb");
    if (new_file == NULL) {
        perror("File creation failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // Write the received data to the file
    fwrite(response, 1, response_size, new_file);

    // Continue receiving and writing data until the transfer is complete
    while ((response_size = recv(client_fd, response, BUFFER_SIZE, 0)) > 0) {
        fwrite(response, 1, response_size, new_file);
    }

    fclose(new_file);
    printf("File '%s' received successfully.\n", file_name);
}

// Function to handle file update
void updateFile(int client_fd, const char *file_name, const char *update_content) {
    FILE *update_file = fopen(file_name, "ab");
    if (update_file == NULL) {
        perror("File update failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    fwrite(update_content, 1, strlen(update_content), update_file);

    char response[BUFFER_SIZE];
    ssize_t response_size;
    while ((response_size = recv(client_fd, response, BUFFER_SIZE, 0)) > 0) {
        fwrite(response, 1, response_size, update_file);
    }

    fclose(update_file);
    printf("File '%s' updated successfully.\n", file_name);
}

int main(int argc, char *argv[]) {
    char file_name[BUFFER_SIZE];
    char server_ip[BUFFER_SIZE];

    // Checking if file name and server IP have been entered as arguments to the application call from CLI
    
    if (argc != 2 && argc != 3) {
        printf("Enter file name: ");
        if (fgets(file_name, sizeof(file_name), stdin) == NULL) {
        perror("Input error");
        exit(EXIT_FAILURE);
    }
        file_name[strlen(file_name) - 1] = '\0'; // Remove newline character
 
        printf("Enter server IP (default: %s): ", DEFAULT_SERVER_IP);
        if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
        perror("Input error");
        exit(EXIT_FAILURE);
    }
        server_ip[strlen(server_ip) - 1] = '\0'; // Remove newline character

        if (strlen(server_ip) == 0) {
            strcpy(server_ip, DEFAULT_SERVER_IP);
        }

    } else {
        // Parsing command line command to get file name and server IP as arguments 
        strncpy(file_name, argv[1], sizeof(file_name));
        if (argc == 3) {
            strncpy(server_ip, argv[2], sizeof(server_ip));
        } else {
            strcpy(server_ip, DEFAULT_SERVER_IP);
        }
    }

    int client_fd = connectToServer(server_ip);

    
    // Check if the file exists on the client side
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        // File doesn't exist on the client side, requesting it from the server
        send(client_fd, file_name, strlen(file_name), 0);
        printf("Requesting file %s\n", file_name);
        downloadFile(client_fd, file_name);
    } else {
        // File exists on the client side, calculating its size
        printf("File %s exists, checking for the update on the server \n", file_name);
        fseek(file, 0, SEEK_END);
        long client_file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Sending a request for an update with the client's file size
        char update_request[UPDATE_REQUEST_SIZE];
        snprintf(update_request, sizeof(update_request), "UPDATE %s %ld", file_name, client_file_size);
        send(client_fd, update_request, strlen(update_request), 0);
        printf("Update for the file %s requested \n", file_name);

        // Receiving the server's response
        char response[BUFFER_SIZE];
        ssize_t response_size = recv(client_fd, response, BUFFER_SIZE, 0);
        response[response_size] = '\0';

        if (strcmp(response, "NO_UPDATE") == 0) {
            // Server indicates no update available, do nothing
            printf("No updates available from the server \n");
        } else {
            // Server indicates an update, append it to the file
            updateFile(client_fd, file_name, response);
        } 

        fclose(file);
    }

    // Clean up
    close(client_fd);

    return 0;
}
