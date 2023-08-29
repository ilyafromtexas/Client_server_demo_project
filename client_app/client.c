#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024
#define UPDATE_REQUEST_SIZE 2048

void downloadFile(int client_fd, const char *file_name);
void updateFile(int client_fd, const char *file_name, const char *update_content);
void getUserInput(int argc, char *argv[], char *file_name, char *server_ip);

int main(int argc, char *argv[]) {
    char file_name[BUFFER_SIZE];
    char server_ip[BUFFER_SIZE];
    char choice[10];
    int client_fd;
    struct sockaddr_in server_addr;

     do {
        getUserInput(argc, argv, file_name, server_ip);

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

        if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            perror("Connection failed");
            exit(EXIT_FAILURE);
        }

        FILE *file = fopen(file_name, "rb");
        if (file == NULL) {
            send(client_fd, file_name, strlen(file_name), 0);
            printf("Requesting file %s\n", file_name);
            downloadFile(client_fd, file_name);
        } else {
            printf("File %s exists, checking for update on server\n", file_name);
            fseek(file, 0, SEEK_END);
            long client_file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            fclose(file);

            char update_request[UPDATE_REQUEST_SIZE];
            snprintf(update_request, sizeof(update_request), "UPDATE %s %ld", file_name, client_file_size);
            send(client_fd, update_request, strlen(update_request), 0);
            printf("Update for the file %s requested\n", file_name);

            char response[BUFFER_SIZE];
            ssize_t response_size = recv(client_fd, response, BUFFER_SIZE - 1, 0);
            if (response_size <= 0) {
                perror("Connection closed or error");
                close(client_fd);
                exit(EXIT_FAILURE);
            }
            response[response_size] = '\0';

            if (strcmp(response, "NO_UPDATE") == 0) {
                printf("No updates available from server\n");
            } else {
                updateFile(client_fd, file_name, response);
            }
        }

    close(client_fd);
    printf("Would you like to make another file request? (Yes/No): ");
        if (fgets(choice, sizeof(choice), stdin) == NULL) {
            perror("Input error");
            exit(EXIT_FAILURE);
        }
        strtok(choice, "\n");  // Remove the newline character

    } while (strcasecmp(choice, "Yes") == 0);

    printf("Exiting the program.\n");
    return 0;
}

void getUserInput(int argc, char *argv[], char *file_name, char *server_ip) {
    if (argc != 2 && argc != 3) {
        printf("Enter file name: ");
        if (fgets(file_name, BUFFER_SIZE, stdin) == NULL) {
            perror("Input error");
            exit(EXIT_FAILURE);
        }
        strtok(file_name, "\n");

        printf("Enter server IP (default: %s): ", DEFAULT_SERVER_IP);
        if (fgets(server_ip, BUFFER_SIZE, stdin) == NULL) {
            perror("Input error");
            exit(EXIT_FAILURE);
        }
        server_ip[strcspn(server_ip, "\n")] = '\0';  // Remove newline character, if present

        if (strlen(server_ip) == 0) {
            strncpy(server_ip, DEFAULT_SERVER_IP, BUFFER_SIZE - 1);
        }
        server_ip[BUFFER_SIZE - 1] = '\0'; // Ensuring null-termination
    } else {
        strncpy(file_name, argv[1], BUFFER_SIZE - 1);
        file_name[BUFFER_SIZE - 1] = '\0';
        if (argc == 3) {
            strncpy(server_ip, argv[2], BUFFER_SIZE - 1);
        } else {
            strncpy(server_ip, DEFAULT_SERVER_IP, BUFFER_SIZE - 1);
        }
        server_ip[BUFFER_SIZE - 1] = '\0'; // Ensuring null-termination
    }
}

void downloadFile(int client_fd, const char *file_name) {
    char response[BUFFER_SIZE];
    ssize_t response_size;

    response_size = recv(client_fd, response, BUFFER_SIZE - 1, 0);
    if (response_size <= 0) {
        perror("Error receiving server response or connection closed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    response[response_size] = '\0';

    if (strcmp(response, "File not found") == 0) {
        printf("Server response: %s\n", response);
        return;
    }

    FILE *new_file = fopen(file_name, "wb");
    if (new_file == NULL) {
        perror("File creation failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    fwrite(response, 1, response_size, new_file);

    while ((response_size = recv(client_fd, response, BUFFER_SIZE - 1, 0)) > 0) {
        fwrite(response, 1, response_size, new_file);
    }

    if (response_size < 0) {
        perror("Error receiving remaining part of file or connection closed");
    }

    fclose(new_file);
    printf("File '%s' received successfully.\n", file_name);
}

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
    while ((response_size = recv(client_fd, response, BUFFER_SIZE - 1, 0)) > 0) {
        fwrite(response, 1, response_size, update_file);
    }

    if (response_size < 0) {
        perror("Error receiving remaining part of update or connection closed");
    }

    fclose(update_file);
    printf("File '%s' updated successfully.\n", file_name);
}

