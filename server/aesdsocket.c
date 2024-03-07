#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_BUFFER_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"

// Global variable to control the main loop
volatile sig_atomic_t running = 1;
// Global variable to store daemon mode
int daemon_mode = 0;

// Signal handler for SIGINT and SIGTERM
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        running = 0;
        syslog(LOG_INFO, "Caught signal, exiting");
    }
}

// Function to cleanup resources and exit gracefully
void cleanup() {
    // Close syslog
    closelog();

    // Delete the data file
    remove(DATA_FILE_PATH);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Set up signal handler for SIGINT and SIGTERM
    struct sigaction sigint_action, sigterm_action;
    memset(&sigint_action, 0, sizeof(sigint_action));
    sigint_action.sa_handler = signal_handler;
    sigaction(SIGINT, &sigint_action, NULL);
    memset(&sigterm_action, 0, sizeof(sigterm_action));
    sigterm_action.sa_handler = signal_handler;
    sigaction(SIGTERM, &sigterm_action, NULL);

    // Open syslog
    openlog("aesdsocket", LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    if (daemon_mode) {
        // Run as a daemon
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            cleanup();
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            // Parent process, exit
            exit(EXIT_SUCCESS);
        }

        // Child process continues
        umask(0);
        if (setsid() == -1) {
            perror("setsid");
            cleanup();
            exit(EXIT_FAILURE);
        }
    }
    
    // Create a socket
    int sockfd;
    struct sockaddr_in server_addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        cleanup();
        exit(EXIT_FAILURE);
    }

    // Fill in the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address and port
    while (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        if (errno == EADDRINUSE) {
            // Port is in use, wait and retry
            sleep(1);
        } else {
            perror("bind");
            cleanup();
            exit(EXIT_FAILURE);
        }
    }

    // Listen for incoming connections
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        cleanup();
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Server is listening on port 9000...");

    while (running) {
        // Accept incoming connections
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (new_sockfd == -1) {
            if (errno == EINTR) {
                // Accept interrupted by signal, continue
                continue;
            } else {
                perror("accept");
                cleanup();
                exit(EXIT_FAILURE);
            }
        }

        // Log client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s:%d", client_ip, ntohs(client_addr.sin_port));

        // Open file to append data
        FILE *file = fopen(DATA_FILE_PATH, "a");
        if (file == NULL) {
            perror("fopen");
            close(new_sockfd);
            cleanup();
            exit(EXIT_FAILURE);
        }

        // Receive data and append to file
        ssize_t bytes_received;
        char buffer[MAX_BUFFER_SIZE];
        while ((bytes_received = recv(new_sockfd, buffer, sizeof(buffer), 0)) > 0) {
            // Iterate over received buffer
            for (int i = 0; i < bytes_received; i++) {
                // Write each character to file
                fwrite(&buffer[i], 1, 1, file);
                // If newline is found, flush the buffer and send file content back
                if (buffer[i] == '\n') {
                    fflush(file); // Flush the buffer

                    // Send the content of the file back to the sender
                    FILE *read_file = fopen(DATA_FILE_PATH, "r");
                    if (read_file == NULL) {
                        perror("fopen");
                        close(new_sockfd);
                        cleanup();
                        exit(EXIT_FAILURE);
                    }
                    
                    char read_buffer[MAX_BUFFER_SIZE];
                    size_t read_bytes;
                    while ((read_bytes = fread(read_buffer, 1, MAX_BUFFER_SIZE, read_file)) > 0) {
                        send(new_sockfd, read_buffer, read_bytes, 0);
                    }

                    fclose(read_file);
                }
            }
        }

        if (bytes_received == -1) {
            perror("recv");
        }

        // Log message indicating the closure of the connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);

        // Close the file
        fclose(file);

        // Close the new socket
        close(new_sockfd);
    }

    // Close the main socket
    close(sockfd);

    // Cleanup resources and exit gracefully
    cleanup();

    return 0;
}
