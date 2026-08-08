#include "gui.h"
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>

void gui_init(int flag){
    clean_screen();

    if(flag == 1){
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

char *init_board(){
    char grid[GRID_SIZE][GRID_SIZE];
    cleanup_board(grid);
    draw_board(grid);
    return grid;
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

void clean_screen(){
    system("clear");
}

void close_game(){
    printf("Grazie per aver giocato! Arrivederci!\n");
    fflush(stdout);
    clear_screen();
    exit(0);
}