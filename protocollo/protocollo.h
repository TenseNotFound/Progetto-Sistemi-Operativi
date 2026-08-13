#ifndef PROTOCOL_H
#define PROTOCOL_H

#define SHIP_NUMBER 5
#define GRID_SIZE 10
#define BACKLOG 16
#define DISCOVERY_PORT 9999
#define DISCOVER "DISCOVER"
#define TIMEOUT 5
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
    int player_id;
    int target_id;
    int x,y; // posizione nave da colpire
    mode gamemode;
    char username[USERNAME];
} azioni;

typedef struct posizionamento{ // mi definisce la nave
    int index; //indice in ship_tipe
    int x,y; //coordinate nave 
    char orientation; //orientamento della nave (N,S,E,O)
}posizionamento;

typedef struct navi{
    const char *name;
    int size;
    posizionamento posizioni[5]; // massimo 5 navi, quindi massimo 5 posizioni
} nave;

extern const nave ship_tipe[SHIP_NUMBER]; // così non creo 3 blocchi di memoria identica 
                                          // serve const così non ci sono problemi di scrittura quindi sincronizzazione
                                          // è read-only 

#endif