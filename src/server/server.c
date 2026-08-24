/*
        SERVER POSIX RELEASE
*/

#include "../../protocollo/protocollo.h"
#include "../../utils/utils.h"
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
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef game_info info;
typedef azioni mosse;
typedef struct players player;

volatile sig_atomic_t timeout = 1, lobby_aperta = 1, shutdown_flag = 0;
int sem1 = -1, sem2 = -1, n = 0 /*client attivi -> serve per le post su sem2*/, slot = 0 /*quanti sem_id assegnati contando anche quelli di quit*/;
unsigned int port;  
/*
    sem1-> serve per i turni
    sem2-> serve per dare il via libera a tutti dopo che anche l'ultimo client connesso ha finito di disporre le navi
    n-> mi serve per il numero totali di gettoni che andranno nei semafori
*/

void *udp_discovery_port(void *args);
void timeout_lobby(int sig);
void chiusura (int sig);
void gestore_bot (int sig);
void *client_thread (void *args);
void *add_player(int fd, char *username, int sem_id);
void remove_player(int id);
player *trova_giocatore(int valore, bool flag); // serve per trovare un giocatore in base al suo id quando si vuole fare una 
                                // mossa contro di lui, così da poter aggiornare la sua griglia e il numero di navi rimaste
                                // si può cercare tramite id (flag = false) oppure sem_id (flag = true)
bool validazione_formazione(char board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, uint8_t dimensione_nave, int indice); // serve per il check della formazione in entrata, INTEGRITY CHECK
int ricezione_navi(int fd, player *me); // serve per ricevere la formazione che manda il client
void broadcast(azioni *esito); // serve nel client_thread per fare le comunicazioni a tutti gli utenti 


//serve perchè ho bisogno di passare più informazioni al thread, uso quindi una struttura
typedef struct{ // descrive il client
    int client_fd;
    struct sockaddr_in client_addr;
    int sem_id; // valore transitorio che poi viene passato come argomento al thread client che lo inserisce nella struct privata player
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
    mode modalita;
    int mode_scelta;
    pthread_mutex_t lock; // serve per proteggere questi valori, senza che lo dichiaro globale lo metto qui dentro
} gstate;

static gstate stato = {
    NULL, 
    0,  // quanti giocatori sono connessi
    0, // quanti utenti sono già connessi
    0, // inizializzazione flag per la fine
    0, // inizialmente nessun thread attivo
    MODE_DEFAULT, // modalità di gioco di default
    0, // se 0 ancora non scelta, appena diventa 1 è scelta
    PTHREAD_MUTEX_INITIALIZER
};

int main(int argc, char **argv){

    if(argc<2){
        printf("Sintassi corretta: %s <pnumber>\n", argv[0]);
        return -1;
    }
    
    srand((unsigned int)time(NULL));

    int porta = atoi(argv[1]);
    if(porta< 5000 || porta >65535){
        printf("Devi inserire un numero di porta valido nel range 5000-65535!\n");
        printf("Avvio la ricerca automatica della porta...\n");
        port = 5000 + (rand() % (65535 - 5000));
    } else port = (unsigned int) porta;

    printf("\n      POSIX SERVER RELEASE        \nStartup server...\n");
    fflush(stdout);


    int llisten = socket(AF_INET, SOCK_STREAM, 0);
    if (llisten < 0) {
        perror("Errore nel socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1, ret;
    ret = setsockopt(llisten, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret == -1){
        perror("Errore nella setsockopt");
        goto exit;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // ip assegnato dal DHCP

    int bound = 0; // appena ho successo mi fa uscire dal for
    for (int attempt = 0; attempt < TENTATIVI && !bound; attempt++) {
        server_addr.sin_port = htons(port);
 
        if (bind(llisten, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            bound = 1;
        } else if (errno == EADDRINUSE) {
            printf("Porta %d occupata, provo una porta casuale...", port);
            port = 5000 + (rand() % (65535 - 5000));   /* range non privilegiato */
        } else {
            perror("bind() fallita");
            goto exit;
        }
    }
 
    if (!bound) {
        perror("Impossibile trovare una porta libera dopo il numero massimo di tentativi concesso");
        goto exit;
    }

    // dimensione del backlog -> dimensione della coda di attesa (pending connection non ancora accettate), definita nel protocollo
    if (listen(llisten, BACKLOG) < 0) {
        perror("listen() fallita");
        goto exit;
    }

    printf("\n==================================================\n         SERVER AVVIATO CON SUCCESSO         \n==================================================\n");
    printf(" Indirizzo IP : 0.0.0.0 (INADDR_ANY)\n Porta        : %u\n", port);
    printf(" Backlog      : %d (Coda massima client)\n", BACKLOG);
    printf(" Per chiudere : CTRL + C\n==================================================\n[*] In attesa di nuove connessioni...\n\n");

    fflush(stdout);
    
    struct sigaction sa;
    sa.sa_flags = 0;

    if (sigemptyset(&sa.sa_mask) == -1){
        perror("Errore nello svuotare la maschera delle segnalazioni");
        goto exit;
    }

    sa.sa_handler = SIG_IGN;

    if(sigaction(SIGPIPE, &sa, NULL) == -1){
        perror("Errore nell'installare la sigaction per SIGPIPE, procedo con l'installazione della signal");
        if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            goto exit;
        }
    }

    sa.sa_handler = timeout_lobby;

    if(sigaction(SIGALRM, &sa, NULL) == -1){
        perror("Errore nell'installare la sigaction per SIGALARM, procedo con l'installazione della signal");
        if(signal(SIGALRM, timeout_lobby) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            goto exit;
        }
    }
    
    sa.sa_handler = chiusura;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Errore nell'installare la sigaction per SIGINT, procedo con l'installazione della signal");
        if(signal(SIGINT, chiusura) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            goto exit;
        }
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Errore nell'installare la sigaction per SIGTERM, procedo con l'installazione della signal");
        if(signal(SIGTERM, chiusura) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            goto exit;
        }
    }

    sa.sa_handler = gestore_bot;
    if(sigaction(SIGCHLD, &sa, NULL) == -1){
        perror("Errore nell'installare la sigaction per SIG_CHILD, procedo con l'installazione della signal");
        if(signal(SIGCHLD, gestore_bot) == SIG_ERR){
            perror("Errore nell'installazione della signal");
            goto exit;
        }
    }

    pthread_t udp_thread;
    
    if(pthread_create(&udp_thread, NULL, udp_discovery_port, &port) != 0){
        perror("Errore nella creazione del thread per la discovery UDP");
        goto exit;
    } else {
        pthread_detach(udp_thread);
        printf("Thread UDP discovery creato con successo\n");
        fflush(stdout);
    }

    sem2 = semget(IPC_PRIVATE, 1, IPC_CREAT|0664);
    if(sem2 == -1){
        perror("Errore nella creazione del semaforo per il controllo che tutti i player hanno terminato l'invio della formazione");
        goto exit;
    }

    semctl(sem2, 0, SETVAL, 0); // inizialmente ho 0 gettoni, poi mi faccio fare le post su questo semaforo e quando torna a 0 sblocco tutto

    sem1 = semget(IPC_PRIVATE, MAX_PLAYER, IPC_CREAT|0664);
    if(sem1 == -1){
        perror("Errore nella creazione del semaforo per i turni dei client connessi");
        goto exit;
    }

    int current_player = 0; // variabile locale al while per il controllo dei client collegati, senza che uso il mutex
    printf("Lobby aperta per %ds\n", TIMEOUT_LOBBY);
    alarm(TIMEOUT_LOBBY);
 
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

        if(current_player >= MAX_PLAYER){
            // magari aggiungi un messaggio di notifica al client per la chiusura lobby 
            close(client);
            alarm(0);
            break;
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
        cargs->sem_id = current_player;

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
        current_player++;

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
            snprintf(porta, sizeof(porta), "%u", port);
            execl(".././bot", "bot", "127.0.0.1", porta, NULL);
            perror("Errore nello startup del bot");
            exit(1);

        } else if (pid < 0){
            perror("Errore nella fork per l'avvio del bot");
        } else {
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
                cbot->sem_id = current_player;

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
                n++;
                current_player++;

            } else perror("Errore nell'accept del bot");
        } 
    }

    slot = current_player;
    close(llisten);
    llisten = -1;

    if(n > 0){
        /*
                CAUSA RACE CONDITION E ROMPE LA CONSISTENZA DEI SEM_ID
            int i = 0;
            pthread_mutex_lock(&stato.lock);
            for(player *p = stato.head; p!= NULL; p = p->next){
                // questo for serve per rinumerare gli id dei semafori perchè un client potrebbe scollegarsi anche in lobby
                p->sem_id = i++;
            }

            pthread_mutex_unlock(&stato.lock);
        */
        

        for(int j = 0; j<slot; j++){
            semctl(sem1, j, SETVAL, 0);
        }

        printf("[*] Lobby chiusa, ricezione nuove richieste di partecipazione bloccate\n");
        fflush(stdout);

        struct sembuf sem;
        sem.sem_flg = 0;
        sem.sem_op = -n; // -n così mi fanno le post e torna a 0
        sem.sem_num = 0;

try:
        if ((ret =semop(sem2, &sem, 1)) == -1 && errno != EINTR){
            perror("Errore nella semop");
            if (shutdown_flag) goto exit;
        } else if (ret == -1) goto try;
        

        printf("[*] Startup della partita\n");

        struct sembuf startup;
        startup.sem_flg = 0;
        startup.sem_op = 1;
        int primo = -1; // lo uso per trovare il primo giocatore vivo a cui dare il gettone 

        pthread_mutex_lock(&stato.lock);
        for(int j = 0; j<slot; j++){
            // ricerco il primo giocatore disponibile da cui far partire la partita
            player *p = trova_giocatore(j, true);
            if(p != NULL && p->alive) {
                primo = j;
                break;
            }
        }
        pthread_mutex_unlock(&stato.lock);

        if(primo == -1){
            printf("[!] Impossibile trovare un giocatore disponibile per lo startup\n");
            fflush(stdout);
            goto exit;
        } else startup.sem_num = primo;

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

    if (llisten >= 0) close(llisten);

    // devo attendere che tutti i thread terminino
    bool terminati = false;
    for(int max = 0; max< TIMEUOUT_SHOUTDOWN; max++){
        pthread_mutex_lock(&stato.lock);
        if(stato.active_threads == 0){
            terminati = true;
            pthread_mutex_unlock(&stato.lock);
            break;
        } else pthread_mutex_unlock(&stato.lock);
        sleep(1);
    }

    if (sem1 >= 0) semctl(sem1, 0, IPC_RMID);

    if (sem2 >= 0) semctl(sem2, 0, IPC_RMID);


    pthread_mutex_lock(&stato.lock);
    if (terminati){
        player *curr = stato.head, *temp;
        while (curr != NULL) {
            temp = curr;
            curr = curr->next;
            free(temp);
        }
        stato.head = NULL;
    } else printf("[!] Ci sono ancora %d thread attivi e non terminati!\n", stato.active_threads);
    
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

    me = add_player(fd, msg.username, cargs->sem_id);

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
        printf("[!] Errore nella ricezione della gamemode dal client %s:%d\n", ip, portc);
        goto exit;
    }

    if(gamemode.gamemode != MODE_DEFAULT && gamemode.gamemode != MODE_1V1){
        printf("[!] Ricevuta una modalità di gioco non valida da %s:%d", ip, portc);
        goto exit;
    }

    pthread_mutex_lock(&stato.lock);

    if(!stato.mode_scelta){
        stato.modalita = gamemode.gamemode;
        stato.mode_scelta = 1;
        printf("[*] Modalità di gioco impostata da %s:%d a %s\n", ip, portc, stato.modalita == MODE_1V1 ? "1v1" : " Default (TcT)");
        fflush(stdout);
    }
    bool rifiutato = (stato.modalita == MODE_1V1 && stato.count > 2);

    pthread_mutex_unlock(&stato.lock);

    if(rifiutato){
        printf("[!] Connessione rifiutata per %s:%d in quanto si è in 1v1 e ci sono già 2 client connessi\n", ip, portc);
        fflush(stdout);
        goto exit;
    }

    printf("[*] In attesa della formazione da %s:%d (ID: %d)", ip, portc, me->id);
    fflush(stdout);

    if(ricezione_navi(fd, me) != 0){
        printf("Formazione ricevuta non valida");
        //manda un messaggio di notifica al client
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
        bool colpito = false;

gback:
        if((ret = semop(sem1, &sem, 1)) == -1 && errno != EINTR){  
            perror("Errore nel prendere il gettone");
            if(shutdown_flag) goto exit;
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
                azioni infopk;
                memset(&infopk, 0 , sizeof(infopk));
                infopk.type = INFO;
                infopk.player_id = p->id;
                strncpy(infopk.username, p->username, USERNAME-1);
                if(send_msg(me->socket, &infopk) == -1){
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
            goto next_turn;
        }

        struct timeval tv;
        tv.tv_sec = AFK_TIMEOUT;
        tv.tv_usec = 0;
        if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1){
            perror("Errore nell'installare il timeout di inattività sul socket del client");
            pthread_mutex_lock(&stato.lock);
            me->alive = 0;
            pthread_mutex_unlock(&stato.lock);
            goto next_turn;
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
        
        tv.tv_sec = 0;
        if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1){
            perror("Errore nella disattivazione del timer per il socket del client connesso");
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
        
        // poichè uint8_t è unsigned, se passo un valore negativo ottengo sicuramente un valore ≥ a GRID_SIZE quindi il controllo sul negativo
        // è superfluo
        if(bersaglio == NULL || !bersaglio->alive || mossa.x >= GRID_SIZE || mossa.y >= GRID_SIZE || mossa.target_id == id){
            printf("Bersaglio specificato o coordinate ricevute non valide\n");
            fflush(stdout);
            esito.type = MISS;
        } else {
            char cella = bersaglio->griglia[mossa.x][mossa.y];
            colpito = (cella >= '0' && cella <= '4');

            if(colpito){
                bersaglio->griglia[mossa.x][mossa.y] = 'X';
                esito.type = HIT;
                colpito = true;
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

        int bersaglio_id = -1;
        if(bersaglio != NULL) bersaglio_id = bersaglio->id;

        pthread_mutex_unlock(&stato.lock);

        if(eliminato){
            azioni gameover;
            memset(&gameover, 0, sizeof(gameover));
            gameover.type = ELIMINATED;
            gameover.target_id = bersaglio_id;
            
            pthread_mutex_lock(&stato.lock);
            broadcast(&gameover);
            pthread_mutex_unlock(&stato.lock);
        }

        // poichè a priori non so se il prossimo giocatore è stato eliminato, devo trovare il prossimo ancora vivo e poi passare l'id alla sembuf
next_turn:
        pthread_mutex_lock(&stato.lock);
        int prossimo = -1; 
        int test = semid;
        for(int i = 0; i<slot; i++){
            test = (test +1) %slot;
            if (test == semid) continue;
            player *p = trova_giocatore(test, true); // true specifica se ricerca per numero di semaforo, false solo con id
            if(p != NULL && p->alive){ 
                prossimo = test;
                break;
            }
        }

        bool solo = (prossimo == -1); // per vedere se ci sono altri avversari
        if(solo && !stato.fine){
            stato.fine = fine_partita = 1; //sono solo chiudo la partira
        } else if(colpito && !solo) prossimo = semid;

        pthread_mutex_unlock(&stato.lock);
        
        if(fine_partita){
            azioni win; 
            memset(&win, 0, sizeof(win));
            win.type = WIN;
            win.player_id = id;

            pthread_mutex_lock(&stato.lock);
            broadcast(&win);
            pthread_mutex_unlock(&stato.lock);

            // devo risvegliare tutti i thread in lock
            for(int j = 0; j<slot; j++){
                struct sembuf sem;
                sem.sem_op = 1;
                sem.sem_flg = 0;
                sem.sem_num = j;

wake:
                if((ret = semop(sem1, &sem, 1)) == -1 && errno != EINTR){
                    perror("Errore nel risvegliare tutti i thread per fine partita");
                    goto exit;
                } else if (ret == -1) goto wake;
            }

            goto exit;

        } else if (prossimo == -1) goto exit;
        sem.sem_op = 1;
        sem.sem_num = prossimo;

pass: 
        if((ret = semop(sem1, &sem, 1)) == -1 && errno != EINTR){
            perror("Errore nel rilasciare il gettone del semaforo al prossimo player");
            goto exit;
        } else if(ret == -1) goto pass;

        pthread_mutex_lock(&stato.lock);
        alive = me->alive;
        pthread_mutex_unlock(&stato.lock);
        if(!alive) goto exit;
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

void *add_player(int fd, char *username, int sem_id){

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
    new_player->sem_id = sem_id;

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
    int chiave; // scelgo la chiave in base a quel che gli passo

    while(current != NULL){
        if(flag){
            chiave = current->sem_id;
        } else {
            chiave = current->id;
        }

        if(chiave == valore){
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void *udp_discovery_port(void *args){
    unsigned int porta = *((unsigned int *)args);
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0), letti;
    
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
        letti = recvfrom(socket_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&client_addr, &client_addr_len); // bloccata al massimo per 2s
        if(letti > 0){
            buffer[letti] = '\0';
            if(strcmp(buffer, "DISCOVER") == 0){
                snprintf(reply, sizeof(reply), "%u", porta);
                sendto(socket_fd, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, client_addr_len);
                printf("Ricevuta richiesta di discovery da %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                fflush(stdout);
            }
        } else if(letti < 0){
            if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; // se l'errore è dovuto a una segnalazione, riprovo
            perror("Errore nella ricezione del messaggio UDP");
        }
    }

    close(socket_fd);
    pthread_exit(NULL);
}

bool validazione_formazione(char board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, uint8_t dimensione_nave, int indice){
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
        if(readn(fd, &p, sizeof(p)) != (size_t)sizeof(p)){
            perror("Errore nella ricezione della formazione ");
            return -1;
        }

        index = p.index;
        x = p.x; // non servono ntohl poichè lavorano su int, non su byte singolo, endianess safe in quanto solo un byte (endianess a livello di byte)
        y = p.y;
        orientazione = p.orientation;

        if(index != i){
            printf("Indice della nave errato");
            return -1;
        }
        if(!validazione_formazione(me->griglia, x, y, orientazione, ship_type[i].size, i)){
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

void gestore_bot(int sig){
    //(void)sig; scarta il valore della segnalazione, è superfluo quindi si può levare come mettere, non cambia nulla
    wait(NULL);
}