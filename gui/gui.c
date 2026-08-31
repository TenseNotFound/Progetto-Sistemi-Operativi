#include "gui.h"
#include "../protocollo/protocollo.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

char grid[GRID_SIZE][GRID_SIZE];
char target_grids[MAX_PLAYER +1][GRID_SIZE][GRID_SIZE]; // MAX_PLAYER +1 perchè gli id partono da 1
static bool grigliaN_inizializz[MAX_PLAYER +1] = {false}; // tiene traccia di quale griglia è inizializzata o meno
static bool ingame = false;
static char esito[128] = ""; // mi serve perchè non riuscivo mai a vedere le notifiche di HIT a schermo, così è sicuro che vengono stampate perchè è direttamente draw_grids() a stampare le notifiche
uint8_t targetId = 0;


typedef struct bersagli{
    uint8_t id;
    char nome[USERNAME];
    struct bersagli *next;
}nemici;

nemici *target = NULL;


void gui_init(bool flag){
    clean_screen();

    if(flag){
        printf(GIALLO " [!] Modalità connessione automatica attiva. \n [*] Startup in corso...\n" RESET);
    }
    
    printf(BLU "====================================================================\n");
    printf("                   BATTAGLIA NAVALE - CLIENT v1.0                   \n");
    printf("====================================================================\n"RESET);
    printf(" Progetto Sistemi Operativi - A.A. 2025/2026                        \n");
    printf(" Realizzato da:                                                     \n");
    printf(VERDE"  [*] Lorenzo Tarantino (TenseNotFound)                             \n");
    printf("  [*] Leonardo (lrcicalini)                                         \n"RESET);
    printf(" Link alla repository:                                              \n");
    printf(BLU"  [*] https://github.com/TenseNotFound/Progetto-Sistemi-Operativi   \n");
    printf("====================================================================\n\n"RESET);
    
    printf("Benvenuto nel gioco battaglia navale!\n");
    printf(GIALLO" [*] In attesa della connessione con il server...\n\n"RESET);
    
    fflush(stdout);
}

void server_connected(char *ip, unsigned int port, int id){
    printf(VERDE" [*] Connessione stabilita con il server %s:%u\n" RESET, ip, port);
    printf(" [*] Il tuo ID e': " VERDE "%d\n" RESET, id);
    fflush(stdout);
}

void init_board(){
    cleanup_board(grid);
    targetId = 0;
    for(int i = 0; i<=MAX_PLAYER; i++){
        grigliaN_inizializz[i] = false; 
    }
}

void cleanup_board(char grid[GRID_SIZE][GRID_SIZE]){
    for(int i = 0; i<GRID_SIZE; i++){
        for(int j = 0; j<GRID_SIZE; j++){
            grid[i][j] = '~';
        }
    }
}

void init_target_board(uint8_t id){
    if(id > MAX_PLAYER) return;
    if(!grigliaN_inizializz[id]) {
        cleanup_board(target_grids[id]);
        grigliaN_inizializz[id] = true;
    }
}

static void draw_board(char griglia[GRID_SIZE][GRID_SIZE]) {
    char buffer[BOARD_BUFFER_SIZE];
    size_t offset = 0;
    int scritto = 0;
    char temp[16];

    int col_width = snprintf(temp, sizeof(temp), "%d", GRID_SIZE);
    scritto = snprintf(buffer + offset, sizeof(buffer) - offset, "    ");
    offset += (scritto > 0) ? (size_t) scritto : 0;

    for (int i = 0; i<GRID_SIZE; i++){
        scritto = snprintf(buffer + offset, sizeof(buffer) - offset, "%-*d", col_width, i+1);
        offset += (scritto > 0) ? (size_t) scritto : 0;
    }

    scritto = snprintf(buffer + offset, sizeof(buffer)- offset, "\n"); // mando a capo dopo la riga dell'intestazione
    offset += (scritto > 0) ? (size_t) scritto : 0;

    for(int i = 0; i<GRID_SIZE; i++){

        scritto = snprintf(buffer + offset, sizeof(buffer) - offset, " %c ", 'A' + i);
        offset += (scritto > 0) ? (size_t) scritto : 0;
        for(int j = 0; j<GRID_SIZE; j++){
            scritto = snprintf(buffer+offset, sizeof(buffer) - offset, "|%s%c%s",
                                griglia[i][j] == 'N' ? NAVE :
                                griglia[i][j] == 'X' ? COLPITO :
                                griglia[i][j] == 'O' ? MANCATO :
                                ACQUA,
                                griglia[i][j],
                                RESET);
            offset += (scritto > 0) ? (size_t) scritto : 0;
        }

        scritto = snprintf(buffer + offset, sizeof(buffer) - offset, "|\n");
        offset += (scritto > 0) ? (size_t) scritto : 0;
    }

    scritto = snprintf(buffer + offset, sizeof(buffer) - offset, "\n");
    offset += (scritto > 0) ? (size_t) scritto : 0;

    fputs(buffer, stdout);
    fflush(stdout);
}

void draw_grids(){
    printf(VERDE"\n--- LA TUA FLOTTA ---\n"RESET);
    draw_board(grid); 
    
    printf(ROSSO"\n--- RADAR NEMICO ---\n"RESET);
    if(targetId == 0) {
        printf(GIALLO" [!] Nessun nemico ancora selezionato\n"RESET);
    } else {
        printf(" [*] Radar del player id:%d\n", targetId);
        draw_board(target_grids[targetId]);
    }

    if(esito[0] != '\0'){
        printf("\n%s\n", esito);
    }
    fflush(stdout);
}

void clean_screen(){
    printf(CLEAN); //codice ANSI per pulire lo schermo
}

void close_game(){
    if(ingame) printf(VERDE"\n [!] Grazie per aver giocato! Arrivederci!\n"RESET);
    else printf(GIALLO"\n [!] Chiusura del client\n"RESET);
    fflush(stdout);
    exit(0);
}

void addboat(int x, int y, char orientazione, uint8_t size){
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

    printf(BLU "\n [*] Scegli la modalità di gioco! \n"RESET);
    printf(VERDE"    [1] 1v1 \n"RESET);
    printf(GIALLO"    [2] Default [Premi INVIO o qualsiasi altro tasto]\n"RESET);
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

    printf(ROSSO"\n [!] Connessione al server persa\n"RESET);
    fflush(stdout);
}

void bersagli(uint8_t id, char *username){
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

static bool bersaglio_valido(int id){
    for(nemici *curr = target; curr != NULL; curr = curr->next){
        if(curr->id == id) return true;
    }
    return false;
}

static void free_bersagli(void){
    nemici *curr = target, *temp;
    while(curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    target = NULL;
}

void turno() {
    printf(VERDE"\n==================================================\n");
    printf(" [*] È IL TUO TURNO \n");
    printf("==================================================\n"RESET);

    printf(ROSSO"\n--- BERSAGLI DISPONIBILI ---\n"RESET);
    nemici *curr = target;
    int i = 0;
    while(curr != NULL) {
        printf(BLU " [%d] [ID: %d] %s\n"RESET,i+1 ,curr->id, curr->nome);
        curr = curr->next;
        i++;
    }
    printf(ROSSO "----------------------------\n"RESET);
    fflush(stdout);
}

void ricezione_mossa(azioni *mossa) {
    int bersaglio, y;
    char x_in;
    bool mossa_valida = false;

    mossa->type = MOVE;
    int letti;
    while (!mossa_valida) {
        
        printf("\n [*] Inserisci l'ID del giocatore da colpire: ");
        fflush(stdout);
        if((letti = scanf("%d", &bersaglio)) == EOF) close_game();
        if(letti != 1 || !bersaglio_valido(bersaglio)) {
            printf(GIALLO" [!] Inserisci un id tra quelli disponibili\n"RESET);
            fflush(stdout);
            fflush_stdin(); 
            continue;
        }

        if(bersaglio == mossa->player_id){
            printf(GIALLO" [!] Non puoi fare fuoco a te stesso!\n"RESET);
            fflush(stdout);
            fflush_stdin();
            continue;
        }
        while(!mossa_valida){
            printf("Inserisci le coordinate <Y> <n>: ");
            letti = scanf(" %c %d", &x_in, &y);
            if(letti == EOF) close_game();
            if(letti != 2){
                printf(GIALLO" [!] Inserisci delle coordinate valide !\n"RESET);
                fflush_stdin();
                fflush(stdout);
                continue;
            }

            fflush_stdin();

            y = y - 1;
            int x = -1;

            if (x_in >= 'a' && x_in <= 'j') x = x_in - 'a'; // CAST ASCII -> 'a' = 97; 'A' = 65
            else if (x_in >= 'A' && x_in <= 'J') x = x_in - 'A';

            if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
                init_target_board((uint8_t)bersaglio);
                if (target_grids[bersaglio][x][y] == 'X' || target_grids[bersaglio][x][y] == 'O') {
                    printf(GIALLO" [!] Hai già sparato in queste coordinate!\n"RESET);
                    fflush(stdout);
                } else {
                    mossa->target_id = bersaglio;
                    mossa->x = x;
                    mossa->y = y;
                    targetId = (uint8_t)bersaglio;
                    mossa_valida = true;
                }
            } else {
                printf(ROSSO" [!] Coordinate non valide. Devi inserire Lettera (A-J) e Numero (1-10).\n"RESET);
                fflush(stdout);
            }
        }
    }
    free_bersagli();
}

void errore_invio_mossa(void) { 
    printf(ROSSO"\n [!] Errore: Impossibile inviare la mossa al server.\n"RESET);
    fflush(stdout);
}

void non_mio_turno(uint8_t id) {
    printf(GIALLO"\n [*] È il turno del giocatore %u. In attesa...\n"RESET, id);
    fflush(stdout);
}

void colpito() {
    snprintf(esito, sizeof(esito), VERDE"\n [+] BERSAGLIO COLPITO!\n"RESET);
    fflush(stdout);
}

void miss() {
    snprintf(esito, sizeof(esito), GIALLO"\n [-] Mancato!\n"RESET);
    fflush(stdout);
}

void nave_affondata(const char *nave, uint8_t id){
    snprintf(esito, sizeof(esito), VERDE" [!] Hai affondato %s (%d)!\n"RESET, nave, id);
    fflush(stdout);
}

void spettatore(azioni *pck) {
    char esito_str[20];
    if (pck->type == HIT) strcpy(esito_str, VERDE"COLPITO"RESET);
    else strcpy(esito_str, GIALLO"MANCATO"RESET);
    
    printf(BLU"\n [*] Il giocatore %d ha sparato al giocatore %d in %c%d: %s!\n"RESET, 
            pck->player_id, pck->target_id, pck->x + 'A', pck->y + 1, esito_str);
    fflush(stdout);
}

void s_eliminato() {
    printf(ROSSO"\n=====================================================\n");
    printf(" [!] SEI STATO ELIMINATO! LA TUA FLOTTA È AFFONDATA [!]\n");
    printf("=======================================================\n"RESET);
    printf(CIANO"[*] MODALITÀ SPETTATORE ATTIVA\n"RESET);
    printf(CIANO"[*] Sei ora uno spettatore\n"RESET);
    printf(GIALLO"[*] Rimani in attesa per guardare il resto della partita.\n"RESET);
    fflush(stdout);
}

void eliminato(int id) {
    printf(ROSSO"\n [!] IL GIOCATORE %d È STATO ELIMINATO!\n"RESET, id);
    fflush(stdout);
}

void s_vittoria() {
    printf(VERDE"\n==================================================\n");
    printf("                [*] VITTORIA! [*]                   \n");
    printf("===================================================\n"RESET);
    fflush(stdout);
}

void vittoria(uint8_t id) {
    printf(VERDE"\n==================================================\n");
    printf(" [*] LA PARTITA È CONCLUSA! HA VINTO IL GIOCATORE %u [*] \n", id);
    printf("===================================================\n"RESET);
    fflush(stdout);
}

void fflush_stdin(void){
	int c;
	while((c = getchar()) != '\n' && c != EOF);
}

void welcomeback(char *buff){
    printf(VERDE" [*] Bentornato %s! \n"RESET, buff);
    fflush(stdout);
}

void invalid_input(void){
    printf(ROSSO" [!] Input non valido! \n Sintassi corretta: <x> <y> <orientamento (N,S,E,O)>\n"RESET); 
    fflush(stdout);
}

void posizionamento_ok(const char *nave, int x, int y, char orientazione){
    printf(VERDE" [*] Nave %s posizionata in (%d,%d) con orientamento %c\n"RESET, nave, x, y, orientazione);
    fflush(stdout);
}

void chiusura_forzata(void){
    printf(ROSSO"\n [!] Chiusura forzata del client, inizio routine di shutdown...\n"RESET);
    fflush(stdout);
}

void waiting_player(void){
    ingame = true;
    printf(GIALLO"\n [*] In attesa che tutti i giocatori siano pronti...\n"RESET);
    fflush(stdout);
}

void connection_lost_fallback(const char *ip_buf, unsigned int port){
    printf(GIALLO" [*] Connessione a %s:%u fallita, provo il discovery automatico...\n"RESET, ip_buf, port);
	fflush(stdout);
}

void mod_ospite(void){
    printf(GIALLO" [!] Modalità ospite attivata\n"RESET);
    fflush(stdout);
}

void s_nave_affondata(const char *nave, uint8_t id){
    printf(ROSSO" [!] Il giocatore con id %d ha affondato la nave %s della tua flotta!\n"RESET, id, nave);
    fflush(stdout);
}

void discovery_server_errore(const char *pname){
    printf(ROSSO " [!] Errore nel discovery del server, inserisci manualmente ip e porta\n Sintassi: %s <IP> <port>\n"RESET, pname);
    fflush(stdout);
}

void sintassi_corretta(const char *pname){
    printf(GIALLO " [!] Sintassi corretta: %s <IP> <port>\n"RESET, pname);
    fflush(stdout);
}

void invalid_port(){
	printf(GIALLO " [!] Inserisci un numero di porta valido nel range 5000-65535, provo ora il discovery automatico...\n"RESET);
    fflush(stdout);
}

void server_not_found(){
    printf(ROSSO " [!] Errore: impossibile trovare un server\n" RESET); 
    fflush(stdout);
}

void connection_error(){
    printf(ROSSO" [!] Impossibile stabilire una connessione con il server\n"RESET);
    fflush(stdout);
}

void invalid_ip(const char *ip){
    printf(GIALLO" [!] Indirizzo ip non valido: %s\n"RESET, ip); 
    fflush(stdout);
}

void inserisci_username(){
    printf("\n [*] Inserisci il tuo username: ");
    fflush(stdout);
}

void username_too_long(size_t len){
    printf(GIALLO " [!] Inserisci un username di massimo %zu caratteri!\n"RESET, len - 1);
    fflush(stdout);
}

void inserisci_coordinate(const char *nave, uint8_t size){
    printf(" [*] Inserisci le coordinate della nave %s (dimensione %u) e l'orientamento (N,S,E,O):\n", nave, size);
    fflush(stdout);
}

void invalid_placement(){
    printf(GIALLO" [!] Posizionamento non valido: la nave esce dalla griglia o si sovrappone con un'altra nave. Riprova.\n"RESET);
	fflush(stdout);
}

void discovery_req_sent(){
    printf(GIALLO" [*] Richiesta di discovery inviata con successo (broadcast). \n In attesa di riscontro dal server...\n"RESET);
    fflush(stdout);
}

void server_found(const char *ip, unsigned int port){
    printf(VERDE" [*] Server trovato con successo!\n [*] In ascolto su %s:%u\n"RESET, ip, port);
	fflush(stdout);
}

void failed_attempt(int i){
    printf(GIALLO" [!] Tentativo %d fallito (Timeout), riprovo...\n"RESET, i + 1);
    fflush(stdout);
}

void sig_received(void){
    printf(GIALLO" [*] Ricevuta segnalazione, interrompo... \n"RESET);
    fflush(stdout);
}

void server_not_found_attempt(){
    printf(ROSSO" [*] Nessun server trovato in rete dopo %d tentativi.\n"RESET, TENTATIVI);
    fflush(stdout);
}

void mostra_flotta(){
    printf(VERDE"\n--- LA TUA FLOTTA ---\n"RESET);
    draw_board(grid);
}