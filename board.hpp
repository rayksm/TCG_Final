#ifndef __BOARD__
#define __BOARD__ 1

#define RED 0
#define BLUE 1

#define PIECE_NUM 6
#define MAX_TREE_DEPTH 6

#define mini_m -10000
#define max_m 10000
#define MIN_EVAL -10000
#define MAX_EVAL 10000
#define MAX_SIM 100

// hash parameter
#define MAX_COLOR 2     // e.g. color 0 red and 1 blue
#define MAX_DICE 6
#define MAP_SIZE 5 
struct TTEntry {
    double alpha;
    double beta;
    double m;
};

typedef struct _board
{
    // all captured: piece_bits becomes 0
    unsigned char piece_bits[2];
    int piece_position[2][PIECE_NUM];
    // blank is -1
    int board[25];
    char moves[PIECE_NUM][2];
    int move_count, heuristic_depth, tree_depth;
    //double alpha, beta;
    char moving_color;
    char dice;
    void init_with_piecepos(int input_piecepos[2][6], char input_color);
    void move(int id_with_dice);
    void generate_moves();
    bool check_winner();
    void print_board();

    // not basic functions, written in decide.cpp
    bool simulate();
    int decide();
    int first_move_decide_dice();
    
    double star1(double alpha, double beta);
    double negascout(double alpha, double beta);
    void evaluation();

} Board;
#endif
