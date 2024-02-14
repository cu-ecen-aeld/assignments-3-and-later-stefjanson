#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>


int main() {

    // Open syslog
    openlog("aesdsocket", LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    // Create a socket
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    FILE *file;
    ssize_t bytes_received;
    char buffer[1024];
    int new_sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        closelog();
        return -1;
    }

    // Fill in the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address and port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        closelog();
        return -1;
    }

    // Listen for incoming connections
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        closelog();
        return -1;
    }

    syslog(LOG_INFO, "Server is listening on port 9000...");

    // Accept incoming connections
    client_len = sizeof(client_addr);
    new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (new_sockfd == -1) {
        perror("accept");
        close(sockfd);
        closelog();
        return -1;
    }

    // Log client information
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    syslog(LOG_INFO, "Connection accepted from %s:%d", client_ip, ntohs(client_addr.sin_port));

    // Open file to append data
    file = fopen("/var/tmp/aesdsocketdata", "a");
    if (file == NULL) {
        perror("fopen");
        close(new_sockfd);
        close(sockfd);
        closelog();
        return -1;
    }

    // Receive data and append to file
    while ((bytes_received = recv(new_sockfd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    if (bytes_received == -1) {
        perror("recv");
    }

    fclose(file);
    close(new_sockfd);
    close(sockfd);
    closelog();

    return 0;
}

