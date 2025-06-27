#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 12345
#define MAX_CLIENTS 100
#define EMAIL_LEN 50
#define PASS_LEN 50
#define MSG_LEN 256
#define DB_FILE "users.db"  // File usato come database

typedef struct {
    int sockfd;
    char email[EMAIL_LEN];
} client_t;

client_t clients[MAX_CLIENTS];  // Elenco client connessi
int client_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // Per proteggere accessi concorrenti

// ✅ Funzione per caricare gli utenti registrati dal file
int is_registered(const char *email, const char *password) {
    FILE *fp = fopen(DB_FILE, "r");
    if (!fp) return 0;

    char line[EMAIL_LEN + PASS_LEN + 10];
    char stored_email[EMAIL_LEN], stored_pass[PASS_LEN];

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%s %s", stored_email, stored_pass);
        if (strcmp(stored_email, email) == 0 &&
            strcmp(stored_pass, password) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

// ✅ Funzione per aggiungere un nuovo utente al file
int register_user(const char *email, const char *password) {
    if (is_registered(email, password)) return 0;

    FILE *fp = fopen(DB_FILE, "a");
    if (!fp) return 0;

    fprintf(fp, "%s %s\n", email, password);
    fclose(fp);
    return 1;
}

// ✅ Funzione per trovare il socket di un destinatario
int get_client_socket_by_email(const char *email) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].email, email) == 0) {
            return clients[i].sockfd;
        }
    }
    return -1;
}

// ✅ Funzione thread: gestisce ogni client
void *client_handler(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[MSG_LEN + EMAIL_LEN];
    char email[EMAIL_LEN] = {0}, password[PASS_LEN];

    // Riceve comando iniziale (REGISTER o LOGIN)
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        close(client_sock);
        return NULL;
    }
    buffer[bytes] = '\0';

    char cmd[10];
    sscanf(buffer, "%s %s %s", cmd, email, password);

    // Gestione registrazione/login
    if (strcmp(cmd, "REGISTER") == 0) {
        if (register_user(email, password)) {
            send(client_sock, "✅ Registrazione avvenuta con successo\n", 40, 0);
        } else {
            send(client_sock, "❌ Registrazione fallita: utente esistente\n", 44, 0);
            close(client_sock);
            return NULL;
        }
    } else if (strcmp(cmd, "LOGIN") == 0) {
        if (!is_registered(email, password)) {
            send(client_sock, "❌ Login fallito: credenziali errate\n", 38, 0);
            close(client_sock);
            return NULL;
        } else {
            send(client_sock, "✅ Login avvenuto con successo\n", 32, 0);
        }
    } else {
        send(client_sock, "❌ Comando sconosciuto\n", 23, 0);
        close(client_sock);
        return NULL;
    }

    // Aggiunge il client alla lista degli attivi
    pthread_mutex_lock(&mutex);
    if (client_count < MAX_CLIENTS) {
        clients[client_count].sockfd = client_sock;
        strncpy(clients[client_count].email, email, EMAIL_LEN);
        client_count++;
    }
    pthread_mutex_unlock(&mutex);

    // Ricezione messaggi da questo client
    while ((bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';

        // Atteso formato: destinatario@example.com Messaggio...
        char dest_email[EMAIL_LEN], message[MSG_LEN];
        char *space = strchr(buffer, ' ');
        if (!space) continue;

        *space = '\0';
        strcpy(dest_email, buffer);
        strcpy(message, space + 1);

        // Prepara messaggio con mittente
        char formatted_msg[MSG_LEN + EMAIL_LEN + 50];
        snprintf(formatted_msg, sizeof(formatted_msg),
                 "📨 Da %s: %s", email, message);

        int dest_sock = get_client_socket_by_email(dest_email);
        if (dest_sock >= 0) {
            send(dest_sock, formatted_msg, strlen(formatted_msg), 0);
        } else {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg),
                     "⚠️ Utente %s non connesso\n", dest_email);
            send(client_sock, error_msg, strlen(error_msg), 0);
        }
    }

    // Rimuove client dalla lista se si disconnette
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sockfd == client_sock) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    close(client_sock);
    return NULL;
}

int main() {
    int server_fd, *client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Crea socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket");
        exit(1);
    }

    // Imposta parametri del server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind alla porta
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind");
        exit(1);
    }

    // Inizia ad ascoltare
    listen(server_fd, 10);
    printf("🟢 Server in ascolto sulla porta %d...\n", PORT);

    // Loop per accettare connessioni
    while (1) {
        client_sock = malloc(sizeof(int));
        *client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (*client_sock < 0) {
            perror("Accept");
            continue;
        }

        // Crea un thread per ogni client connesso
        pthread_t t;
        pthread_create(&t, NULL, client_handler, client_sock);
        pthread_detach(t); // Libera risorse al termine
    }

    close(server_fd);
    return 0;
}
