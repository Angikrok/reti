// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.1.100"  // IP del server
#define PORT 12345
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    printf("Connesso al server!\n");

    while (1) {
        printf("Inserisci messaggio (exit per uscire): ");
        fgets(buffer, BUFFER_SIZE, stdin);

        if (strncmp(buffer, "exit", 4) == 0)
            break;

        send(sockfd, buffer, strlen(buffer), 0);

        memset(buffer, 0, BUFFER_SIZE);
        recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        printf("Risposta: %s\n", buffer);
    }

    close(sockfd);
    return 0;
}
