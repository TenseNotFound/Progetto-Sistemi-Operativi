#include "gui.h"
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static char grid[GRID_SIZE][GRID_SIZE];
static char target_grid[GRID_SIZE][GRID_SIZE];

void gui_init(bool flag){
    clean_screen();

    if(flag){
        printf("[!] Modalità connessione automatica attiva. \n[*] Startup in corso...\n");
    }
    
    printf("====================================================================\n");
    printf("                   BATTAGLIA NAVALE - CLIENT v1.0                   \n");
    printf("====================================================================\n");
    printf(" Progetto Sistemi Operativi - A.A. 2025/2026                        \n");
    printf(" Realizzato da:                                                     \n");
    printf("  * Lorenzo Tarantino (TenseNotFound)                               \n");
    printf("  * Leonardo (lrcicalini)                                           \n");
    printf(" Link alla repository:                                              \n");
    printf("  https://github.com/TenseNotFound/Progetto-Sistemi-Operativi       \n");
    printf("====================================================================\n\n");
    
    printf("Benvenuto nel gioco battaglia navale!\n");
    printf("[*] In attesa della connessione con il server...\n\n");
    
    fflush(stdout);
}

void server_connected(char *ip, int port, int id){
    printf("[*] Connessione stabilita con il server %s:%d\n", ip, port);
    printf("[*] Il tuo ID è: %d\n", id);
    fflush(stdout);
}

void init_board(){
    cleanup_board(grid);
    cleanup_board(target_grid);
    draw_board(grid);
}

void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]){
    for(int i = 0; i<GRID_SIZE; i++){
        for(int j = 0; j<GRID_SIZE; j++){
            grid[i][j] = '~';
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

void draw_grids(){
    printf("\n--- LA TUA FLOTTA ---\n");
    draw_board(grid); 
    
    printf("\n--- RADAR NEMICO ---\n");
    draw_board(target_grid);
}

void clean_screen(){
    system("clear");
}

void close_game(){
    printf("Grazie per aver giocato! Arrivederci!\n");
    fflush(stdout);
    clean_screen();
    exit(0);
}

void addboat(int x, int y, char orientazione, int size){
    int riga_off = 0;
    int col_off = 0;

    if (orientazione == 'N') { 
        riga_off = -1; // alto
    } else if (orientazione == 'S') { 
        riga_off = 1;  // basso
    } else if (orientazione == 'E') { 
        col_off = 1;   // destra
    } else if (orientazione == 'O') { 
        col_off = -1;  // sinistra
    }

    for (int i = 0; i < size; i++) {
        int r = x + (i * riga_off);
        int c = y + (i * col_off);
        
        if (r >= 0 && r < GRID_SIZE && c >= 0 && c < GRID_SIZE) {
            grid[r][c] = 'N'; 
        }
    }
}