#define SHIP_NUMBER 5
#define GRID_SIZE 10

typedef enum game_info{ // serve poi per capire che mossa/azione è stata fatta/compiuta/subita sul client
    WELCOME, JOIN, TURN, MOVE, HIT, MISS, ELIMINATED, WIN, MODE
}game_info;


typedef struct azioni{
    game_info type; // specifico il tipo di azione
    int player_id;
    int target_id;
    int x,y; // posizione nave da colpire
} azioni;

typedef struct navi{
    const char *name;
    int size;
} nave;

static const nave ship_tipe[SHIP_NUMBER] = {
    {"Portaerei", 5 },
    {"Corazzata", 4 },
    {"Incrociatore", 3},
    {"Sottomarino", 3},
    {"Cacciatorpediniere", 2}
};

typedef struct posizionamento{
    int index; //indice in ship_tipe
    int x,y;
    int orientamento; // 1 -> orizzontale, 0 verticale 
}posizionamento;