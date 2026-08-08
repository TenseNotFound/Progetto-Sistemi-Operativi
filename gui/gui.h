#ifndef GUI_H
#define GUI_H
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>


void gui_init(int flag);
char *init_board();
void draw_board(char grid[GRID_SIZE][GRID_SIZE]);
void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]);
void server_connected(char *ip, int port, int id);

void clean_screen();

void close_game();

#endif