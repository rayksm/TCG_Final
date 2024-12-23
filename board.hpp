#ifndef __BOARD__
#define __BOARD__ 1

#define RED 0
#define BLUE 1

#define PIECE_NUM 6
#define MAX_TREE_DEPTH 15

#define mini_m -100
#define max_m 100
#define MIN_EVAL -100
#define MAX_EVAL 100

#define MAX_SIM 50
#define MAX_TABLE 500000

// hash parameter
#define MAX_COLOR 2     // e.g. color 0 red and 1 blue
#define MAX_DICE 6
#define MAP_SIZE 5 
struct TTEntry {
    double alpha;
    double beta;
    double m;
    bool flag = false; 
};

typedef struct _board
{
    // all captured: piece_bits becomes 0
    unsigned char piece_bits[2];
    int piece_position[2][PIECE_NUM];
    // blank is -1
    int board[25];
    char moves[PIECE_NUM][2];
    int move_count, heuristic_depth;
    //int tree_depth;
    //double alpha, beta;
    char moving_color;
    char dice;
    double total_remain_time;
    void init_with_piecepos(int input_piecepos[2][6], char input_color);
    void move(int id_with_dice);
    void generate_moves();
    bool check_winner();
    void print_board();

    // not basic functions, written in decide.cpp
    bool simulate();
    int decide();
    int first_move_decide_dice();
    
    double star1(double alpha, double beta, int depth);
    double negascout(double alpha, double beta, int depth);
    void evaluation();
    void cal_remain_time(double total_time);
    bool time_exceed();

} Board;
#endif
