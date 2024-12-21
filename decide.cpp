#include "board/board.hpp"
#include "math_lib/maths.hpp"
#include <stdio.h>
#include <random>
#include <math.h>
#include <cmath>
#include <time.h>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <algorithm> // For std::max and std::min
#define mini_m -100
#define max_m 100
#define MIN_EVAL -100
#define MAX_EVAL 100

// hash parameter
#define MAX_COLOR 2     // e.g. color 0 red and 1 blue
#define MAX_DICE 6
#define MAP_SIZE 5 
struct TTEntry {
    double alpha;
    double beta;
    double m;
};

// You should use mersenne twister and a random_device seed for the pseudo-random generator
// Call random_num()%num to randomly pick number from 0 to num-1
std::mt19937 random_num(std::random_device{}());

double double_max(double a, double b){
    return a > b ? a : b;
}
double double_min(double a, double b){
    return a < b ? a : b;
}

//---------------------------------------------------------------------------------------------------------------
// Global Zobrist tables
uint64_t ZobristPiece[MAX_COLOR * MAX_DICE][MAP_SIZE * MAP_SIZE];                 // for indexing by piece type (0, 1) * (0, 1, 2, 3, 4, 5)
uint64_t ZobristColor[MAX_COLOR];                  // +1 for indexing colors (0, 1)
uint64_t ZobristDice[MAX_DICE];                    // For dice values (0, 1, 2, 3, 4, 5)
std::unordered_map<uint64_t, TTEntry> transpositionTable;
// Initialize Zobrist keys (call once at program start)
void initZobristKeys() {
    // Initialize piece-position keys
    for (int r = 0; r < MAX_COLOR * MAX_DICE; r++) {
        for (int c = 0; c < MAP_SIZE * MAP_SIZE; c++) {
            ZobristPiece[r][c] = random_num();
        }
    }

    // Initialize color keys
    for (int color = 0; color < MAX_COLOR; color++) {
        ZobristColor[color] = random_num();
    }

    // Initialize dice keys
    for (int d = 0; d < MAX_DICE; d++) {
        ZobristDice[d] = random_num();
    }
}

// Compute the Zobrist hash of the current board state
uint64_t computeZobristHash(const int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice) {
    uint64_t h = 0ULL;

    // XOR in piece positions
    for (int r = 0; r < MAX_COLOR; r++) {
        for (int c = 0; c < MAX_DICE; c++) {
            int piece = piece_position[r][c];
            if (piece > -1) {
                h ^= ZobristPiece[r * MAX_DICE + c][piece];
            }
        }
    }

    // XOR in moving color
    h ^= ZobristColor[moving_color];

    // XOR in dice
    h ^= ZobristDice[dice];

    return h;
}

// Insert an entry into the transposition table
void insertTT(const int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice, double alpha, double beta, double m) {
    uint64_t hashKey = computeZobristHash(piece_position, moving_color, dice);
    TTEntry entry;
    
    entry.alpha = alpha;
    entry.beta = beta;
    entry.m = m;

    transpositionTable[hashKey] = entry;
}

// Check if a position is in the transposition table
TTEntry* lookupTT(const int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice) {
    uint64_t hashKey = computeZobristHash(piece_position, moving_color, dice);
    auto it = transpositionTable.find(hashKey);
    if (it != transpositionTable.end()) {
        return &(it->second);
    }
    return nullptr;
}
//---------------------------------------------------------------------------------------------------------------
// heuristic evaluation
double bluemap[5][5] = {
    { 0.0,  1.0,  4.0,  8.0, 13.0},
    { 1.0,  1.0,  3.0,  7.0, 12.0},
    { 4.0,  3.0,  2.0,  6.0, 11.0},
    { 8.0,  7.0,  6.0,  5.0, 10.0},
    {13.0, 12.0, 11.0, 10.0,  9.0},
};
double redmap[5][5] = {
    { 9.0, 10.0, 11.0, 12.0, 13.0},
    {10.0,  5.0,  6.0,  7.0,  8.0},
    {11.0,  6.0,  2.0,  3.0,  4.0},
    {12.0,  7.0,  3.0,  1.0,  1.0},
    {13.0,  8.0,  4.0,  1.0,  0.0},
};

// for heuristic evaluation
double Board::evaluation(){
    // blue end move, and red start to move
    double heuristic_estimate = 0.0;
    if(moving_color == 0){       
        int now_piece;
        for(int i = 0; i < PIECE_NUM; i++){
            // player
            now_piece = piece_position[moving_color][i];
            if(now_piece != -1)
                heuristic_estimate -= redmap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate += 1;
            
            // opposite
            now_piece = piece_position[moving_color ^ 1][i];
            if(now_piece != -1)
                heuristic_estimate += bluemap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate -= 1;
        }
    }
    else{
        int now_piece;
        for(int i = 0; i < PIECE_NUM; i++){
            // player
            now_piece = piece_position[moving_color][i];
            if(now_piece != -1)
                heuristic_estimate -= bluemap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate += 1;
            
            // opposite
            now_piece = piece_position[moving_color ^ 1][i];
            if(now_piece != -1)
                heuristic_estimate += redmap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate -= 1;
        }
    }
    return heuristic_estimate;
}

// stochastic use star1
double Board::star1(double alpha, double beta){
    double total = 0;
    double windowLeft = (double)6 * (alpha - MAX_EVAL) + MAX_EVAL;
    double windowRight = (double)6 * (beta - MIN_EVAL) + MIN_EVAL;
    double lowerBound = MIN_EVAL, upperBound = MAX_EVAL;
    for (int dice = 0; dice < 6; dice++) {
        Board newboard = *(this);
        newboard.dice = dice;
        newboard.remain_depth -= 1;
        double score = -newboard.negascout(double_max(windowLeft, MIN_EVAL), double_min(windowRight, MAX_EVAL));
        //printf("nega score = %lf\n", score);
        lowerBound += (score - MIN_EVAL) / (double)6;
        upperBound += (score - MAX_EVAL) / (double)6;
        
        // failed high 
        if (score >= windowRight) return lowerBound;
        //fail low
        if (score <= windowLeft) return upperBound;
        
        total += score;
        windowLeft = double_max(windowLeft, score - MAX_EVAL);
        windowRight = double_min(windowRight, score - MIN_EVAL);
    }
    return (total / (double)6);

}

// deterministic negascout
double Board::negascout(double alpha, double beta){
    // if win or remain depth = 0, return, and time control and some heuristic
    if(check_winner() || remain_depth == 0){
        return simulate();
        //return evaluation();
    }

    // table search
    TTEntry* entry = lookupTT(piece_position, moving_color, dice);
    if(entry){
        if (entry->alpha < entry->m && entry->beta > entry->m) {
            return entry->m;
        }
        else if (entry->beta <= entry->m) {
            // beta cut, lower bound
            alpha = double_max(alpha, entry->m);
            if (alpha >= beta) return alpha;
        }
        else {
            // alpha cut, upper bound
            beta = double_min(beta, entry->m);
            if (beta <= alpha) return beta;
        }
    }

    // negascout
    generate_moves();
    double m = mini_m, n = beta, t;

    // for negascout
    for(int i = 0; i < move_count; i++){
        Board newboard = *(this);
        newboard.move(i);
        //newboard.remain_depth -= 1;
        
        // alpha beta in star1
        t = -newboard.star1(-n, -double_max(alpha, m));
        if (t > m) {
            if (t >= beta || n == beta) {
                m = t;
            } else {
                m = -newboard.star1(-beta, -t); // research
            }
        }

        // cut off
        if(m >= max_m || m >= beta)
            return m;

        // set new window
        n = double_max(alpha, m) + 1;
    }
    insertTT(piece_position, moving_color, dice, alpha, beta, m);
    return m;
}

int Board::decide(){
    initZobristKeys();
    generate_moves();
    if (move_count == 0) {
        // No possible moves
        return -1;
    }

    double best_score = mini_m;
    std::vector<int> best_moves;

    for (int i = 0; i < move_count; i++) {
        Board newboard = *this;
        newboard.move(i);
        newboard.remain_depth -= 1;

        // Evaluate the move using star1 with initial alpha and beta
        double score = -newboard.star1(-MAX_EVAL, MAX_EVAL);
        //printf("%lf\n", score);
        if (score > best_score){
            best_score = score;
            best_moves.clear();
            best_moves.push_back(i);
        }
        else if (score == best_score){
            best_moves.push_back(i);
        }
    }

    // If multiple moves have the best score, choose one randomly
    if (!best_moves.empty()) {
        std::uniform_int_distribution<int> dist(0, best_moves.size() - 1);
        return best_moves[dist(random_num)];
        //return best_moves[0];
    }
    //printf("Error No Moves\n");
    // Fallback (should not reach here)
    return 0;
}

// Very fast and clean simulation!
bool Board::simulate(){
    Board board_copy = *(this);
    // run until game ends.
    while (!board_copy.check_winner())
    {
        board_copy.generate_moves();
        board_copy.move(random_num() % board_copy.move_count);
    }

    // game ends! find winner.
    // Win!
    if (board_copy.moving_color == moving_color)
        return true;
    // Lose!
    else
        return false;
}