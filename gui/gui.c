#include "gui.h"
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>

void gui_init(){
    clear_screen();
    printf("Benvenuto nel gioco della battaglia navale!\n");
    printf("In attesa di connessione al server...\n");
    fflush(stdout);
}

void init_board(){
    char grid[GRID_SIZE][GRID_SIZE];
    cleanup_board(grid);
    draw_board(grid);
}

void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]){
    for(int i = 0; i<GRID_SIZE; i++){
        for(int j = 0; j<GRID_SIZE; j++){
            grid[i][j] = "~";
        }
    }
}

void draw_board(char griglia[GRID_SIZE][GRID_SIZE]) {
    printf("    "); 
    for(int i = 0; i < GRID_SIZE; i++){
        if (i < 9) {
            printf("%d ", i + 1); 
        } else {
            printf("%d", i + 1);
        }
    }
    printf("\n");

    for(int i = 0; i < GRID_SIZE; i++){
        printf(" %c ", 'A' + i);
        for(int j = 0; j < GRID_SIZE; j++){
            printf("|%c", griglia[i][j]);
        }
        printf("|\n"); 
    }
    printf("\n");
    fflush(stdout);
}

void clean_screen(){
    system("clear");
}

void close_game(){
    printf("Grazie per aver giocato! Arrivederci!\n");
    fflush(stdout);
    clear_screen();
    exit(0);
}