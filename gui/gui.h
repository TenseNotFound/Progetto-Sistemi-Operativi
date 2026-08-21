#ifndef GUI_H
#define GUI_H
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#define BOARD_BUFFER_SIZE 4096
#define ROSSO    "\x1b[31m"
#define VERDE    "\x1b[32m"
#define GIALLO   "\x1b[33m"
#define BLU      "\x1b[34m"
#define CIANO    "\x1b[36m"
#define BIANCO   "\x1b[37m"
#define RESET    "\x1b[0m" // colore di default del terminale

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
void draw_board(char grid[GRID_SIZE][GRID_SIZE]);
void addboat(int x, int y, char orientazione, uint8_t size);
void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]);
void server_connected(char *ip, int port, int id);
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

void fflush_stdin(void); // serve per pulire il buffer di input, così da evitare che rimangano caratteri in stdin, visto che fflush(stdin) non esiste, lo creo io


#endif