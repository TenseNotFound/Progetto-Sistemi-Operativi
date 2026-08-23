/*
    FUNZIONI RIPETUTE ED IDENTICHE TRA sever.c E client.c
    QUI SONO CONTENUTE LE FIRME DELLE FUNZIONI
*/

#ifndef UTILS_H
#define UTILS_H
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

extern volatile sig_atomic_t shutdown_flag; // mi serve perchè sia client.c che server.c usano un procedimento di sblocco da queste funzioni basato su shutdown_flag impostato dal gestore ad 1
                                            // extern perchè definita in client.c e server.c
#ifdef _WIN32
    int recv_msg(SOCKET fd, azioni *msg); // IDENTICA -> serve per la ricezione di messaggi 
    int send_msg(SOCKET fd, azioni *msg); // IDENTICA -> serve per la spedizione di messaggi
    SSIZE_T readn(SOCKET fd, void *buf, size_t n); // serve per controllare l'avvenuta lettura di tutti i dati in rete		IDENTICA
    SSIZE_T writen(SOCKET fd, const void *buf, size_t n); // serve per controllare l'avvenuta scrittura di tutti i dati in rete 	IDENTICA
#else
    int recv_msg(int fd, azioni *msg); // IDENTICA -> serve per la ricezione di messaggi 
    int send_msg(int fd, azioni *msg); // IDENTICA -> serve per la spedizione di messaggi
    ssize_t readn(int fd, void *buf, size_t n); // serve per controllare l'avvenuta lettura di tutti i dati in rete		IDENTICA
    ssize_t writen(int fd, const void *buf, size_t n); // serve per controllare l'avvenuta scrittura di tutti i dati in rete 	IDENTICA
#endif

#endif