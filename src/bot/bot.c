/*
        BOT POSIX RELEASE
*/

#include "../../protocollo/protocollo.h"
#include "../../utils/utils.h"

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

volatile sig_atomic_t shutdown_flag = 0;

void gestore(int sig);
int connetti(char *ip, int porta, struct sockaddr_in *server_addr);
int piazzamento_navi (int socket);
int invio_navi(int socket, posizionamento *navi);
bool validazione(bool board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, int dimensione_nave ); // valido la formazione (se è nei limiti prima di inviare)
void selezione_prossima_mossa(uint8_t *target, uint8_t *x, uint8_t *y); // permette di far scegliere al bot la prossima mossa
void ricerca(uint8_t *x_out, uint8_t *y_out); 
bool cella_papabile(int x, int y);

bool tentativi[GRID_SIZE][GRID_SIZE] = {false};
bool inseguimento = false;
int colpo_iniziale_x, colpo_iniziale_y; /* primo HIT ad aprire un inseguimento di una nave da affondare */
int direzione_colpi = -1;
int ultimo_colpo_x, ultimo_colpo_y; /* ultima cella colpita con successo durante un inseguimento */
bool riprovato_verso_opposto; /* per sapere se, dopo un MISS in un verso, è già stato provato il verso opposto */
int port, avversario_id = -1;

int main(int argc, char **argv) {

        char ip_buf[16] = {0};
        int  socketfd = -1;
        srand(time(NULL));

        if(argc <3){
                printf("Sintassi corretta: %s <IP> <port>\n", argv[0]);
                goto chiusura;
        }
        strncpy(ip_buf, argv[1], sizeof(ip_buf) - 1);
        port = atoi(argv[2]);

        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = gestore;
        sa.sa_flags = 0;

        if(sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1){
                perror("Errore nell'installazione della sigaction");
                goto chiusura;
        }

        sa.sa_handler = SIG_IGN;
        if(sigaction(SIGPIPE, &sa, NULL) == -1){
                perror("Errore nell'installazione della sigaction per la SIGPIPE");
                goto chiusura;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0,sizeof(server_addr));

        int mio_id = -1;

        if((socketfd = connetti(ip_buf, port, &server_addr)) == -1){
                printf("[BOT] Impossibile collegarsi al server\n");
                goto chiusura;
        }
        
        //procedimento di handshake con il server
        azioni msg;
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.username, "BOT", USERNAME-1);
        msg.type = JOIN;

        if(send_msg(socketfd, &msg) == -1){
                printf("[BOT] Errore nella ricezione del messaggio di JOIN");
                goto chiusura;
        }

        azioni welcome_msg;
        if(recv_msg(socketfd, &welcome_msg) == -1){
                printf("[BOT] Errore nella ricezione del messaggio di WELCOME");
                goto chiusura;
        }

        if(welcome_msg.type != WELCOME){
                printf("[BOT] Messaggio di benvenuto non valido\n");
                goto chiusura;
        }

        // ok handshake, il server mi ha assegnato un id
        mio_id = welcome_msg.player_id;
        printf("[BOT] Connesso a %s:%d, id %d\n", ip_buf, port, mio_id);
        fflush(stdout);

        azioni mode_msg;
        memset(&mode_msg, 0, sizeof(mode_msg));
        mode_msg.type = MODE;
        mode_msg.player_id = mio_id;
        mode_msg.gamemode = MODE_DEFAULT;

        if(send_msg(socketfd, &mode_msg) == -1){
                perror("errore nell'invio della modalità di gioco");
                goto chiusura;
        }

        if(piazzamento_navi(socketfd) == -1){
                printf("[BOT] Errore nell'invio della formazione al server\n");
                goto chiusura;
        }

        printf("\n[BOT] Formazione inviata\n [*] In attesa che tutti i giocatori siano pronti...\n");
        fflush(stdout);

        azioni pck;
        bool vivo = true;

        while(vivo && !shutdown_flag){
                memset(&pck, 0, sizeof(pck));
                if(recv_msg(socketfd, &pck) == -1){
                        if(!shutdown_flag) printf("[BOT] Connesssione al server persa\n");
                        break;
                }

                switch(pck.type){
                        case INFO:
                                if(pck.player_id != mio_id) { avversario_id = pck.player_id; }
                                break;

                        case TURN:
                                if(pck.player_id == mio_id){
                                        azioni mossa;
                                        memset(&mossa, 0, sizeof(mossa));
                                        mossa.player_id = mio_id;
                                        mossa.type = MOVE;
                                        
                                        selezione_prossima_mossa(&mossa.target_id, &mossa.x, &mossa.y);

                                        if(send_msg(socketfd, &mossa) == -1){
                                                printf("[BOT] Impossibile inviare la mossa al server\n");
                                                goto chiusura;
                                        }
                                }
                                break;

                        case HIT:
                        case MISS:
                                /*
                                        va messo inseguimento qua
                                */
                               break;
                        case ELIMINATED:
                                if(pck.target_id == mio_id){
                                        printf("[BOT] Sono stato eliminato\n");
                                        fflush(stdout);
                                }
                                break;

                        case WIN:
                                if(pck.player_id == mio_id) printf("[BOT] Ho vinto\n");
                                else printf("[BOT] Ha vinto il player con id %d\n", pck.player_id);
                                fflush(stdout);
                                vivo = false;
                                break;
                        default:
                                break;

                }
        }

chiusura:

        if(shutdown_flag){
                printf("\n[BOT] Chiusura forzata rilevata\n");
                fflush(stdout);
        }

        if(socketfd >= 0) close(socketfd);
        return 0;
}

int connetti(char *ip, int porta, struct sockaddr_in *server_addr){
        memset(server_addr, 0, sizeof(struct sockaddr_in));
        server_addr->sin_family = AF_INET;
        server_addr->sin_port = htons(porta);

        if (inet_aton(ip, &server_addr->sin_addr) == 0) {
                printf("indirizzo ip non valido: %s\n", ip);
                return -1;
        }

        int socketfd = socket(AF_INET, SOCK_STREAM, 0);

        if(socketfd == -1){
                perror("Errore nell'apertura del socket di connessione");
                return -1;
        }

        if(connect(socketfd, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in)) == -1){
                perror("Errore nella connessione al server");
                close(socketfd);
                return -1;
        }

        return socketfd;
}

void selezione_prossima_mossa( uint8_t *target, uint8_t *x_out, uint8_t *y_out){
        *target = (uint8_t)avversario_id;
        int delta_riga[4] = {0,0,1,-1}; /* N,S,E,O */
        int delta_colonna[4] = {-1,1,0,0};
        int x,y;
        if(inseguimento == false) ricerca(x_out, y_out);
        else if(inseguimento == true && direzione_colpi == -1){
                for ( int i = 0;i<4;i++){
                        x = colpo_iniziale_x + delta_riga[i];
                        y = colpo_iniziale_y + delta_colonna[i];
                        if (cella_papabile(x,y)){
                                *x_out = x;
                                *y_out = y;
                                direzione_colpi = i;
                                return;}
                        }
                inseguimento = false;
                ricerca(x_out,y_out);}
        else {
                x = ultimo_colpo_x + delta_riga[direzione_colpi];
                y = ultimo_colpo_y + delta_colonna[direzione_colpi];
                if (!cella_papabile(x,y) && !riprovato_verso_opposto){
                        if(direzione_colpi%2 == 0) { direzione_colpi += 1;} /* serve ad invertire la direzione con cui il bot spara */
                        else { direzione_colpi -= 1;}
                        x = ultimo_colpo_x + delta_riga[direzione_colpi];
                        y = ultimo_colpo_y + delta_colonna[direzione_colpi];
                        if(!cella_papabile(x,y)) ricerca(x_out,y_out);
                        riprovato_verso_opposto = true;
                        }
                else if (!cella_papabile(x,y) && riprovato_verso_opposto) {
                        riprovato_verso_opposto = false;
                        ricerca(x_out,y_out);
                        }
                else {
                        *x_out = x;
                        *y_out = y;
                        return;
                }
        }
}

void ricerca(uint8_t *x_out, uint8_t *y_out) {
        int x,y;
        int liberi = 0;

        for(int i=0; i<GRID_SIZE; i++){
                for(int j = 0; j<GRID_SIZE; j++){
                        if(!tentativi[i][j]) liberi++;
                }
        }

        if(liberi == 0) memset(tentativi, 0, sizeof(tentativi)); // ripulisco se sono tutte provate

        do {
                x = rand() % GRID_SIZE;
                y = rand() % GRID_SIZE;
         } while (tentativi[x][y]);
        *x_out = x;
        *y_out = y;
        tentativi[x][y] = true;
}

bool cella_papabile(int x,int y) {
        if ( x >= GRID_SIZE || x < 0 || y >= GRID_SIZE || y < 0 )  return false;
        else if ( tentativi[x][y] == true)  return false;
        else  return true;
}

int piazzamento_navi (int socket) {
        posizionamento posizioni_navi[SHIP_NUMBER];
        int x, y;
        char orientazione;
        bool occupata[GRID_SIZE][GRID_SIZE] = {false}, valid; // griglia temporanea per capire dove ho messo le navi
        char direzioni[4] = { 'N', 'S' , 'E', 'O' };
        for (int i = 0; i < SHIP_NUMBER; i++ ) {
                do {
                        x = rand() % 10 + 1;
                        y = rand() % 10 + 1;
                        orientazione = direzioni[rand() % 4];
                        valid = validazione(occupata, x - 1, y - 1, orientazione, ship_type[i].size);
                } while (!valid);

                posizioni_navi[i].index = i;
                posizioni_navi[i].x = x-1;
                posizioni_navi[i].y = y-1;
                posizioni_navi[i].orientation = orientazione;

                
                printf("Nave %s posizionata in (%d,%d) con orientamento %c\n", ship_type[i].name, x, y, orientazione);

                fflush(stdout);
        }

        if(invio_navi(socket, posizioni_navi) == -1) return -1;

        return 0;
}

int invio_navi(int socket, posizionamento *navi){
        for(int i = 0; i< SHIP_NUMBER; i++){
                posizionamento p;
                memset(&p, 0, sizeof(p)); //pulisco prima di inviare
                p.index = navi[i].index;
                p.x = navi[i].x;
                p.y = navi[i].y;
                p.orientation = navi[i].orientation;
                if(writen(socket, &p, sizeof(posizionamento)) != sizeof(posizionamento)){
                        return -1;
                }
        }
        return 0;
}

// la validazione viene poi rifatta dal server
bool validazione( bool board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, int dimensione_nave ) {
        // la funzione si occupa di controllare e validare il piazzamento andando a controllare i limiti di griglia
        // marcando quale posizione risulta occupata, non è il controllo ufficiale ma serve solo in fase di posizionamento
        // il vero controllo poi lo rifarà anche il server.
    int delta_riga, delta_colonna, r, c;

        if ( orientazione == 'N' ) { delta_riga = -1; delta_colonna = 0;}
        else if ( orientazione == 'S' ) { delta_riga = 1; delta_colonna = 0;}
        else if ( orientazione == 'E' ) { delta_riga = 0; delta_colonna = 1;}
        else if ( orientazione == 'O' ) { delta_riga = 0; delta_colonna = -1;}
        else return false;
        for ( int i = 0; i < dimensione_nave; i++ ) {
                r = x + i * delta_riga;
                c = y + i * delta_colonna;
                if ( r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE ) {
                        return false;
                } else if ( board[r][c] == true ) return false;
        }

        for ( int i = 0; i < dimensione_nave; i++ ) {
                r = x + i * delta_riga;
                c = y + i * delta_colonna;
                board[r][c] = true;
        }
        return true;
}

void gestore(int sig){
        //(void)sig; scartsrand(time(NULL));a il valore della segnalazione, è superfluo quindi si può levare come mettere, non cambia nulla
        shutdown_flag = 1;
}
