/*
    FUNZIONI RIPETUTE ED IDENTICHE TRA sever.c E client.c
    QUI SONO CONTENUTE LE DICHIARAZIONI DELLE FUNZIONI
*/

#include "../protocollo/protocollo.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>


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
				if(shutdown_flag) return -1;
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
				if(shutdown_flag) return -1;
				continue;
			}
            return -1;
        } if(w == 0) return (ssize_t)(n - left); // il client ha chiuso la connessione, analogo chiusura PIPE
        left -= (size_t)w;
        p += w;
    }
    return (ssize_t)n; // se arrivo qui ho scritto tutti i byte richiesti e comunico con n
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