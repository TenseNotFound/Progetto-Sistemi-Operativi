/*
        CLIENT POSIX RELEASE
*/

#include "../../protocollo/protocollo.h"
#include "../../gui/gui.h"

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
#include <stdbool.h>

volatile sig_atomic_t shutdown = 0;

void gestore(int sig);
int discovery_server(char *ip, int *port); // serve qualora non passo argomenti in esecuzione o qualora il server non sia raggiungibile con i parametri specificati
int recv_msg(int fd, azioni *msg);
int send_msg(int fd, azioni *msg);
ssize_t readn(int fd, void *buf, size_t n); // serve per controllare l'avvenuta lettura di tutti i dati in rete
ssize_t writen(int fd, const void *buf, size_t n); // serve per controllare l'avvenuta scrittura di tutti i dati in rete
void fflush_stdin(void); // serve per pulire il buffer di input, così da evitare che rimangano caratteri in stdin, visto che fflush(stdin) non esiste, lo creo io
int piazzamento_navi (int socket);
int invio_navi(int socket, posizionamento *navi);
void udp_handler(int sig);
void getUsername(char *buf, size_t len); // per prendere l'username del nuovo giocatore
bool validazione( bool board[GRID_SIZE][GRID_SIZE], int x, int y, char orientazione, int dimensione_nave ); // valido la formazione (se è nei limiti prima di inviare)


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
		perror("Errore nell'installaziuone della sigaction");
		goto chiusura;
	}

	gui_init(auto_mode);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0,sizeof(server_addr));
	
	int socketfd = -1, mio_id = -1;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	char buffer[BUFFER_SIZE];

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
	getUsername(username, USERNAME);

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
	piazzamento_navi(socketfd);

	printf("\n[*] In attesa che tutti i giocatori siano pronti...\n");
    fflush(stdout);

	azioni pck;
	bool vivo = true;
	char esito;

	while(vivo && !shutdown){
		memset(&pck, 0, sizeof(pck));
		if(recv_msg(socketfd, &pck) == -1){
			connection_lost();
			break;
		}

		switch(pck.type){
			
			case TURN:
				if(pck.player_id == mio_id){
					turno();

					azioni mossa;
					mossa.player_id = mio_id;

					ricezione_mossa(&mossa);

					if(send_msg(socketfd, &mossa) == -1){
						errore_inivo_mossa();
					}

				} else {
					non_mio_turno(pck.player_id);
				}
			
			case HIT:
			case MISS:
				if(pck.player_id == mio_id){
					if(pck.type == HIT){
						esito = 'X';
					} else esito = 'O';

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

					target_grid[pck.x][pck.y] = esito;
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
					s_elimitato();
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
			default:
				break;

		}
	}

chiusura:

	if(shutdown){
		printf("\n [!] Chiusura forzata del client, inizio routine di shutdown...\n");
		fflush(stdout);
	}

	if(socketfd > 0) close(socketfd);
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

	if(connect(socketfd, (struct sockaddr_in *)server_addr, sizeof(struct sockaddr_in)) == -1){
		perror("Errore nella connessione al server");
		close(socketfd);
		return -1;
	}

	return socketfd;
}

void getUsername(char *buf, size_t len){
	char temp[BUFFER_SIZE];
	while(1) {
        printf("\n[*] Inserisci il tuo username: ");
        fflush(stdout); 
        
        scanf("%255s", temp); 
        fflush_stdin();
        
        if (strlen(temp) < len) {
            strcpy(buf, temp); 
            break;
        }
        
        printf("Inserisci un username di massimo %zu caratteri!\n", len - 1);
    }
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

/*
	LE FUNZIONI readn E writen SONO LE STESSE CHE VENGONO 
	UTILIZZATE IN server.c, FORSE VERRANNO POI MIGRATE
	IN UN FILE COMUNE PER CHIAREZZA
*/

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
            if(errno == EINTR){  // lettura bloccata da segnalazione, riprovo
				if(shutdown) return -1;
				continue;
			}
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
            if(errno == EINTR){ // scrittura bloccata da segnalazione, riprovo
				if(shutdown) return -1;
				continue;
			}
            return -1;
        } if(w == 0) return (ssize_t)(n - left); // il client ha chiuso la connessione, analogo chiusura PIPE
        left -= (size_t)w;
        p += w;
    }
    return (ssize_t)n; // se arrivo qui ho scritto tutti i byte richiesti e comunico con n
}

void fflush_stdin(void){
	int c;
	while((c = getchar()) != '\n' && c != EOF);
}

int piazzamento_navi (int socket) {

	posizionamento posizioni_navi[SHIP_NUMBER];
	int x, y;
	char orientazione;
	bool occupata[GRID_SIZE][GRID_SIZE] = {false}, valid; // griglia temporanea per capire dove ho messo le navi

	for (int i = 0; i < SHIP_NUMBER; i++ ) {
		draw_grids();
		do {
			printf("Inserisci le coordinate della nave %s (dimensione %d) e l'orientamento (N,S,E,O):\n", ship_tipe[i].name, ship_tipe[i].size);
			while(scanf("%d %d %c", &x, &y, &orientazione)!= 3 || (orientazione != 'N' && orientazione != 'S' && orientazione != 'E' && orientazione != 'O')){ 
				printf("Input non valido! \n Sintassi corretta: <x> <y> <orientamento (N,S,E,O)>\n"); 
				fflush(stdout);
				fflush_stdin();
			}

			fflush_stdin();
			valid = validazione(occupata, x - 1, y - 1, orientazione, ship_tipe[i].size);
			if (!valid) {
				printf("Posizionamento non valido: la nave esce dalla griglia o si sovrappone a un'altra nave. Riprova.\n");
				fflush(stdout);
			}
		} while (!valid);

		posizioni_navi[i].index = i;
		posizioni_navi[i].x = x-1;
		posizioni_navi[i].y = y-1;
		posizioni_navi[i].orientation = orientazione;

		addboat(x - 1 , y - 1, orientazione, ship_tipe[i].size);
	
		clean_screen();
		printf("Nave %s posizionata in (%d,%d) con orientamento %c\n", ship_tipe[i].name, x, y, orientazione);
		
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
		p.index = htonl(navi[i].index);
		p.x = htonl(navi[i].x);
		p.y = htonl(navi[i].y);
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
        pthread_exit(NULL);
    }

	struct sockaddr_in broadcast;
	memset(&broadcast, 0, sizeof(broadcast));
	broadcast.sin_family = AF_INET;
	broadcast.sin_port = htons(DISCOVERY_PORT);
	broadcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	
	printf("[*] Richiesta di discovery inviata con successo (broadcast). \n In attesa di riscontro dal server...\n");
	fflush(stdout);

	struct sockaddr_in ricezione;
	char buffer[BUFFER_SIZE];

	ssize_t n;
	
	for(int i = 0; i<TENTATIVI; i++){

		if(sendto(sock, DISCOVER, strlen(DISCOVER), 0, (struct sockaddr *)&broadcast, sizeof(broadcast))<0){
			perror("Errore nell'inviare il payload di discovery");
			close(sock);
			return -1;
		}

		n = recvfrom(sock, buffer, sizeof(buffer) -1 , 0, (struct sockaddr *)&ricezione, sizeof(ricezione));

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
	(void)sig;
	shutdown = 1;
}