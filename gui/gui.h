#ifndef GUI_H
#define GUI_H
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>


void init_board();
void draw_board(char grid[GRID_SIZE][GRID_SIZE]);
void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]);

void clear_screen();

void gui_init();
void close_game();

#endif