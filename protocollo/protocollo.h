typedef enum game_info{ // serve poi per capire che mossa/azione è stata fatta/compiuta/subita sul client
    WELCOME, JOIN, TURN, MOVE, HIT, MISS, ELIMINATED, WIN, MODE
}game_info;


typedef struct azioni{
    game_info type; // specifico il tipo di azione
    int player_id;
    int target_id;
    int x,y; // posizione nave da colpire
} azioni;