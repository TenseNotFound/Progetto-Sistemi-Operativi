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
#include <arpa/inet.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef game_info info;
typedef azioni mosse;
typedef struct players player;

volatile sig_atomic_t timeout = 1, shutdown_flag = 0, lobby_aperta = 1;;
int port;           // eventualmente qua va poi dichiarato il semaforo

void *udp_discovery_port(void *args);
void timeout_lobby(int sig);
void chiusura (int sig);
void *client_thread (void *args);
int recv_msg(int fd, azioni *msg);
int send_msg(int fd, azioni *msg);
void *add_player(int fd, char *username);
player *trova_giocatore(int id); // serve per trovare un giocatore in base al suo id quando si vuole fare una 
                                // mossa contro di lui, così da poter aggiornare la sua griglia e il numero di navi rimaste
ssize_t readn(int fd, void *buf, size_t n); // serve per controllare l'avvenuta lettura di tutti i dati in rete
ssize_t writen(int fd, const void *buf, size_t n); // serve per controllare l'avvenuta scrittura di tutti i dati in rete
//serve perchè ho bisogno di passare più informazioni al thread, uso quindi una struttura

typedef struct{ // descrive il client
    int client_fd;
    struct sockaddr_in client_addr;
}client_arg;


typedef struct players{
    int id;
    char username[USERNAME];
    int socket;
    pthread_t thread;
    char griglia[GRID_SIZE][GRID_SIZE];
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
    int active_threads; // serve per tenere traccia di quanti thread client sono attivi, così da poterli chiudere tutti in caso di poweroff
    pthread_mutex_t lock;
} gstate;

static gstate stato = {
    NULL, 
    0,  // quanti giocatori sono connessi
    0, // quanti utenti sono già connessi
    0, // 0 -> partita non iniziata, 1 -> partita iniziata
    0, // inizialmente nessun thread attivo
    PTHREAD_MUTEX_INITIALIZER
};

int main(int argc, char **argv){

    if(argc<2){
        printf("Sintassi corretta: %s <pnumber>\n", argv[0]);
        return -1;
    }
    
    port = atoi(argv[1]);
    if(port < 5000 || port >65535){
        printf("Inserisci un numero di porta valido nel range 5000-65535\n");
        return -1;
    }

    printf("\n      POSIX SERVER RELEASE        \nStartup server...\n");
    fflush(stdout);

    srand((unsigned int)time(NULL));

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
    for (int attempt = 0; attempt < TENTATIVI && !bound; attempt++) {
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

    if(sigaction(SIGALRM, &sa, NULL) == -1){
        perror("Errore nell'installare la sigaction, procedo con l'installazione della signal");
        signal(SIGALRM, timeout_lobby);
    }
    
    sa.sa_handler = chiusura;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Errore nell'installare la sigaction per SIGINT");
        signal(SIGINT, chiusura);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Errore nell'installare la sigaction per SIGTERM");
        signal(SIGTERM, chiusura);
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
    while(timeout && !shutdown_flag){
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client = accept(llisten, (struct sockaddr *)&client_addr, &addrlen); // bloccante
        if(client == -1){
            if(errno == EINTR) continue; // se l'errore è dovuto a una segnalazione, riprovo
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

        pthread_mutex_lock(&stato.lock);
        stato.active_threads++;
        pthread_mutex_unlock(&stato.lock);

        if(pthread_create(&tid, NULL, client_thread, cargs) != 0){
            perror("Errore nella creazione del thread di gestione associato al client connesso");
            pthread_mutex_lock(&stato.lock);
            stato.active_threads--;
            pthread_mutex_unlock(&stato.lock);
            close(client);
            free(cargs);
            continue;
        }
        pthread_detach(tid);

        
    }

    lobby_aperta = 0;
    close(llisten);
    /*
          printf("\n[*] Chiusura del server in corso...\n");
    fflush(stdout);
 
    pthread_mutex_lock(&stato.lock);
 
    // sblocco eventuali recv()/read() bloccate nei thread client. shutdown()
    // e' preferibile a close() qui: chiudere un fd usato da un altro thread
    // e' una race condition, shutdown() invece sveglia in sicurezza chi e'
    // bloccato in lettura senza invalidare subito il descrittore.
    player *p = stato.head;
    while (p != NULL) {
        shutdown(p->socket, SHUT_RDWR);
        p = p->next;
    }
 
    // aspetto che i thread client terminino, con un timeout di sicurezza
    // per non restare bloccati per sempre se qualcosa va storto
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;
    while (stato.active_threads > 0) {
        if (pthread_cond_timedwait(&stato.cv, &stato.lock, &ts) == ETIMEDOUT) {
            printf("[*] Timeout in attesa dei thread client, chiudo comunque\n");
            break;
        }
    }
 
    // libero la lista dei giocatori e chiudo eventuali socket rimasti aperti
    player *cur = stato.head;
    while (cur != NULL) {
        player *next = cur->next;
        close(cur->socket);
        free(cur);
        cur = next;
    }
    stato.head = NULL;
 
    pthread_mutex_unlock(&stato.lock);
    pthread_mutex_destroy(&stato.lock);
    pthread_cond_destroy(&stato.cv);
 
    printf("[*] Server chiuso correttamente\n");
 
    */
    return 0;
}


void *client_thread(void *args){
    client_arg *cargs = (client_arg *)args;
    int fd = cargs->client_fd;

    //recupero ip e porta del client collegato
    char ip[16];
    strncpy(ip, inet_ntoa(cargs->client_addr.sin_addr), 15);
    ip[15] = '\0';    
    int portc = ntohs(cargs->client_addr.sin_port);

    printf("Nuova connessione:\n [*] Client collegato %s:%d \n", ip, portc);
    fflush(stdout);

    azioni msg;
    bzero(&msg, sizeof(msg));

    if(recv_msg(cargs->client_fd, &msg) != 0 || msg.type != JOIN){
        printf("(Server) handshake non validato per %s:%d\n", ip, portc);
        goto exit;
        return NULL;
    }

    msg.username[USERNAME -1] = '\0';

    // serve causa rischio race condition, qua basta un mutex ed un semaforo non è necessario
    pthread_mutex_lock(&stato.lock);

    player *me = add_player(fd, msg.username);
    if (me == NULL) {
        pthread_mutex_unlock(&stato.lock);
        printf("(Server) Impossibile creare struct player per %s:%d\n", ip, portc);
        goto exit;
    }
    int assigned_id = me->id;

    pthread_mutex_unlock(&stato.lock);

    azioni welcome_msg;
    bzero(&welcome_msg, sizeof(welcome_msg));
    welcome_msg.type = WELCOME;
    welcome_msg.player_id = assigned_id;

    if(send_msg(cargs->client_fd, &welcome_msg) == -1){
        printf("Errore nell'invio del messaggio di benvenuto a %s:%d\n", ip, portc);
        goto exit;
    }

    printf("Giocatore %d (\"%s\") connesso da %s:%d\n", assigned_id, msg.username, ip, portc);
    fflush(stdout);

    /*
        SE IL THREAD DEVE MORIRE QUINDI return NULL; copia e incolla questo
        pthread_mutex_lock(&stato.lock);
        stato.active_threads--; 
        pthread_mutex_unlock(&stato.lock);
        IL MUTEX SERVE SOLO NELLE ZONE NON PROTETTE, SE GIà PROTETTA OK
        METTI goto exit;
    */


    /*
        DA FARE: ricezione delle mosse e gestione del gioco, invio dei messaggi di risposta ai client
    */




exit:
    pthread_mutex_lock(&stato.lock);
    stato.active_threads--;
    pthread_mutex_unlock(&stato.lock);

    free(cargs);
    close(fd);
    return NULL;
    

}

int send_msg(int fd, azioni *msg){
    azioni packet;
    bzero(&packet, sizeof(packet));

    //carico i dati in rete
    packet.type = htonl(msg->type);
    packet.player_id = htonl(msg->player_id);
    packet.target_id = htonl(msg->target_id);
    packet.x = htonl(msg->x);
    packet.y = htonl(msg->y);
	memcpy(packet.username, msg->username, USERNAME);

    ssize_t n = writen(fd, &packet, sizeof(azioni));
    // c'è rischio che in TCP si scrivano meno byte di quelli richiesti,

    if(n != (ssize_t)(sizeof(azioni))){
        return -1;
    }
    return 0;
}

int recv_msg(int fd, azioni *msg){
    azioni packet;

    ssize_t n = readn(fd, &packet, sizeof(azioni));
    // c'è rischio che in TCP si ricevano meno byte di quelli richiesti, 
    // quindi bisogna fare un ciclo finchè non si ricevono tutti i byte

    if(n == (ssize_t)(sizeof(azioni))){

        //scarico i dati da rete
        msg->type = ntohl(packet.type);
        msg->player_id = ntohl(packet.player_id);
        msg->target_id = ntohl(packet.target_id);
        msg->x = ntohl(packet.x);
        msg->y = ntohl(packet.y);
		memcpy(msg->username, packet.username, USERNAME);

        return 0;
    } else if(n == 0){
        return -1;
    }

    return -1;
}

ssize_t readn(int fd, void *buf, size_t n){
    /*
        QUESTA FUNZIONE SERVE PER IL CONTROLLO
        DELL'AVVENUTA LETTURA DI TUTTI I DATI IN RETE
        SE NE LEGGO DI MENO CONITNUO A LEGGERE ESCLUDENDO I VARI CASI
    */
    size_t left = n;
    char *p = (char *)buf;

    while(left >0){
        ssize_t r = read(fd, p, left);
        if(r<0){
            if(errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return (ssize_t)(n - left); // il client ha chiuso la connessione, analogo chiusura PIPE
        left -= (size_t)r;
        p += r;
    }
    return (ssize_t)n; // se arrivo qui ho letto tutti i byte richiesti e comunico con n
}

ssize_t writen(int fd, const void *buff, size_t n){
    /*
        QUESTA FUNZIONE SERVE PER IL CONTROLLO
        DELL'AVVENUTA SCRITTURA DI TUTTI I DATI IN RETE
        SE NE SCRIVO DI MENO CONITNUO A SCRIVERE ESCLUDENDO I VARI CASI
    */
    size_t left = n;
    const char *p = (const char *)buff;
    while(left > 0){ // > e non < perchè è unisgned e non può essere negativo
        ssize_t w = write(fd, p, left);
        if(w<0){
            if(errno == EINTR)continue; // scrittura bloccata da segnalazione, riprovo
            return -1;
        } if(w == 0) return (ssize_t)(n - left); // il client ha chiuso la connessione, analogo chiusura PIPE
        left -= (size_t)w;
        p += w;
    }
    return (ssize_t)n; // se arrivo qui ho scritto tutti i byte richiesti e comunico con n
}

void *add_player(int fd, char *username){
    player *new_player = malloc(sizeof(player));

    if(new_player == NULL){
        perror("Errore nella malloc del nuovo giocatore");
        return NULL;
    }

    new_player->id = stato.next_id++;
    strncpy(new_player->username, username, USERNAME -1);
    new_player->username[USERNAME-1] = '\0';
    new_player->socket = fd;
    new_player->alive = 1;
    new_player->sem_id = new_player->id;

    new_player->navi_rimaste = SHIP_NUMBER;

    new_player->next = stato.head;
    stato.head = new_player;
    stato.count++;
    for(int i = 0; i < GRID_SIZE; i++){
        for(int j = 0; j < GRID_SIZE; j++){
            new_player->griglia[i][j] = '~';
        }
    }
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
    int porta = *((int *)args), n;

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_fd < 0){
        perror("Errore nella creazione del socket UDP");
        pthread_exit(NULL);
    }

    struct timeval time;
    time.tv_sec = 2; 
    time.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
        perror("Errore nell'impostare il timeout UDP");
        close(socket_fd);
        pthread_exit(NULL);
    }

    struct sockaddr_in udp_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr); // serve per dopo nelle send e recv

    //ripulisco le strutture per sicurezza
    memset(&udp_addr, 0, sizeof(udp_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(DISCOVERY_PORT); 
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(socket_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr))<0){
        perror("Errore nel bind del socket UDP");
        close(socket_fd);
        pthread_exit(NULL);
    }

    char buffer[BUFFER_SIZE], reply[BUFFER_SIZE]; // dimensioni allineate con il client, definite nel protocollo.h
    

    while(!shutdown_flag && lobby_aperta){
        buffer[0] = '\0';
        n = recvfrom(socket_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&client_addr, &client_addr_len); // bloccata al massimo per 2s
        if(n > 0){
            buffer[n] = '\0';
            if(strcmp(buffer, "DISCOVER") == 0){
                snprintf(reply, sizeof(reply), "%d", porta);
                sendto(socket_fd, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, client_addr_len);
                printf("Ricevuta richiesta di discovery da %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                fflush(stdout);
            }
        } else if(n < 0){
            if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; // se l'errore è dovuto a una segnalazione, riprovo
            perror("Errore nella ricezione del messaggio UDP");
        }
    }

    close(socket_fd);
    pthread_exit(NULL);
}

/*
    INSIEME DELLE FUNZIONI DEDITE ALLA RICEZIONE DI SEGNALAZIONI
*/

void timeout_lobby(int sig){
    timeout = 0;
}

void chiusura (int sig){
    shutdown_flag = 1;
}