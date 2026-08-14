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

volatile sig_atomic_t timeout = 1, lobby_aperta = 1, shutdown_flag = 0;
int port, sem1 = 0, sem2 = 0, n = 0;           
/*
    sem1-> serve per i turni
    sem2-> serve per dare il via libera a tutti dopo che anche l'ultimo client connesso ha finito di disporre le navi
    n-> mi serve per il numero totali di gettoni che andranno nei semafori
*/

void *udp_discovery_port(void *args);
void timeout_lobby(int sig);
void chiusura (int sig);
void *client_thread (void *args);
int recv_msg(int fd, azioni *msg);
int send_msg(int fd, azioni *msg);
void *add_player(int fd, char *username);
void remove_player(int id);
player *trova_giocatore(int valore, bool flag); // serve per trovare un giocatore in base al suo id quando si vuole fare una 
                                // mossa contro di lui, così da poter aggiornare la sua griglia e il numero di navi rimaste
                                // si può cercare tramite id (flag = false) oppure sem_id (flag = true)
ssize_t readn(int fd, void *buf, size_t n); // serve per controllare l'avvenuta lettura di tutti i dati in rete
ssize_t writen(int fd, const void *buf, size_t n); // serve per controllare l'avvenuta scrittura di tutti i dati in rete
bool validazione_formazione(char board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, int dimensione_nave, int indice); // serve per il check della formazione in entrata, INTEGRITY CHECK
int ricezione_navi(int fd, player *me); // serve per ricevere la formazione che manda il client
void broadcast(azioni *esito); // serve nel client_thread per fare le comunicazioni a tutti gli utenti 


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
    int fine;
    int active_threads; // serve per tenere traccia di quanti thread client sono attivi, così da poterli chiudere tutti in caso di poweroff
    pthread_mutex_t lock; // serve per proteggere questi valori, senza che lo dichiaro globale lo metto qui dentro
} gstate;

static gstate stato = {
    NULL, 
    0,  // quanti giocatori sono connessi
    0, // quanti utenti sono già connessi
    0, // inizializzazione flag per la fine
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
        exit(EXIT_FAILURE);
    }

    int opt = 1, ret;
    ret = setsockopt(llisten, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret == -1){
        perror("Errore nella setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // ip assegnato dal DHCP

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
        exit(EXIT_FAILURE);
    }

    // dimensione del backlog -> dimensione della coda di attesa, definita nel protocollo
    if (listen(llisten, BACKLOG) < 0) {
        perror("listen() fallita");
        close(llisten);
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    if(sigaction(SIGALRM, &sa, NULL) == -1){
        perror("Errore nell'installare la sigaction, procedo con l'installazione della signal");
        if(signal(SIGALRM, timeout_lobby) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            exit(EXIT_FAILURE);
        }
    }
    
    sa.sa_handler = chiusura;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Errore nell'installare la sigaction per SIGINT");
        if(signal(SIGINT, chiusura) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            exit(EXIT_FAILURE);
        }
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Errore nell'installare la sigaction per SIGTERM");
        if(signal(SIGTERM, chiusura) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            exit(EXIT_FAILURE);
        }
    }

    pthread_t udp_thread;
    
    if(pthread_create(&udp_thread, NULL, udp_discovery_port, &port) != 0){
        perror("Errore nella creazione del thread per la discovery UDP");
        close(llisten);
        exit(EXIT_FAILURE);
    } else {
        pthread_detach(udp_thread);
        printf("Thread UDP discovery creato con successo\n");
        fflush(stdout);

    }

    sem2 = semget(IPC_PRIVATE, 1, IPC_CREAT|0664);
    if(sem2 == -1){
        perror("Errore nella creazione del semaforo per il controllo che tutti i player hanno terminato l'invio della formazione");
        exit(EXIT_FAILURE);
    }

    semctl(sem2, 0, SETVAL, 0); // inizialmente ho 0 gettoni, poi mi faccio fare le post su questo semaforo e quando torna a 0 sblocco tutto

    printf("Lobby aperta per 30s\n");
    alarm(30);
 
    //lobby con timeout
    while(timeout && !shutdown_flag){
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client = accept(llisten, (struct sockaddr *)&client_addr, &addrlen); // bloccante
        if(client == -1){
            if(errno == EINTR) continue;
            perror("Errore nell'accept() in ascolto del client");
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
    pthread_mutex_lock(&stato.lock);
    n = stato.count;
    pthread_mutex_unlock(&stato.lock);

    if(n == 1){
        printf("[*] Attenzione: un solo client collegato, startup del bot...\n");
        fflush(stdout);
        pid_t pid = fork();
        if(pid == 0){
            char porta[10];
            snprintf(porta, sizeof(porta), "%d", port);
            execl(".././bot", "bot", "127.0.0.1", porta, NULL);
            perror("Errore nello startup del bot");
            exit(1);
        }

        struct sockaddr_in bot;
        socklen_t lbot = sizeof(bot);
        int bot_fd = accept(llisten, (struct sockaddr *)&bot, &lbot);

        if(bot_fd != -1){
            client_arg *cbot = malloc(sizeof(client_arg));
            if(cbot == NULL){
                perror("Errore nella malloc");
                goto exit;
            }

            cbot->client_fd = bot_fd;
            cbot->client_addr = bot;

            pthread_t tid;
            pthread_mutex_lock(&stato.lock);
            stato.active_threads++;
            pthread_mutex_unlock(&stato.lock);

            if(pthread_create(&tid, NULL, client_thread, cbot) != 0){
                perror("Errore nella creazione del thread di gestione del bot");
                pthread_mutex_lock(&stato.lock);
                stato.active_threads--;
                pthread_mutex_unlock(&stato.lock);
                close(bot_fd);
                free(cbot);
                goto exit;
            }
            pthread_detach(tid);

        }
        
        n++;
    }

    close(llisten);
    

    if(n > 0){
        int i = 0;

        pthread_mutex_lock(&stato.lock);
        for(player *p = stato.head; p!= NULL; p = p->next){
            // questo for serve per rinumerare gli id dei semafori perchè un client potrebbe scollegarsi anche in lobby
            p->sem_id = i++;
        }

        pthread_mutex_unlock(&stato.lock);

        sem1 = semget(IPC_PRIVATE, n, IPC_CREAT|0664);

        if(sem1 == -1){
            perror("Errore nella creazione del semaforo per i turni dei client connessi");
            goto exit;
        }

        for(int i = 0; i<n; i++){
            semctl(sem1, i, SETVAL, 0);
        }

        printf("[*] Lobby chiusa, ricezione nuove richieste di partecipazione bloccate\n");
        fflush(stdout);

        int ret;
        struct sembuf sem;
        sem.sem_flg = 0;
        sem.sem_op = -n; // -n così mi fanno le post e torna a 0
        sem.sem_num = 0;

try:
        if ((ret =semop(sem2, &sem, 1)) == -1 && errno != EINTR){
            perror("Errore nella semop");
            goto exit;
        } else if (ret == -1) goto try;
        

        printf("[*] Startup della partita\n");

        struct sembuf startup;
        startup.sem_flg = 0;
        startup.sem_num = 0;
        startup.sem_op = 1;

start:
        if((ret = semop(sem1, &startup, 1)) == -1 && errno != EINTR){
            perror("Errore nell'avviare i player");
            goto exit;
        } else if (ret == -1) goto start;

    }

    printf("[*] Partita avviata con successo, player 1 abilitato, vado in background \n");
    fflush(stdout);
    
    while(!shutdown_flag){
        pthread_mutex_lock(&stato.lock);
        int attivi = stato.active_threads;
        pthread_mutex_unlock(&stato.lock);
        
        if (attivi == 0) break;

        sleep(1);
    }

exit:
    printf("\n[*] Routine di chiusura avviata. Pulizia risorse in corso...\n");
    fflush(stdout);

    if (sem1 > 0) semctl(sem1, 0, IPC_RMID);

    if (sem2 > 0) semctl(sem2, 0, IPC_RMID);
    
    if (llisten > 0) close(llisten);

    pthread_mutex_lock(&stato.lock);
    player *curr = stato.head, *temp;
    while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    stato.head = NULL; 
    pthread_mutex_unlock(&stato.lock);
    
    printf("[*] Server chiuso correttamente!\n");
    return 0;
}


void *client_thread(void *args){
    client_arg *cargs = (client_arg *)args;
    player *me = NULL;
    int fd = cargs->client_fd;

    //recupero ip e porta del client collegato
    char ip[16];
    strncpy(ip, inet_ntoa(cargs->client_addr.sin_addr), 15);
    ip[15] = '\0';    
    int portc = ntohs(cargs->client_addr.sin_port);

    printf("Nuova connessione:\n [*] Client collegato %s:%d \n", ip, portc);
    fflush(stdout);

    azioni msg;
    memset(&msg, 0, sizeof(msg));

    if(recv_msg(cargs->client_fd, &msg) != 0 || msg.type != JOIN){
        printf("(Server) handshake non validato per %s:%d\n", ip, portc);
        goto exit;
        return NULL;
    }

    msg.username[USERNAME -1] = '\0';

    me = add_player(fd, msg.username);

    if (me == NULL) {
        printf("(Server) Impossibile creare struct player per %s:%d\n", ip, portc);
        goto exit;
    }

    azioni welcome_msg;
    memset(&welcome_msg, 0, sizeof(welcome_msg));
    welcome_msg.type = WELCOME;
    welcome_msg.player_id = me->id;

    if(send_msg(cargs->client_fd, &welcome_msg) == -1){
        printf("Errore nell'invio del messaggio di benvenuto a %s:%d\n", ip, portc);
        goto exit;
    }

    printf("Giocatore %d (\"%s\") connesso da %s:%d\n", me->id, msg.username, ip, portc);

    azioni gamemode;
    memset(&gamemode, 0, sizeof(gamemode));
    if(recv_msg(fd, &gamemode) != 0 || gamemode.type != MODE){
        /*
            IMPLEMENTARE UN HANDLE PER LA GAMEMODE
        */
        printf("[!] Errore nella ricezione della gamemode dal client %s:%d\n", ip, portc);
        goto exit;
    }

    printf("[*] In attesa della formazione da %s:%d (ID: %d)", ip, portc, me->id);
    fflush(stdout);

    if(ricezione_navi(fd, me) != 0){
        printf("Formazione ricevuta non valida");
        goto exit;
    }

    printf("[*] Formazione ricevuta correttamente (%s)!\n", me->username);
    fflush(stdout);

    struct sembuf ready;
    ready.sem_flg = 0;
    ready.sem_num = 0;
    ready.sem_op = 1;

    int ret;
    bool fine_partita;

post:
    if((ret = semop(sem2, &ready, 1)) == -1 && errno != EINTR){
        perror("Impossibile segnalare l'avvenuta ricezione in sem2");
        goto exit;
    } else if(ret == -1) goto post;

    
    while(1){
        struct sembuf sem;
        sem.sem_flg = 0;
        sem.sem_num = me->sem_id;
        sem.sem_op = -1;

gback:
        if((ret = semop(sem1, &sem, 1)) == -1 && errno != EINTR){  
            perror("Errore nel prendere il gettone");
            goto exit;
        } else if(ret == -1) goto gback;

        pthread_mutex_lock(&stato.lock);
        fine_partita = stato.fine;
        pthread_mutex_unlock(&stato.lock);

        if(fine_partita) goto exit;

        pthread_mutex_lock(&stato.lock);
        int id = me->id;
        int alive = me->alive;
        int semid = me->sem_id;
        char username[USERNAME];
        strncpy(username, me->username, USERNAME);
        pthread_mutex_unlock(&stato.lock);

        if(!alive) goto exit;

        pthread_mutex_lock(&stato.lock);
        for(player *p  = stato.head; p != NULL; p = p->next){
            if(p->id != id && p->alive){
                azioni info;
                memset(&info, 0 , sizeof(info));
                info.type = INFO;
                info.player_id = p->id;
                strncpy(info.username, p->username, USERNAME-1);
                if(send_msg(me->socket, &info) == -1){
                    printf("Errore nell'invio del pacchetto di informazioni agli altri player");
                    pthread_mutex_unlock(&stato.lock);
                    goto exit;
                }
            }
        }
        
        pthread_mutex_unlock(&stato.lock);

        azioni turno;
        memset(&turno, 0, sizeof(turno));
        turno.type = TURN;
        turno.player_id = id;
        if(send_msg(fd, &turno) == -1){
            printf("Errore nel comunicare il turno a %s\n, rimuovo il player\n", username);
            fflush(stdout);
            pthread_mutex_lock(&stato.lock);
            me->alive = 0;
            pthread_mutex_unlock(&stato.lock);
            goto exit;
        }

        azioni mossa;
        if(recv_msg(fd, &mossa) == -1 || mossa.type != MOVE){
            printf("è stata ricevuta una mossa non valida oppure è stata persa la connessione da %s:%d, rimuovo il player\n", username, id);
            fflush(stdout);
            pthread_mutex_lock(&stato.lock);
            me->alive = 0;
            pthread_mutex_unlock(&stato.lock);
            goto next_turn; 
        }

        azioni esito;
        memset(&esito, 0, sizeof(esito));
        esito.player_id = id;
        esito.target_id = mossa.target_id;
        esito.x = mossa.x;
        esito.y = mossa.y;
        bool eliminato = false;

        pthread_mutex_lock(&stato.lock);
        player *bersaglio = trova_giocatore(mossa.target_id, false);
        
        
        if(bersaglio == NULL || !bersaglio->alive || mossa.x < 0 || mossa.y < 0 || mossa.x >= GRID_SIZE || mossa.y >= GRID_SIZE){
            printf("Bersaglio specificato o coordinate ricevute non valide\n");
            fflush(stdout);
            esito.type = MISS;
        } else {
            char cella = bersaglio->griglia[mossa.x][mossa.y];
            bool colpito = (cella >= '0' && cella <= '4');

            if(colpito){
                bersaglio->griglia[mossa.x][mossa.y] = 'X';
                esito.type = HIT;
                bool affondata = true;
                for(int i = 0; i<GRID_SIZE; i++){
                    for(int j = 0; j<GRID_SIZE; j++){

                        if(bersaglio->griglia[i][j] == cella){ // se trovo un altro elemento di quella nave, vuol dire che ancora non è affondata
                            affondata = false;
                            break;
                        }
                    }
                    if(!affondata) break; // nel caso abbia già trovato una nave esco 
                }

                if(affondata){
                    bersaglio->navi_rimaste--;
                    printf("[*] Il giocatore %s:%d ha affondato la nave di %s:%d!\n", username, id, bersaglio->username, bersaglio->id);

                    if(bersaglio->navi_rimaste <= 0){
                        bersaglio->alive = 0;
                        eliminato = true;
                    }

                }
                
            } else {
                esito.type = MISS;
                if(bersaglio->griglia[mossa.x][mossa.y] == '~') {
                    bersaglio->griglia[mossa.x][mossa.y] = 'O'; 
                }
            }
        }
        
        broadcast(&esito);

        pthread_mutex_unlock(&stato.lock);

        if(eliminato){
            azioni gameover;
            memset(&gameover, 0, sizeof(gameover));
            gameover.type = ELIMINATED;
            gameover.target_id = bersaglio->id;
            
            pthread_mutex_lock(&stato.lock);
            broadcast(&gameover);
            pthread_mutex_unlock(&stato.lock);
        }

        // poichè a priori non so se il prossimo giocatore è stato eliminato, devo trovare il prossimo ancora vivo e poi passare l'id alla sembuf
next_turn:
        pthread_mutex_lock(&stato.lock);
        int prossimo = -1; 
        int test = semid;
        for(int i = 0; i<n; i++){
            test = (test +1) %n;
            player *p = trova_giocatore(test, true); // true specifica se ricerca per numero di semaforo, false solo con id
            if(p != NULL && p->alive){  
                prossimo = test;
                break;
            }
        }

        if(prossimo == -1 && !stato.fine){
            stato.fine = 1;
            fine_partita = true;
        }

        pthread_mutex_unlock(&stato.lock);
        
        if(fine_partita){
            azioni win; 
            memset(&win, 0, sizeof(win));
            win.type = WIN;
            win.player_id = id;

            pthread_mutex_lock(&stato.lock);
            broadcast(&win);
            pthread_mutex_unlock(&stato.lock);
            goto exit;

        } else if (prossimo == -1) goto exit;
        sem.sem_op = 1;
        sem.sem_num = prossimo;

pass: 
        if((ret = semop(sem1, &sem, 1)) == -1 && errno != EINTR){
            perror("Errore nel rilasciare il gettone del semaforo al prossimo player");
            goto exit;
        } else if(ret == -1) goto pass;
    }
    

exit:
    pthread_mutex_lock(&stato.lock);
    stato.active_threads--;

    if(me != NULL){
        remove_player(me->id);
    }
    pthread_mutex_unlock(&stato.lock);

    free(cargs);
    close(fd);
    pthread_exit(NULL);
    
}

int send_msg(int fd, azioni *msg){
    azioni packet;
    memset(&packet, 0, sizeof(packet));

    //carico i dati in rete
    packet.type = htonl(msg->type);
    packet.player_id = htonl(msg->player_id);
    packet.target_id = htonl(msg->target_id);
    packet.x = htonl(msg->x);
    packet.y = htonl(msg->y);
	packet.gamemode = htonl(msg->gamemode);
    
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
        msg->gamemode = ntohl(packet.gamemode);
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

    // serve per aggiungere un nuovo player, non necessita di lock su mutex stato.lock perchè viene già fatto qui dentro
    player *new_player = malloc(sizeof(player));

    if(new_player == NULL){
        perror("Errore nella malloc del nuovo giocatore");
        return NULL;
    }

    for(int i = 0; i < GRID_SIZE; i++){
        for(int j = 0; j < GRID_SIZE; j++){
            new_player->griglia[i][j] = '~';
        }
    }

    strncpy(new_player->username, username, USERNAME -1);
    new_player->username[USERNAME-1] = '\0';
    new_player->socket = fd;
    new_player->alive = 1;
    
    new_player->navi_rimaste = SHIP_NUMBER;

    pthread_mutex_lock(&stato.lock);

    stato.next_id++;
    new_player->id = stato.next_id;
    new_player->sem_id = new_player->id -1;

    new_player->next = stato.head;
    stato.head = new_player;
    stato.count++;
    
    pthread_mutex_unlock(&stato.lock);

    return new_player;
}

void remove_player(int id){
    // serve per rimuovere i giocatori in fase di chiusura o per altri scenari
    player *curr = stato.head;
    player *prev = NULL;

    while(curr != NULL){
        if(curr->id == id){
            if(prev == NULL){
                stato.head = curr->next;
            } else {
                prev->next = curr->next;
            }
            stato.count--;
            printf("[*] Giocatore %d (%s) rimosso con successo!", curr->id, curr->username);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    return;
}

player *trova_giocatore(int valore, bool flag){
    // permette di trovare un giocatore tramite id (con flag = false) oppure sem_id (con flag = true)
    player *current = stato.head;
    int mode;

    while(current != NULL){
        if(flag){
            mode = current->sem_id;
        } else {
            mode = current->id;
        }

        if(mode == valore){
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void *udp_discovery_port(void *args){
    int porta = *((int *)args), socket_fd = socket(AF_INET, SOCK_DGRAM, 0), n;
    
    if(socket_fd < 0){
        perror("Errore nella creazione del socket UDP");
        pthread_exit(NULL);
    }

    struct timeval time; // dal man di setsockopt
    time.tv_sec = 2; 
    time.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
        perror("errore nel setup del timer in setsockopt");
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

    char buffer[BUFFER_SIZE], reply[BUFFER_SIZE]; 

    while(lobby_aperta){
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

bool validazione_formazione(char board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, int dimensione_nave, int indice){
    // la funzione si occupa di controllare e validare il piazzamento andando a controllare i limiti di griglia 
	// marcando quale posizione risulta occupata, non è il controllo ufficiale ma serve solo in fase di posizionamento
	// il vero controllo poi lo rifarà anche il server.
    // funzione del tutto analoga a quella che c'è in client.c
    // indice indica quale nave è arrivata
    int delta_riga, delta_colonna, r, c;
	
	if ( orientazione == 'N' ) { delta_riga = -1; delta_colonna = 0;}
	else if ( orientazione == 'S' ) { delta_riga = 1; delta_colonna = 0;}
	else if ( orientazione == 'E' ) { delta_riga = 0; delta_colonna = 1;}
	else if ( orientazione == 'O' ) { delta_riga = 0; delta_colonna = -1;}
	else return false;
	
	for (int i = 0; i < dimensione_nave; i++) {
        r = x + i * delta_riga;
        c = y + i * delta_colonna;
        
        if (r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE) {
            return false; 
        } else if (board[r][c] != '~') {
            return false; 
        }
    }

	for ( int i = 0; i < dimensione_nave; i++ ) {
		r = x + i * delta_riga;
		c = y + i * delta_colonna;
		board[r][c] = '0' + indice; 
	}
	return true;
}

int ricezione_navi (int fd, player *me){
    // azzera la griglia dell'utente e riceve la formazione
    for(int i = 0; i<GRID_SIZE; i++){
        for (int j = 0; j<GRID_SIZE; j++){
            me->griglia[i][j] = '~';
        }
    }

    int index /*della nave*/, x, y;
    char orientazione;
    for(int i = 0; i<SHIP_NUMBER; i++){
        posizionamento p; 
        memset(&p, 0, sizeof(p));
        if(readn(fd, &p, sizeof(p))<0){
            perror("Errore nella ricezione della formazione ");
            return -1;
        }

        index = ntohl(p.index);
        x = ntohl(p.x);
        y = ntohl(p.y);
        orientazione = p.orientation;

        if(index != i){
            printf("Indice della nave errato");
            return -1;
        }
        if(!validazione_formazione(me->griglia, x, y, orientazione, ship_tipe[i].size, i)){
            printf("Inserimento della nave non valido");
            return -1;
        }

    }

    return 0;

}

void broadcast(azioni *esito){
    // serve per le comunicazioni di esiti a tutti i player, da utilizzare sotto mutex di stato.lock
    for(player *p = stato.head; p != NULL; p = p->next){
        if(send_msg(p->socket, esito) == -1){
            printf("Impossibile inviare il messaggio di esito al giocatore %s:%d\n", p->username, p->id);
        }
    }
}

/*
    INSIEME DELLE FUNZIONI DEDITE ALLA RICEZIONE DI SEGNALAZIONI
*/

void timeout_lobby(int sig){
    //(void)sig; scarta il valore della segnalazione, è superfluo quindi si può levare come mettere, non cambia nulla
    timeout = 0;
}

void chiusura (int sig){
    //(void)sig; scarta il valore della segnalazione, è superfluo quindi si può levare come mettere, non cambia nulla
    shutdown_flag = 1;
}