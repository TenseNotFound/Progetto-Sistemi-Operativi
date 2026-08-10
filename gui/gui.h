#ifndef GUI_H
#define GUI_H
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


void gui_init(bool flag);
void init_board();
void draw_board(char grid[GRID_SIZE][GRID_SIZE]);
void addboat(int x, int y, char orientazione, int size);
void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]);
void server_connected(char *ip, int port, int id);
void draw_grids();

void clean_screen();

void close_game();

#endif