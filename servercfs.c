#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 12345
#define MAX_PLAYERS 2
#define BUFFER_SIZE 100
#define MAX_ROUNDS 3

enum { ROCK = 1, PAPER = 2, SCISSORS = 3 };

typedef struct {
    int sockfd;
    char name[50];
    int move;
    int score;
} player_t;

player_t players[MAX_PLAYERS];
int connected_players = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int evaluate(int p1, int p2) {
    if (p1 == p2) return 0;
    if ((p1 == ROCK && p2 == SCISSORS) ||
        (p1 == SCISSORS && p2 == PAPER) ||
        (p1 == PAPER && p2 == ROCK)) {
        return 1;
    }
    return 2;
}

void *client_handler(void *arg) {
    int idx = *(int *)arg; //prende il valore passato come puntatore e lo mette nella variabile idx
    free(arg);

    char buffer[BUFFER_SIZE];
    player_t *player = &players[idx];

    // Riceve nome giocatore
    int n = recv(player->sockfd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(player->sockfd);
        return NULL;
    }
    buffer[n] = '\0';
    strncpy(player->name, buffer, sizeof(player->name));

    printf("Giocatore connesso: %s\n", player->name);

    // Attendi entrambi i giocatori
    while (1) {
        pthread_mutex_lock(&mutex);
        int ready = (connected_players == MAX_PLAYERS);
        pthread_mutex_unlock(&mutex);
        if (ready) break;
        sleep(1);
    }

    int round;
    for (round = 1; round <= MAX_ROUNDS; round++) {
        // Invia messaggio round corrente
        snprintf(buffer, sizeof(buffer), "\n--- Round %d di %d ---\n", round, MAX_ROUNDS);
        send(player->sockfd, buffer, strlen(buffer), 0);

        // Chiede la mossa
        send(player->sockfd, "Inserisci la tua mossa (1=Rock, 2=Paper, 3=Scissors): ", sizeof(msg), 0);

        // Riceve mossa
        n = recv(player->sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';
        player->move = atoi(buffer);

        printf("%s ha scelto %d\n", player->name, player->move);

        // Attendi mossa avversario
        while (1) {
            pthread_mutex_lock(&mutex);
            int both_moved = (players[0].move != 0 && players[1].move != 0);
            pthread_mutex_unlock(&mutex);
            if (both_moved) break;
            sleep(1);
        }

        // Valuta vincitore del round
        pthread_mutex_lock(&mutex);
        int winner = evaluate(players[0].move, players[1].move);
        if (winner == 1) players[0].score++;
        else if (winner == 2) players[1].score++;
        pthread_mutex_unlock(&mutex);

        char msg[BUFFER_SIZE];
        if (winner == 0) {
            snprintf(msg, sizeof(msg), "Round %d: Pareggio! Entrambi hanno scelto %d.\n", round, player->move);
        } else if ((winner == 1 && idx == 0) || (winner == 2 && idx == 1)) {
            snprintf(msg, sizeof(msg), "Round %d: Hai vinto! Tu: %d, Avversario: %d\n", round, player->move, players[1 - idx].move);
        } else {
            snprintf(msg, sizeof(msg), "Round %d: Hai perso! Tu: %d, Avversario: %d\n", round, player->move, players[1 - idx].move);
        }

        send(player->sockfd, msg, strlen(msg), 0);

        // Reset mosse per il prossimo round
        pthread_mutex_lock(&mutex);
        players[0].move = 0;
        players[1].move = 0;
        pthread_mutex_unlock(&mutex);
    }

    // Dopo tutti i round, invia risultato finale
    pthread_mutex_lock(&mutex);
    int my_score = player->score;
    int opp_score = players[1 - idx].score;
    pthread_mutex_unlock(&mutex);

    if (my_score > opp_score) {
        snprintf(buffer, sizeof(buffer), "\nHai vinto la partita! Punteggio: %d - %d\n", my_score, opp_score);
    } else if (my_score < opp_score) {
        snprintf(buffer, sizeof(buffer), "\nHai perso la partita! Punteggio: %d - %d\n", my_score, opp_score);
    } else {
        snprintf(buffer, sizeof(buffer), "\nLa partita è finita in pareggio! Punteggio: %d - %d\n", my_score, opp_score);
    }
    send(player->sockfd, buffer, strlen(buffer), 0);

    printf("%s si è disconnesso\n", player->name);
    close(player->sockfd);
    return NULL;
}

int main() {
    int server_fd, *client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    listen(server_fd, MAX_PLAYERS);
    printf("Server Rock Paper Scissors in ascolto sulla porta %d\n", PORT);

    while (connected_players < MAX_PLAYERS) {
        client_sock = malloc(sizeof(int));
        *client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (*client_sock < 0) {
            perror("accept");
            free(client_sock);
            continue;
        }

        pthread_mutex_lock(&mutex);
        players[connected_players].sockfd = *client_sock;
        players[connected_players].move = 0;
        players[connected_players].score = 0;
        connected_players++;
        pthread_mutex_unlock(&mutex);

        int *idx = malloc(sizeof(int));
        *idx = connected_players - 1;
        pthread_t t;
        pthread_create(&t, NULL, client_handler, idx);
        pthread_detach(t);
    }

    while (1) sleep(10);

    close(server_fd);
    return 0;
}
