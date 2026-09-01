#ifndef GUI_H
#define GUI_H
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#define BOARD_BUFFER_SIZE 4096
// codici ANSI per i colori
#define ROSSO    "\x1b[31m"
#define VERDE    "\x1b[32m"
#define GIALLO   "\x1b[33m"
#define BLU      "\x1b[34m"
#define CIANO    "\x1b[36m"
#define BIANCO   "\x1b[37m"
#define RESET    "\x1b[0m" // colore di default del terminale
#define CLEAN "\033[H\033[J" // comando ANSI per ripulire

#define ACQUA    CIANO
#define NAVE     BIANCO
#define COLPITO  ROSSO
#define MANCATO  GIALLO

extern char grid[GRID_SIZE][GRID_SIZE];
extern char target_grids[MAX_PLAYER +1 ][GRID_SIZE][GRID_SIZE];
extern uint8_t targetId;

void gui_init(bool flag);
void init_board();
void init_target_board(uint8_t id);
void addboat(int x, int y, char orientazione, uint8_t size);
void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]);
void server_connected(char *ip, unsigned int port, int id);
int game_mode();
void bersagli(uint8_t id, char *username);
void turno(void);
void ricezione_mossa(azioni *buff);
void errore_invio_mossa(void); 
void non_mio_turno(uint8_t id);
void colpito(void);
void miss(void);
void spettatore(azioni *pyl);
void s_eliminato(void); 
void eliminato(int id);
void s_vittoria(void);
void vittoria(uint8_t id);
void connection_lost(void);
void draw_grids();
void clean_screen();
void close_game();
void welcomeback(char *buff);
void invalid_input(char riga, int colonna, char orientamento);
void posizionamento_ok(const char *nave, char riga, int colonna, char orientazione);
void chiusura_forzata(void);
void waiting_player(void);
void connection_lost_fallback(const char *ip_buf, unsigned int port);
void mod_ospite();
void nave_affondata(const char *nave, uint8_t id);
void s_nave_affondata(const char *nave, uint8_t id);
void discovery_server_errore(const char *pname);
void sintassi_corretta(const char *pname);
void invalid_port(void);
void server_not_found(void);
void connection_error(void);
void invalid_ip(const char *ip);
void inserisci_username(size_t max);
void username_too_long(size_t len);
void inserisci_coordinate(const char *nave, uint8_t size);
void invalid_placement(void);
void discovery_req_sent(void);
void server_found(const char *ip, unsigned int port);
void failed_attempt(int i);
void sig_received(void);
void server_not_found_attempt(void);
void mostra_flotta(void);

void fflush_stdin(void); // serve per pulire il buffer di input, così da evitare che rimangano caratteri in stdin, visto che fflush(stdin) non esiste, lo creo io


#endif