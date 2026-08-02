#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#if defined(UNIX)
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#elif defined(WINDOWS)
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#error "specificare piattaforma UNIX o WINDOWS in compilazione"
#endif
#if defined(LOCAL)
#define IP "127.0.0.1"
/*#elif defined(GLOBAL)
#define IP "inserire url per acquisizione IP"*/
#else
#endif
#define BUFFER_SIZE 256

typedef struct {
	int riga;
	int colonna;
	char direzione;
	} Nave;

void piazzamento_navi() {
	bool board[10][10] = {false};
	int dimensioni_navi[5] = { 5, 4, 3, 3, 2};
	const char *nomi_navi[5] = {"Portaerei", "Corazzata", "Incrociatore", "Incrociatore", "Cacciatorpediniere"};
	Nave navi_validate[5];
	bool valid;
	int x, y;
	char orientazione;
	char buffer[40];
	int offset = 0;
	for ( int i = 0; i < 5; i++) {
		printf("Piazzare la nave %s, di dimensione %d\n", nomi_navi[i], dimensioni_navi[i]);
		do {
			scanf( "%d %d %c", &x, &y, &orientazione);
			valid = validazione( board, x, y, orientazione, dimensioni_navi[i]);
			if ( !valid) { printf("Piazzamento della nave %s invalido, riprovare\n");}
		} while ( !valid);
		navi_validate[i].riga = x;
                navi_validate[i].colonna = y;
                navi_validate[i].direzione = orientazione;
		offset += snprintf( buffer + offset, sizeof(buffer) - offset, "%d %d %c ",
				navi_validate[i].riga,
				navi_validate[i].colonna,
				navi_validate[i].direzione);
		/*scrivere funzione validazione()*/
		/*scrivere codice per mandare array buffer al server*/

int main() {
    /*gestione socket*/
	struct sockaddr_in my_addr;
	int ds_socket, ip, connessione, mio_id;
	char buffer[BUFFER_SIZE];
	/*settaggio socket*/
	my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(10000);
	ds_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( ds_socket == -1) { perror("errore"); return -1;}
	ip = inet_pton( AF_INET, IP , &my_addr.sin_addr );
	if ( ip == 0 || ip == -1) { perror("errore"); return -2;}
	/*connessione socket*/
	connessione = connect( ds_socket, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_in));
	if ( connessione == -1) {perror("errore"); return -3;}
	/*definizione modalità di gioco*/
	#if defined(MODE_1vs1)
	char mod[] = "1vs1\n";
	#elif defined(MODE_TCT)
	char mod[] = "TCT\n";
	#else
	#error "specificare la modalità che si desidera giocare"
	#endif
	/*collegamento e comunicazione con server*/
	int mes_s = send( ds_socket, mod, strlen(mod), 0 );
	if ( mes_s == -1) {perror("errore"); return -4;}
	memset( buffer, 0, sizeof(buffer));
	int mes_r = recv( ds_socket, buffer, sizeof(buffer)-1, 0);
	if ( mes_r == -1) {perror("errore"); return -5;}
	else if ( mes_r == 0) {printf("il server ha chiuso la connessione\n"); return 0;}
	buffer[mes_r] = '\0';
	if ( sscanf( buffer, "WELCOME %d", &mio_id) != 1 ) {perror("errore"); return -6;}
	printf("%s\n", buffer);
	piazzamento_navi();
}
