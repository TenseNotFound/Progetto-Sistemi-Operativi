#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#define SHIP_NUMBER 5
#define GRID_SIZE 10
#define BACKLOG 16
#define MAX_PLAYER 32 // scelta obligata via dimensioni fisse dei semafori, ho bisogno a priori di un semaforo con dim ≥ del numero n di client connessi per 
                      // poter accettare correttamente tutti i nuovi player
#define TIMEUOUT_SHOUTDOWN 10 // (secondi)
#define AFK_TIMEOUT 180 // (secondi) inattività AFK
#define TIMEOUT_LOBBY 30
#define DISCOVERY_PORT 9999
#define DISCOVER "DISCOVER"
#define BUFFER_SIZE 256
#define TENTATIVI 20 // per la ricerca della porta
#define USERNAME 17

typedef enum game_info{ // serve poi per capire che mossa/azione è stata fatta/compiuta/subita sul client
    WELCOME, JOIN, TURN, MOVE, HIT, MISS, ELIMINATED, WIN, MODE, INFO
}game_info;

typedef enum mode {
    MODE_DEFAULT, 
    MODE_1V1
} mode;


typedef struct azioni{ // mi definisce la mossa
    game_info type; // specifico il tipo di azione
    uint8_t player_id;
    uint8_t target_id;
    uint8_t x,y; // posizione nave da colpire
    mode gamemode;
    char username[USERNAME];
} azioni;

typedef struct posizionamento{ // mi definisce la nave
    uint8_t index; //indice in ship_type
    uint8_t x,y; //coordinate nave 
    char orientation; //orientamento della nave (N,S,E,O)
}posizionamento;

typedef struct navi{
    const char *name;
    uint8_t size; // così un solo byte, con numeri da 0-255 rispetto a 4 byte
} nave;

extern const nave ship_type[SHIP_NUMBER]; // così non creo 3 blocchi di memoria identica 
                                          // serve const così non ci sono problemi di scrittura quindi sincronizzazione
                                          // è read-only 

#endif