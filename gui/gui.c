#include "gui.h"
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

char grid[GRID_SIZE][GRID_SIZE];
char target_grid[GRID_SIZE][GRID_SIZE];


// spostare poi in protocollo.c

typedef struct bersagli{
    int id;
    char nome[USERNAME];
    struct bersagli *next;
}nemici;

nemici *target = NULL;


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

int game_mode(){

    char input[BUFFER_SIZE];

    printf("\n [*] Scegli la modalità di gioco! \n");
    printf("   [1] 1v1 \n");
    printf("   [2] Default [Premi invio o qualsiasi altro tasto]\n");
    fflush(stdout);

    if(fgets(input, sizeof(input), stdin) == NULL){
        return MODE_DEFAULT;
    }

    input[strlen(input)-1] = '\0';

    if(strcmp(input, "1") == 0){
        return MODE_1V1;
    } 
    return MODE_DEFAULT;
}

void connection_lost(void){

    printf("\n[!] Connessione al server persa\n");
    fflush(stdout);
}

void bersagli(int id, char *username){
    // serve per aggiungere i nuovi nemici, si usa una lista a puntatori perchè a priori non so quanti bersagli ci sono
    nemici *nemico = malloc(sizeof(nemici));

    if(nemico == NULL){
        perror("Errore nela malloc per allocare il nuovo nemico");
        return;
    }

    nemico->id = id;
    strcpy(nemico->nome, username);
    
    nemico->next = target;
    target = nemico;
}


void turno() {
    printf("\n==================================================\n");
    printf(" [*] È IL TUO TURNO \n");
    printf("==================================================\n");

    printf("\n--- BERSAGLI DISPONIBILI ---\n");
    nemici *curr = target;
    int i = 0;
    while(curr != NULL) {
        printf(" [%d] [ID: %d] %s\n",i+1 ,curr->id, curr->nome);
        curr = curr->next;
        i++;
    }
    printf("----------------------------\n");
    fflush(stdout);

    // stampo i nomi che sono temporanei, quindi poi libero subito
    curr = target;
    nemici *temp;
    while(curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    
    target = NULL;
}

void ricezione_mossa(azioni *mossa) {
    int bersaglio, y;
    char x_in;
    bool mossa_valida = false;

    mossa->type = MOVE;

    while (!mossa_valida) {
        
        printf("\nInserisci l'ID del giocatore da colpire: ");
        scanf("%d", &bersaglio);
        fflush_stdin(); 

        printf("Inserisci le coordinate <Y> <n>: ");
        scanf(" %c %d", &x_in, &y);
        fflush_stdin();

        y = y - 1;
        int x = -1;

        if (x_in >= 'a' && x_in <= 'j') x = x_in - 'a'; // CAST ASCII -> 'a' = 97; 'A' = 65
        else if (x_in >= 'A' && x_in <= 'J') x = x_in - 'A';

        if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
            // verifica se le coordinate sono nei limiti
            if (target_grid[x][y] == 'X' || target_grid[x][y] == 'O') {
                printf("[!] Hai già sparato in queste coordinate. Scegli un altro bersaglio!\n");
            } else {
                mossa->target_id = bersaglio;
                mossa->x = x;
                mossa->y = y;
                mossa_valida = true;
            }
        } else {
            printf("[!] Coordinate non valide. Devi inserire Lettera (A-J) e Numero (1-10).\n");
        }
    }
}

void errore_invio_mossa(void) { 
    printf("\n[!] Errore: Impossibile inviare la mossa al server.\n");
    fflush(stdout);
}

void non_mio_turno(int id) {
    printf("\n[*] È il turno del giocatore %d. In attesa...\n", id);
    fflush(stdout);
}

void colpito() {
    printf("\n[+] BERSAGLIO COLPITO!\n");
    fflush(stdout);
}

void miss() {
    printf("\n[-] Mancato!\n");
    fflush(stdout);
}

void spettatore(azioni *pck) {
    char esito_str[20];
    if (pck->type == HIT) strcpy(esito_str, "COLPITO");
    else strcpy(esito_str, "MANCATO");
    
    printf("\n[*] Il giocatore %d ha sparato al giocatore %d in %c%d: %s!\n", 
            pck->player_id, pck->target_id, pck->x + 'A', pck->y + 1, esito_str);
    fflush(stdout);
}

void s_eliminato() {
    printf("\n==================================================\n");
    printf(" [!] SEI STATO ELIMINATO! LA TUA FLOTTA È AFFONDATA [!]\n");
    printf("==================================================\n");
    printf("[*] Rimani in attesa per guardare il resto della partita.\n");
    fflush(stdout);
}

void eliminato(int id) {
    printf("\n[!] IL GIOCATORE %d È STATO ELIMINATO!\n", id);
    fflush(stdout);
}

void s_vittoria() {
    printf("\n==================================================\n");
    printf(" [*] VITTORIA! SEI IL DOMINATORE DEI MARI! [*] \n");
    printf("==================================================\n");
    fflush(stdout);
}

void vittoria(int id) {
    printf("\n==================================================\n");
    printf(" [*] LA PARTITA È CONCLUSA! HA VINTO IL GIOCATORE %d [*] \n", id);
    printf("==================================================\n");
    fflush(stdout);
}