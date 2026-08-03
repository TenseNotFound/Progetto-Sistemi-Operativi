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

#define BUFFER_SIZE 256
#define IP "127.0.0.1"
#define mod 0           // rimuovere poi questa riga

typedef struct {
	int riga;
	int colonna;
	char direzione;
} Nave;

void piazzamento_navi() {
	bool board[10][10] = {false, false, false, false, false, false, false, false, false, false};
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
    }
}

int main() {
    /*gestione socket*/
	struct sockaddr_in my_addr;
	int ds_socket, ip, connessione, mio_id, ret;
	char buffer[BUFFER_SIZE];
	/*settaggio socket*/
	my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(10000);
	ds_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    
	if ( ds_socket == -1) { 
        perror("errore nell'apertura del socket"); 
        return -1;
    }

	ip = inet_pton( AF_INET, IP , &my_addr.sin_addr ); // cambiare perchè il prof non l'ha spiegata
	if (ip<=0) { 
        perror("errore nel collegarsi al server"); 
        exit(-1);
    }

	/*connessione socket server*/

	if (connect( ds_socket, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("errore nella connect al server"); 
        exit(-1);
    }
    /*
        definizione modalità di gioco
        #if defined(MODE_1vs1)
        char mod[] = "1vs1\n";
        #elif defined(MODE_TCT)
        char mod[] = "TCT\n";
        #else
        #error "specificare la modalità che si desidera giocare"
        #endif

        la modalità di gioco varia in base a quanti utenti sono collegati, di default va in multiplayer
    */
	
	/*collegamento e comunicazione con server*/

    /*
        la mod qua non serve perchè è il server che decide la modalità in base a quante connessioni ci sono
        nel protocollo c'è una voce MODE che specifica poi ai client in che modalità aprire la gui
        facciamo 2 gui: una per TCT e una per l'1v1 contro il bot (il bot entra quando c'è solo un utente collegato)
    */

	if ( send(ds_socket, mod, strlen(mod), 0 ) == -1) {
        perror("errore nel mandare dati al server"); 
        exit(-1);
    }

	if ( (ret = recv( ds_socket, buffer, sizeof(buffer)-1, 0) == -1)) {
        perror("errore nel ricevere i dati dal server"); 
        exit(-1);
    } else if ( ret == 0) {
        printf("il server ha chiuso la connessione\n"); 
        /*
            per il momento return 0 è ok ma va inserita una routine di chiusura del client
            una volta chiuso tutto (strutture dati, facciamo mandare un messaggio al server)
        */
        return 0;
    }
	// funzione per il piazzamento delle navi piazzamentonavi()
}
