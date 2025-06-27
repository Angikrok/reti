/* Tic-Tac-Toe client – versione “lettere A-I”              */
/* gcc -Wall -O2 client.c -o client                         */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSZ 256

ssize_t recvline(int sock, char *buf, size_t max) {
    size_t i = 0; char c;
    while (i < max - 1) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) return n;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = 0; 
    return (ssize_t)i;
}
void print_board(const char *f) {
    puts("+---+---+---+");
    for (int r = 0; r < 3; ++r) {
        printf("|");
        for (int c = 0; c < 3; ++c) {
            printf(" %c |", f[r*3+c]);
        }
        puts("\n+---+---+---+");
    }
}
int main(int argc, char *argv[]) {
    if (argc != 3) { 
        fprintf(stderr,"Uso: %s <ip> <porta>\n",argv[0]); exit(1);
    }

    int sock = socket(AF_INET,SOCK_STREAM,0); 
    
    if (sock<0){perror("socket");exit(1);}

    struct sockaddr_in srv={0}; 
    srv.sin_family=AF_INET;
    srv.sin_port=htons((uint16_t)atoi(argv[2]));

    if(inet_pton(AF_INET,argv[1],&srv.sin_addr)!=1){
        puts("IP errato");exit(1);
    }

    if(connect(sock,(struct sockaddr*)&srv,sizeof(srv))<0){
        perror("connect");exit(1);
    }

    char buf[BUFSZ]; printf("Connesso.\n");
    while (1) {
        ssize_t n=recvline(sock,buf,sizeof(buf)); 
        if(n<=0){
            puts("Server chiuso");break;
        }
        if(!strncmp(buf,"BOARD",5)){ 
            print_board(buf+6); 
        }
        else if(!strncmp(buf,"YOUR_TURN",9)){
            char m; 
            
            do{
                printf("Lettera (A-I): "); 
                fflush(stdout);
                scanf(" %c",&m); if(m>='a'&&m<='i') m-='a'-'A';
            }while(m<'A'||m>'I');

            snprintf(buf,sizeof(buf),"MOVE %c\n",m); 
            
            send(sock,buf,strlen(buf),0);
        }

        else if(!strncmp(buf,"INVALID",7)){ 
            puts("Mossa non valida, riprova!"); 
        }
        else if(!strncmp(buf,"WIN",3)){ 
            puts("*** HAI VINTO! ***"); 
            break;
        }
        else if(!strncmp(buf,"LOSE",4)){ 
            puts("*** Hai perso. ***"); 
            break;
        }
        else if(!strncmp(buf,"DRAW",4)){ 
            puts("*** Pareggio. ***"); 
            break;
        }
        else if(!strncmp(buf,"ABORT",5)){ 
            puts("*** Avversario disconnesso. ***"); 
            break;
        }
    }
    close(sock); return 0;
}
