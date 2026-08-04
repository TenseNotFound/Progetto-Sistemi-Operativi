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
#include <stdbool.h>

#define MAX_BIND_ATTEMPTS 20
#define GRID 10
#define SHIP_NUMBER 5

typedef game_info info;
typedef azioni mosse;

volatile sig_atomic_t timeout = 1, kill = 0;
int port;           // eventualmente qua va poi dichiarato il semaforo

void *udp_discovery_port(void *args);
void timeout_lobby(int sig);
void chiusura (int sig);
void *client_thread (void *args);
int recv_msg(int fd, azioni *msg);
int send_msg(int fd, azioni *msg);
void *add_player(int fd);
player *trova_giocatore(int id); // serve per trovare un giocatore in base al suo id quando si vuole fare una mossa contro di lui, così da poter aggiornare la sua griglia e il numero di navi rimaste

//serve perchè ho bisogno di passare più informazioni al thread, uso quindi una struttura
typedef struct{
    int client_fd;
    struct sockaddr_in client_addr;
}client_arg;


typedef struct players{
    int id;
    int socket;
    pthread_t thread;
    char griglia[GRID][GRID];
    int alive;
    int navi_rimaste;
    int sem_id;
    struct players *next;
}player;


typedef struct{
    player  *head;
    int count;
    int next_id;
    int game_started;
    pthread_mutex_t lock;
} gstate;

static gstate stato = {
    NULL, 
    0,  // quanti giocatori sono connessi
    0, // quanti utenti sono già connessi
    0, // 0 -> partita non iniziata, 1 -> partita iniziata
    PTHREAD_MUTEX_INITIALIZER
} ;

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
    if (llisten < 0) {
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

    // dimensione del backlog -> dimensione della coda di attesa a 16 data dal define iniziale
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
    
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = timeout_lobby;

    if (sigemptyset(&sa.sa_mask) == -1){
        perror("Errore nello svuotare la maschera delle segnalazioni");
        exit(-1);
    }
    /*
    if(sigaddset(&sa.sa_mask, SIGINT) == -1){
        perror("Errore nell'aggiunta di SIGINT alla maschera");
        ret++;
    }

    if(sigaddset(&sa.sa_mask, SIGUSR1) == -1){
        perror("Errore nell'aggiunta di SIGUSR1 alla maschera");
        ret++;
    }
    */

    if(sigaction(SIGALRM, &sa, NULL) == -1 || ret == 2){
        perror("Errore nell'installare la sigaction, procedo con l'installazione della signal");
        signal(SIGALRM, timeout_lobby);
    }

    pthread_t udp_thread;
    if(pthread_create(&udp_thread, NULL, udp_discovery_port, &port) != 0){
        perror("Errore nella creazione del thread per la discovery UDP");
        close(llisten);
        exit(-1);
    } else {
        pthread_detach(udp_thread);
        printf("Thread UDP discovery creato con successo\n");
        fflush(stdout);

    }
    
    printf("Lobby aperta per 30s\n");
    alarm(30);

    //lobby con timeout
    while(timeout){
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client = accept(llisten, (struct sockaddr *)&client_addr, &addrlen);
        if(client == -1){
            perror("Errore nell'accept() in ascolto del client (server)");
            continue;
        }

        client_arg *cargs = malloc(sizeof(client_arg));
        if(cargs == NULL){
            perror("Errore nella malloc");
            close(client);
            continue;
        } 

        //mi salvo tutte le informazioni riferite al client connesso nella struttura e invio poi al thread di gestione
        cargs->client_fd = client;
        cargs->client_addr = client_addr;

        pthread_t tid;

        if(pthread_create(&tid, NULL, client_thread, cargs) != 0){
            perror("Errore nella creazione del thread di gestione associato al client connesso");
            close(client);
            free(cargs);
            continue;
        }
        pthread_detach(tid);

    }

    close(llisten);

    return 0;
}


void timeout_lobby(int sig){
    timeout = 0;
}

void chiusura (int sig){
    kill = 1;
}

void *client_thread(void *args){
    client_arg *cargs = (client_arg *)args;

    //recupero ip e porta del client collegato
    char *ip = inet_ntoa(cargs->client_addr.sin_addr);
    int port = ntohs(cargs->client_addr.sin_port);

    printf("Nuova connessione:\n     client collegato %s:%d \n", ip, port);
    fflush(stdout);

    azioni msg;

    if(recv_msg(cargs->client_fd, &msg) != 0 || msg.type != JOIN){
        printf("(Server) handshake non validato per %s:%d\n", ip, port);
        free(cargs);
        close(cargs->client_fd);
        return NULL;
    }

    pthread_mutex_lock(&stato.lock);

    player *me = add_player(cargs->client_fd);
    if (me == NULL) {
        pthread_mutex_unlock(&stato.lock);
        printf("(Server) Impossibile creare struct player per %s:%d\n", ip, port);
        free(cargs);
        close(cargs->client_fd);
        return NULL;
    }
    int assignet_id = me->id;

    pthread_mutex_unlock(&stato.lock);

    azioni welcome_msg;
    memset(&welcome_msg, 0, sizeof(azioni));
    welcome_msg.type = WELCOME;
    welcome_msg.player_id = assignet_id;

    if(send_msg(cargs->client_fd, &welcome_msg) == -1){
        printf("Errore nell'invio del messaggio di benvenuto a %s:%d\n", ip, port);
        free(cargs);
        close(cargs->client_fd);
        return NULL;
    }

    printf("Giocatore %d connesso da %s:%d\n", assignet_id, ip, port);
    fflush(stdout);


    /*
        DA FARE: ricezione delle mosse e gestione del gioco, invio dei messaggi di risposta ai client
    */



    free(cargs);
    close(cargs->client_fd);
    return NULL;

}

int send_msg(int fd, azioni *msg){
    azioni packet;

    //carico i dati in rete
    packet.type = htonl(msg->type);
    packet.player_id = htonl(msg->player_id);
    packet.target_id = htonl(msg->target_id);
    packet.x = htonl(msg->x);
    packet.y = htonl(msg->y);

    ssize_t n = write(fd, &packet, sizeof(azioni));

    if(n != (ssize_t)(sizeof(azioni))){
        return -1;
    }
    return 0;
}

int recv_msg(int fd, azioni *msg){
    azioni packet;

    ssize_t n = read(fd, &packet, sizeof(azioni));

    if(n == (ssize_t)(sizeof(azioni))){

        //scarico i dati da rete
        msg->type = ntohl(packet.type);
        msg->player_id = ntohl(packet.player_id);
        msg->target_id = ntohl(packet.target_id);
        msg->x = ntohl(packet.x);
        msg->y = ntohl(packet.y);

        return 0;
    } else if(n == 0){
        return -1;
    }

    return -1;
}

void *add_player(int fd){
    player *new_player = malloc(sizeof(player));

    if(new_player == NULL){
        perror("Errore nella malloc del nuovo giocatore");
        return NULL;
    }

    new_player->id = stato.next_id++;
    new_player->socket = fd;
    new_player->alive = 1;
    new_player->sem_id = new_player->id;

    new_player->navi_rimaste = SHIP_NUMBER;

    new_player->next = stato.head;
    stato.head = new_player;
    stato.count++;

    return new_player;
}

player *trova_giocatore(int id){
    player *current = stato.head;

    while(current != NULL){
        if(current->id == id){
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void *udp_discovery_port(void *args){
    int porta = *((int *)args);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_fd < 0){
        perror("Errore nella creazione del socket UDP");
        pthread_exit(NULL);}
    struct sockaddr_in udp_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    //ripulisco le strutture per sicurezza
    memset(&udp_addr, 0, sizeof(udp_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(9999); // porta di discovery
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(socket_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr))<0){
        perror("Errore nel bind del socket UDP");
        close(socket_fd);
        pthread_exit(NULL);
    }

    char buffer[256], reply[256];
    buffer[0] = '\0';

    while(1){
        int n = recvfrom(socket_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if(n > 0 && strcmp(buffer, "DISCOVER") == 0){
            snprintf(reply, sizeof(reply), "%d", porta);
            sendto(socket_fd, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, client_addr_len);
        } else if(n < 0){
            perror("Errore nella ricezione del messaggio UDP");
        } else {
            continue; // messaggio non valido, ignoro
        }
    }

    close(socket_fd);
    pthread_exit(NULL);
}