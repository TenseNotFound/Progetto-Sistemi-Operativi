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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


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


struct posizionamento Nave;
int port;


// la validazione viene fatta dal server
/*
	bool validazione( bool board[10][10], int x, int y, char orientazione, int dimensione_nave ) {

    int delta_riga, delta_colonna, r, c;
	if ( orientazione != 'N' && orientazione != 'S' && orientazione != 'E' && orientazione != 'O' ) {
		printf("orientazione scelta non valida\n");
		return false; 
	}
	else if ( orientazione == 'N' ) { delta_riga = -1; delta_colonna = 0;}
	else if ( orientazione == 'S' ) { delta_riga = 1; delta_colonna = 0;}
	else if ( orientazione == 'E' ) { delta_riga = 0; delta_colonna = 1;}
	else if ( orientazione == 'O' ) { delta_riga = 0; delta_colonna = -1;}
	for ( int i = 0; i < dimensione_nave; i++ ) {
		r = x + i * delta_riga;
		c = y + i * delta_colonna;
		if ( r < 0 || r >= 10 || c < 0 || c >= 10 ) { return false;}
		else if ( board[r][c] == true ) { return false;} 
	}
	for ( int i = 0; i < dimensione_nave; i++ ) {
		r = x + i * delta_riga;
		c = y + i * delta_colonna;
		board[r][c] = true; 
	}
	return true;
}
*/

int main(int argc, char **argv) {

	char ip_buf[16] = {0};
	port = 0;
	bool auto_mode = false;

	if(argc == 1){
		auto_mode = true;
		if(discovery_server(ip_buf, &port) == -1){
			printf("Errore nel discovery del server, inserisci manualmente ip e porta\n Sintassi: %s <IP> <port>\n", argv[0]);
			return -1;
		}
		fflush(stdout);

	} else if(argc <3){
		printf("Sintassi corretta: %s <IP> <port>\n", argv[0]);
		return -1;
	} else {
		port = atoi(argv[2]);
		if(port < 5000 || port >65535){
			printf("Inserisci un numero di porta valido nel range 5000-65535\n");
			return -1;
		}
		strncpy(ip_buf, argv[1], sizeof(ip_buf) - 1);
	}


	// TODO: fallback qualora port e ip non siano corretti
	gui_init(auto_mode);

	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	
	int socketfd, mio_id;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	char buffer[BUFFER_SIZE];

	socketfd = connetti(ip_buf, port, &server_addr);
	if(socketfd == -1){
		printf("[*] Connessione a %s:%d fallita, provo il discovery automatico...\n", ip_buf, port);
		fflush(stdout);
		if(discovery_server(ip_buf, &port) == -1){
			printf("Errore: impossibile trovare un server\n");
			return -1;
		}

		socketfd = connetti(ip_buf, port, &server_addr);
	}

	if(socketfd == -1){
		printf("Impossibile stabilire una connessione con il server\n");
		exit(EXIT_FAILURE);
	}

	server_connected(ip_buf, port, -1);

	char username[USERNAME];
	getUsername(username, USERNAME);

	//procedimento di handshake con il server
	azioni msg;	
	bzero(&msg, sizeof(msg));
	strncpy(msg.username, username, USERNAME-1);
	msg.username[USERNAME -1] = '\0';
	msg.type = JOIN;
	if(send_msg(socketfd, &msg) == -1){
		perror("errore nell'invio del messaggio di JOIN");
		close(socketfd);
		return -1;
	}

	azioni welcome_msg;
	if(recv_msg(socketfd, &welcome_msg) == -1){
		perror("errore nella ricezione del messaggio di WELCOME");
		close(socketfd);
		return -1;
	}

	if(welcome_msg.type != WELCOME){
		printf("messaggio di benvenuto non valido\n");
		close(socketfd);
		return -1;
	}

	// ok handshake, il server mi ha assegnato un id
	mio_id = welcome_msg.player_id;
	init_board();


	piazzamento_navi(socketfd);
	
	
	close(socketfd);
	return 0;
}

int connetti(char *ip, int porta, struct sockaddr_in *server_addr){
	bzero(server_addr, sizeof(struct sockaddr_in));
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

	if(connect(socketfd, (struct sockaddr_in *)&server_addr, sizeof(struct sockaddr_in)) == -1){
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

void fflush_stdin(void){
	int c;
	while((c = getchar()) != '\n' && c != EOF);
}

int piazzamento_navi (int socket) {

	posizionamento posizioni_navi[SHIP_NUMBER];
	int x, y;
	char orientazione;

	for (int i = 0; i < SHIP_NUMBER; i++ ) {
		fflush_stdin();
		draw_grids();
		printf("Inserisci le coordinate della nave %s (dimensione %d) e l'orientamento (N,S,E,O):\n", ship_tipe[i].name, ship_tipe[i].size);
		while(scanf("%d %d %c", &x, &y, &orientazione)!= 3){ 
			printf("Input non valido! \n Sintassi corretta: <x> <y> <orientamento>\n"); 
			fflush(stdout);
			fflush_stdin();
		}
		posizioni_navi[i].index = i;
		posizioni_navi[i].x = x;
		posizioni_navi[i].y = y;
		posizioni_navi[i].orientation = orientazione;

		addboat(x - 1 , y - 1, orientazione, ship_tipe[i].size);

		printf("Nave %s posizionata in (%d,%d) con orientamento %c\n", ship_tipe[i].name, x, y, orientazione);
		clean_screen();
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

	struct sockaddr_in broadcast;
	broadcast.sin_family = AF_INET;
	broadcast.sin_port = htons(DISCOVERY_PORT);
	broadcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	if(sendto(sock, DISCOVER, strlen(DISCOVER), 0, (struct sockaddr *)&broadcast, sizeof(broadcast))<0){
		perror("Errore nell'inviare il payload di discovery");
		close(sock);
		return -1;
	}

	printf("[*] Richiesta di discovery inviata con successo (broadcast). \n In attesa di riscontro dal server...\n");
	fflush(stdout);


	// timeout per il listen broadcast -> fatto con una segnalazione SIGALRM

	struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = udp_handler;
    
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Errore sigaction");
        close(sock);
        return -1;
    }

	alarm(TIMEOUT);

	struct sockaddr_in ricezione;
	char buffer[BUFFER_SIZE];

	socklen_t dim = sizeof(ricezione);
	ssize_t n = recvfrom(sock, buffer, sizeof(buffer) -1 , 0, (struct sockaddr *)&ricezione, &dim);

	alarm(0);

	if(n < 0){
        if(errno == EINTR){
            printf("Timeout: Nessun server trovato nella rete in %d secondi\n", TIMEOUT);
        } else {
            perror("recvfrom() errore");
        }
        close(sock);
        return -1;
    }

	buffer[n] = '\0';
    strncpy(ip, inet_ntoa(ricezione.sin_addr), 16);
    ip[15] = '\0';

    *port = atoi(buffer);

    printf("[*] Server trovato con successo!\n[*] In ascolto su %s:%d\n", ip, *port);

    close(sock);
    return 0;	

}

void udp_handler(int sig){
	// serve solo per sbloccare la recvfrom dal timeout in udp per evitare i blocchi
	(void)sig;
}

