//#include "../../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(Windows)
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#else
#include <sys/select.h>
#include <arpa/inet.h>
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
#endif

#define BUFFER_SIZE 256
#define IP "127.0.0.1"
//#define mod 0           // rimuovere poi questa riga
/* definire comportamento fflush(stdout) per printf() */

typedef struct {
	int riga;
	int colonna;
	char direzione;
	} Nave;

bool validazione( bool board[10][10], int x, int y, char orientazione, int dimensione_nave ) {

    int delta_riga, delta_colonna, r, c;
	if ( orientazione != 'N' && orientazione != 'S' && orientazione != 'E' && orientazione != 'O' ) {printf("orientazione scelta non valida\n");
													return false; }
	else if ( orientazione == 'N' ) { delta_riga = -1; delta_colonna = 0;}
	else if ( orientazione == 'S' ) { delta_riga = 1; delta_colonna = 0;}
	else if ( orientazione == 'E' ) { delta_riga = 0; delta_colonna = 1;}
	else if ( orientazione == 'O' ) { delta_riga = 0; delta_colonna = -1;}
	for ( int i = 0; i < dimensione_nave; i++ ) {
	r = x + i * delta_riga;
	c = y + i * delta_colonna;
	if ( r < 0 || r >= 10 || c < 0 || c >= 10 ) { return false;}
	else if ( board[r][c] == true ) { return false;} }
	for ( int i = 0; i < dimensione_nave; i++ ) {
	    r = x + i * delta_riga;
        c = y + i * delta_colonna;
	    board[r][c] = true; }
	return true;
}

int piazzamento_navi () {

	bool board[10][10] = {false}; /* non serve impostare tutto a false, ci pensa il compilatore in quanto la matrice viene riempita con 0, ovvero
					il valore booleano corrispondente a false */
	int dimensioni_navi[5] = { 5, 4, 3, 3, 2};
	const char *nomi_navi[5] = {"Portaerei", "Corazzata", "Incrociatore", "Incrociatore", "Cacciatorpediniere"};
	Nave navi_validate[5];
	bool valid;
	int x, y;
	char orientazione;
	char buffer[40];
	int offset = 0;
	for ( int i = 0; i < 5; i++) {
		printf("Piazzare la nave %s, di dimensione %d, lungo una delle direzioni possibili: N S E O\n", nomi_navi[i], dimensioni_navi[i]);
		do {
			scanf( "%d %d %c", &x, &y, &orientazione);
			fflush(stdin);
			valid = validazione( board, x, y, orientazione, dimensioni_navi[i]);
			if ( !valid) { printf("Piazzamento della nave %s invalido, riprovare\n", nomi_navi[i]);}
        } while ( !valid);
		        navi_validate[i].riga = x;
                navi_validate[i].colonna = y;
                navi_validate[i].direzione = orientazione;
		offset += snprintf( buffer + offset, sizeof(buffer) - offset, "%d %d %c ",
				navi_validate[i].riga,
				navi_validate[i].colonna,
				navi_validate[i].direzione); }
	printf("%s", buffer ); return 0; }

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
	/*definizione modalit├á di gioco*/
	#if defined(MODE_1vs1)
	char mod[] = "1vs1\n";
	#elif defined(MODE_TCT)
	char mod[] = "TCT\n";
	#else
	#error "specificare la modalit├á che si desidera giocare"
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
return 0;}
