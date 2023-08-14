// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define DEFAULT_SERVER_IP "127.0.0.1"  // Default IP address if none is provided
#define PORT 12345
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    char file_name[BUFFER_SIZE];
    char server_ip[BUFFER_SIZE];
    
    // Checking if file name and server IP has been entered as arguments to the application call from CLI
    
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

    int client_fd;
    struct sockaddr_in server_addr;

    // Creating TCP socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // Connecting to the server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Sending file name to the server
    send(client_fd, file_name, strlen(file_name), 0);

	// Getting response from the server, if the file doesn't exists, server sends error message, otherwise it sends the file requested
	char response[BUFFER_SIZE];
	ssize_t response_size = recv(client_fd, response, BUFFER_SIZE, 0);
	response[response_size] = '\0';
	// Checking if the response contains the error message (File not found). Error message printed as application response
	if (strcmp(response, "File not found") == 0) {
	    printf("File '%s' not found on the server.\n", file_name);
	    close(client_fd);
	    exit(EXIT_FAILURE);
	}

	// File found, opening the file for writing
	FILE *file = fopen(file_name, "wb");	
	if (file == NULL) {
	    perror("File creation failed");
	    close(client_fd);
	    exit(EXIT_FAILURE);
	}
	// Writing the server response into the file
	fwrite(response, 1, response_size, file);
	
	printf("File '%s' received and saved successfully.\n", file_name);

    // Clean up
    fclose(file);
    close(client_fd);

    return 0;
}


