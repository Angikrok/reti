#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

// Costanti per indirizzo del server e limiti dei campi
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345
#define EMAIL_LEN 50
#define PASS_LEN 50
#define MSG_LEN 256

int sockfd; // Descrittore del socket globale per ricezione

// Thread dedicato alla ricezione dei messaggi dal server
void *receive_handler(void *arg) {
    char buffer[MSG_LEN + EMAIL_LEN + 50];

    while (1) {
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break; // Se il server chiude la connessione
        buffer[bytes] = '\0';  // Terminazione stringa
        printf("\n📨 %s> ", buffer); // Mostra messaggio ricevuto
        fflush(stdout);
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr; // Struttura per indirizzo del server
    char email[EMAIL_LEN], password[PASS_LEN];
    char command[10];
    char buffer[MSG_LEN + EMAIL_LEN];

    // Creazione socket TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket");
        exit(1);
    }

    // Configura indirizzo del server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); // Porta
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr); // IP

    // Connessione al server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect");
        exit(1);
    }

    // Login o Registrazione
    printf("Benvenuto al Chat Service!\n");
    printf("Vuoi [register] o [login]? ");
    scanf("%s", command); // Legge 'register' o 'login'
    getchar(); // Consuma il carattere newline rimasto

    // Credenziali dell'utente
    printf("Email: ");
    fgets(email, EMAIL_LEN, stdin);
    email[strcspn(email, "\n")] = '\0'; // Rimuove newline

    printf("Password: ");
    fgets(password, PASS_LEN, stdin);
    password[strcspn(password, "\n")] = '\0';

    // Prepara messaggio per server: REGISTER/LOGIN email password
    snprintf(buffer, sizeof(buffer), "%s %s %s",
             (strncmp(command, "register", 8) == 0 ? "REGISTER" : "LOGIN"),
             email, password);
    send(sockfd, buffer, strlen(buffer), 0); // Invia comando

    // Attende conferma dal server
    int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    buffer[bytes] = '\0';
    printf("%s", buffer);

    // Se fallisce la registrazione/login, chiude
    if (strstr(buffer, "❌")) {
        close(sockfd);
        return 1;
    }

    // Avvia thread per ricezione messaggi
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_handler, NULL);

    // Messaggi
    printf("💬 Inserisci messaggi nel formato:\n    destinatario@example.com Messaggio\n");
    printf("Scrivi '/quit' per uscire.\n");

    // Loop di invio messaggi
    while (1) {
        printf("> ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "/quit") == 0) break;

        send(sockfd, buffer, strlen(buffer), 0); // Invia messaggio
    }

    // Chiusura del socket
    close(sockfd);
    return 0;
}
