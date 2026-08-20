/*
        CLIENT POSIX RELEASE
*/

#include "../../protocollo/protocollo.h"
#include "../../gui/gui.h"
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

volatile sig_atomic_t shutdown_flag = 0;

void gestore(int sig);
int discovery_server(char *ip, int *port); // serve qualora non passo argomenti in esecuzione o qualora il server non sia raggiungibile con i parametri specificati
int connetti(char *ip, int porta, struct sockaddr_in *server_addr);
int piazzamento_navi (int socket);
int invio_navi(int socket, posizionamento *navi);
int getUsername(char *buf, size_t len); // per prendere l'username del nuovo giocatore
bool validazione( bool board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, uint8_t dimensione_nave ); // valido la formazione (se è nei limiti prima di inviare)

struct posizionamento Nave;
int port;


int main(int argc, char **argv) {

	char ip_buf[16] = {0};
	port = 0;
	bool auto_mode = false;

	if(argc == 1){
		auto_mode = true;
		if(discovery_server(ip_buf, &port) == -1){
			printf("Errore nel discovery del server, inserisci manualmente ip e porta\n Sintassi: %s <IP> <port>\n", argv[0]);
			goto chiusura;
		}
		fflush(stdout);

	} else if(argc <3){
		printf("Sintassi corretta: %s <IP> <port>\n", argv[0]);
		goto chiusura;
	} else {
		port = atoi(argv[2]);
		if(port < 5000 || port >65535){
			printf("Inserisci un numero di porta valido nel range 5000-65535\n");
			goto chiusura;
		}
		strncpy(ip_buf, argv[1], sizeof(ip_buf) - 1);
	}

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
		perror("Errore nell'installazione della sigaction per la sigpipe");
		goto chiusura;
	}

	gui_init(auto_mode);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0,sizeof(server_addr));
	
	int socketfd = -1, mio_id = -1;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);


	socketfd = connetti(ip_buf, port, &server_addr);
	if(socketfd == -1){
		printf("[*] Connessione a %s:%d fallita, provo il discovery automatico...\n", ip_buf, port);
		fflush(stdout);
		if(discovery_server(ip_buf, &port) == -1){
			printf("Errore: impossibile trovare un server\n");
			goto chiusura;
		}

		socketfd = connetti(ip_buf, port, &server_addr);
	}

	if(socketfd == -1){
		printf("Impossibile stabilire una connessione con il server\n");
		goto chiusura;
	}

	char username[USERNAME];
	if(getUsername(username, USERNAME) == -1) goto chiusura;

	//procedimento di handshake con il server
	azioni msg;	
	memset(&msg, 0, sizeof(msg));
	strncpy(msg.username, username, USERNAME-1);
	msg.username[USERNAME -1] = '\0';
	msg.type = JOIN;
	if(send_msg(socketfd, &msg) == -1){
		perror("errore nell'invio del messaggio di JOIN");
		goto chiusura;
	}

	azioni welcome_msg;
	if(recv_msg(socketfd, &welcome_msg) == -1){
		perror("errore nella ricezione del messaggio di WELCOME");
		goto chiusura;
	}

	if(welcome_msg.type != WELCOME){
		printf("messaggio di benvenuto non valido\n");
		goto chiusura;
	}

	// ok handshake, il server mi ha assegnato un id
	mio_id = welcome_msg.player_id;
	server_connected(ip_buf, port, mio_id);

	azioni mode_msg;
	memset(&mode_msg, 0, sizeof(mode_msg));
	mode_msg.type = MODE;
	mode_msg.player_id = mio_id;
	mode_msg.gamemode = game_mode();

	if(send_msg(socketfd, &mode_msg) == -1){
		perror("errore nell'invio della modalità di gioco");
		goto chiusura;
	}


	init_board();
	if(piazzamento_navi(socketfd) == -1){
		printf("Impossibile comunicare al server la formazione, chiudo...\n");
		fflush(stdout);
		goto chiusura;
	}

	printf("\n[*] In attesa che tutti i giocatori siano pronti...\n");
    fflush(stdout);

	azioni pck;
	bool vivo = true;
	char esito;

	while(vivo && !shutdown_flag){
		memset(&pck, 0, sizeof(pck));
		if(recv_msg(socketfd, &pck) == -1){
			if(!shutdown_flag) connection_lost(); // mi serve in quanto può dare -1 anche per ctrl+c
			break;
		}

		switch(pck.type){
			case INFO:
				bersagli(pck.player_id, pck.username);
                break;

			case TURN:
				if(pck.player_id == mio_id){
					turno();

					azioni mossa;
					memset(&mossa, 0, sizeof(mossa));
					mossa.player_id = mio_id;

					ricezione_mossa(&mossa);

					if(send_msg(socketfd, &mossa) == -1){
						errore_invio_mossa();
					}

				} else {
					non_mio_turno(pck.player_id);
				}
				break;
			
			case HIT:
			case MISS:
				if(pck.player_id == mio_id){
					if(pck.type == HIT){
						esito = 'X';
					} else esito = 'O';
					
					/*
						SISTEMA GRIGLIA NEMICA
					*/
					clean_screen();
					draw_grids();
					target_grid[pck.x][pck.y] = esito;

					if(pck.type == HIT){
						colpito();
					} else miss();

				} else if(pck.target_id == mio_id){

					if(pck.type == MISS){
						esito = 'O';
					} else esito = 'X';

					grid[pck.x][pck.y] = esito;
					clean_screen();
					draw_grids();

					if(pck.type == HIT){
						colpito();
					} else miss();


				} else {
					spettatore(&pck);
				}
				break;

			
			case ELIMINATED:
				if(pck.target_id == mio_id){
					s_eliminato();
				} else {
					eliminato(pck.target_id);
				}
				break;
			
			case WIN:
				if(pck.player_id == mio_id){
					s_vittoria();
				} else {
					vittoria(pck.player_id);
				}
				vivo = false;
				break;
			default:
				break;

		}
	}

chiusura:

	if(shutdown_flag){
		printf("\n [!] Chiusura forzata del client, inizio routine di shutdown...\n");
		fflush(stdout);
	}

	if(socketfd >= 0) close(socketfd);
	close_game();
	

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

int getUsername(char *buf, size_t len){

	int fd = open("_user", O_RDONLY);
    if(fd == -1){
        perror("Errore nell'apertura del file di gestione degli utenti");
        
    } else {
		buf[0] = '\0';
		ssize_t letti = readn(fd, buf, len-1);
		if(letti > 0){
			buf[letti] = '\0';
			if (buf[letti - 1] == '\n') {
        		buf[letti - 1] = '\0';
    		}
		}

		if(strlen(buf)>0) {
			welcomeback(buf);
			close(fd);
			return;
		}
		close(fd);
	}

	char temp[BUFFER_SIZE];
	while(1) {
        printf("\n[*] Inserisci il tuo username: ");
        fflush(stdout); 
        
        if(scanf("%255s", temp) == EOF){
			return -1;
		}
        fflush_stdin();
        
        if (strlen(temp) < len) {
			int fd = open("_user", O_CREAT|O_WRONLY|O_TRUNC, 0664);
			if(fd == -1){
				perror("Errore nell'apertura del file");
				printf("Procedo normalmente escludendo la scrittura sul file \n");
				fflush(stdout);
				strcpy(buf, temp);
			} else {
				strcpy(buf, temp); 
				writen(fd, buf, strlen(buf));
				close(fd);
			}
			break;
        }
        
        printf("Inserisci un username di massimo %zu caratteri!\n", len - 1);
    }
	return 0;
}

/*
	LE FUNZIONI readn E writen SONO LE STESSE CHE VENGONO 
	UTILIZZATE IN server.c, FORSE VERRANNO POI MIGRATE
	IN UN FILE COMUNE PER CHIAREZZA
*/

int piazzamento_navi (int socket) {

	posizionamento posizioni_navi[SHIP_NUMBER];
	int x, y;
	char orientazione;
	bool occupata[GRID_SIZE][GRID_SIZE] = {false}, valid; // griglia temporanea per capire dove ho messo le navi
	int letti;

	for (int i = 0; i < SHIP_NUMBER; i++ ) {
		draw_grids();
		do {
			printf("Inserisci le coordinate della nave %s (dimensione %u) e l'orientamento (N,S,E,O):\n", ship_type[i].name, ship_type[i].size);
			while((letti = scanf("%d %d %c", &x, &y, &orientazione))!= 3 || (orientazione != 'N' && orientazione != 'S' && orientazione != 'E' && orientazione != 'O')){ 
				
				if(letti == EOF) return -1;

				printf("Input non valido! \n Sintassi corretta: <x> <y> <orientamento (N,S,E,O)>\n"); 
				fflush(stdout);
				fflush_stdin();
			}

			fflush_stdin();
			valid = validazione(occupata, x - 1, y - 1, orientazione, ship_type[i].size);
			if (!valid) {
				printf("Posizionamento non valido: la nave esce dalla griglia o si sovrappone a un'altra nave. Riprova.\n");
				fflush(stdout);
			}
		} while (!valid);

		posizioni_navi[i].index = i;
		posizioni_navi[i].x = x-1;
		posizioni_navi[i].y = y-1;
		posizioni_navi[i].orientation = orientazione;

		addboat(x - 1 , y - 1, orientazione, ship_type[i].size);
	
		clean_screen();
		printf("Nave %s posizionata in (%d,%d) con orientamento %c\n", ship_type[i].name, x, y, orientazione);
		
		fflush(stdout);
	}

	draw_grids();

	if(invio_navi(socket, posizioni_navi) == -1){
		perror("errore nell'invio delle posizioni delle navi");
		return -1;
	}

	return 0;
}

int invio_navi(int socket, posizionamento *navi){
	for(int i = 0; i< SHIP_NUMBER; i++){
		posizionamento p;
		memset(&p, 0, sizeof(p)); //pulisco prima di inviare
		p.index = navi[i].index;
		p.x = navi[i].x; // endianess safe -> mando 1 byte solo e non 4
		p.y = navi[i].y;
		p.orientation = navi[i].orientation;
		if(writen(socket, &p, sizeof(posizionamento)) != sizeof(posizionamento)){
			return -1;
		}
	}
	return 0;
}

int discovery_server(char *ip, int *port){

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock == -1){
		perror("Errore nell'apertura del socket per l'UDP");
		return -1;
	}

	int abilitata = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &abilitata, sizeof(abilitata))<0){
		perror("Errore nell'abilitare la modalità broadcast sul socket di discovery");
		close(sock);
		return -1;
	}

	struct timeval time; // dal man di setsockopt
    time.tv_sec = 2; 
    time.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
        perror("errore nel setup del timer in setsockopt");
        close(sock);
        return -1;
    }

	struct sockaddr_in broadcast;
	memset(&broadcast, 0, sizeof(broadcast));
	broadcast.sin_family = AF_INET;
	broadcast.sin_port = htons(DISCOVERY_PORT);
	broadcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	
	printf("[*] Richiesta di discovery inviata con successo (broadcast). \n In attesa di riscontro dal server...\n");
	fflush(stdout);

	struct sockaddr_in ricezione;
	socklen_t ricezione_s = sizeof(ricezione); // serve necessariamente perchè la recvfrom vuole un puntatore alla dim della struttura (man)
	char buffer[BUFFER_SIZE];

	ssize_t n;
	
	for(int i = 0; i<TENTATIVI; i++){

		if(sendto(sock, DISCOVER, strlen(DISCOVER), 0, (struct sockaddr *)&broadcast, sizeof(broadcast))<0){
			perror("Errore nell'inviare il payload di discovery");
			close(sock);
			return -1;
		}

		n = recvfrom(sock, buffer, sizeof(buffer) -1 , 0, (struct sockaddr *)&ricezione, &ricezione_s);

		if (n > 0) {
            buffer[n] = '\0';
            
            if (atoi(buffer) > 0) { 
                strncpy(ip, inet_ntoa(ricezione.sin_addr), 15); // il server manda solo la porta (Payload), il resto è nell'header del pacchetto
                ip[15] = '\0';
                *port = atoi(buffer);

                printf("[*] Server trovato con successo!\n[*] In ascolto su %s:%d\n", ip, *port);
				fflush(stdout);
                close(sock);
                return 0;
            }
        }
        
        printf(" -> Tentativo %d fallito (Timeout), riprovo...\n", i + 1);
        fflush(stdout);

	}

	printf("[*] Nessun server trovato nella rete dopo %d tentativi.\n", TENTATIVI);
    close(sock);
    return -1;	

}

// la validazione viene poi rifatta dal server
bool validazione( bool board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, uint8_t dimensione_nave ) {
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
	//(void)sig; scarta il valore della segnalazione, è superfluo quindi si può levare come mettere, non cambia nulla
	shutdown_flag = 1;
}