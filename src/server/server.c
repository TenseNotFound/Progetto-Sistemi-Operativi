/*
        SERVER POSIX RELEASE
*/

#include "../../protocollo/protocollo.h"
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG 16
#define MAX_BIND_ATTEMPTS 20

typedef game_info info;
typedef azioni mosse;
int port;

int main(int argc, char **argv){

    if(argc<2){
        printf("Sintassi corretta: %s <pnumber>\n", argv[0]);
        return 1;
    }
    
    if(argv[1] < 5000 || argv[1] >65535){
        printf("Inserisci un numero di porta valido\n");
        return 1;
    }

    printf("\n      POSIX SERVER RELEASE        \nStartup server...\n");
    fflush(stdout);

    port = atoi(argv[1]);


    int llisten = socket(AF_INET, SOCK_STREAM, 0);
    if (listen < 0) {
        perror("Errore nel socket");
        exit(-1);
    }

    int opt = 1, ret;
    ret = setsockopt(llisten, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret == -1){
        perror("Errore nella setsockopt");
        exit(-1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // ip assegnato dal DHCP

    /*
        per mettere 127.0.0.1 serve inserire la libreria #include <arpa/inet.h>
        che il prof non ha spiegato, sarebbe poi così

        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
        al posto di 
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        se serve si mette sennò va bene anche così
    */

 
    int bound = 0;
    for (int attempt = 0; attempt < MAX_BIND_ATTEMPTS && !bound; attempt++) {
        server_addr.sin_port = htons(port);
 
        if (bind(llisten, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            bound = 1;
        } else if (errno == EADDRINUSE) {
            printf("Porta %d occupata, provo una porta casuale...", port);
            port = 1024 + (rand() % (65535 - 1024));   /* range non privilegiato */
        } else {
            perror("bind() fallita");
            close(llisten);
            exit(-1);
        }
    }
 
    if (!bound) {
        perror("Impossibile trovare una porta libera dopo il numero massimo di tentativi concesso");
        close(llisten);
        exit(-1);
    }

    // dimensione del backlocg -> dimensione della coda di attesa a 16 data dal define iniziale
    if (listen(llisten, BACKLOG) < 0) {
        perror("listen() fallita");
        close(llisten);
        exit(-1);
    }

    printf("\n==================================================\n         SERVER AVVIATO CON SUCCESSO         \n==================================================\n");
    printf(" Indirizzo IP : 0.0.0.0 (INADDR_ANY)\n Porta        : %d\n", port);
    printf(" Backlog      : %d (Coda massima client)\n", BACKLOG);
    printf(" Per chiudere : CTRL + C\n==================================================\n[*] In attesa di nuove connessioni...\n\n");

    fflush(stdout);
        
    




    


    


    return 0;




}