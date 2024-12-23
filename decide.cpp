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


// You should use mersenne twister and a random_device seed for the pseudo-random generator
// Call random_num()%num to randomly pick number from 0 to num-1
std::mt19937 random_num(std::random_device{}());

int step_counter = 0, now_max_depth;
double theshold = 2.0;
struct timespec start, end;

double double_max(double a, double b){
    return a > b ? a : b;
}
double double_min(double a, double b){
    return a < b ? a : b;
}
//---------------------------------------------------------------------------------------------------------------
// Global Zobrist tables
TTEntry transpositionTable[MAX_TABLE];
unsigned int ZobristPiece[MAX_COLOR * MAX_DICE][MAP_SIZE * MAP_SIZE];                 // for indexing by piece type (0, 1) * (0, 1, 2, 3, 4, 5)
unsigned int ZobristColor[MAX_COLOR];                  // +1 for indexing colors (0, 1)
unsigned int ZobristDice[MAX_DICE];                    // For dice values (0, 1, 2, 3, 4, 5)
//std::unordered_map<uint64_t, TTEntry> transpositionTable;

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
unsigned int computeZobristHash(int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice) {
    unsigned int h = 0ULL;

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

    return h % MAX_TABLE;
}

// Insert an entry into the transposition table
void insertTT(int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice, double alpha, double beta, double m, int depth) {
    unsigned int hashKey = computeZobristHash(piece_position, moving_color, dice);
    TTEntry entry;
    
    entry.alpha = alpha;
    entry.beta = beta;
    entry.m = m;
    entry.depth = depth;
    entry.flag = true;

    transpositionTable[hashKey] = entry;
}

// Check if a position is in the transposition table
unsigned int lookupTT(int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice) {
    unsigned int hashKey = computeZobristHash(piece_position, moving_color, dice);
    if (transpositionTable[hashKey].flag) {
        return hashKey;
    }
    return MAX_TABLE + 1;
}
//---------------------------------------------------------------------------------------------------------------
// heuristic evaluation
int bluemap[5][5] = {
    { 0,  1,  4,  8, 13},
    { 1,  1,  3,  7, 12},
    { 4,  3,  2,  6, 11},
    { 8,  7,  6,  5, 10},
    {13, 12, 11, 10,  9},
};
int redmap[5][5] = {
    { 9, 10, 11, 12, 13},
    {10,  5,  6,  7,  8},
    {11,  6,  2,  3,  4},
    {12,  7,  3,  1,  1},
    {13,  8,  4,  1,  0},
};

// for heuristic evaluation
void Board::evaluation(){
    // blue end move, and red start to move
    int heuristic_estimate = 0;
    if(moving_color == RED){    
        //heuristic_estimate += (PIECE_NUM - __builtin_popcount(piece_bits[RED]));
        //heuristic_estimate += (__builtin_popcount(piece_bits[BLUE]));     
        
        
        int now_piece;
        for(int i = 0; i < PIECE_NUM; i++){
            // player
            now_piece = piece_position[moving_color][i];
            //if(now_piece == -1) heuristic_estimate += 1;
            if(now_piece != -1)
                heuristic_estimate -= redmap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate += 8;
            
            // opposite
            now_piece = piece_position[moving_color ^ 1][i];
            //if(now_piece != -1) heuristic_estimate += 1;
            if(now_piece != -1)
                heuristic_estimate += bluemap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate -= 8;
        }
        
    }
    else{
        //heuristic_estimate += (PIECE_NUM - __builtin_popcount(piece_bits[BLUE]));
        //heuristic_estimate += (__builtin_popcount(piece_bits[RED]));
        
        
        int now_piece;
        for(int i = 0; i < PIECE_NUM; i++){
            // player
            now_piece = piece_position[moving_color][i];
            //if(now_piece == -1) heuristic_estimate += 1;
            if(now_piece != -1)
                heuristic_estimate -= bluemap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate += 8;
            
            // opposite
            now_piece = piece_position[moving_color ^ 1][i];
            //if(now_piece -= -1) heuristic_estimate += 1;
            if(now_piece != -1)
                heuristic_estimate += redmap[now_piece / 5][now_piece % 5];
            else
                heuristic_estimate -= 8;
        }
        
    }
    // larger means better
    heuristic_depth -= heuristic_estimate;
    //heuristic_depth = std::abs(heuristic_estimate);
}

// stochastic use star1
double Board::star1(double alpha, double beta, int depth){
    double total = 0;
    double windowLeft = (double)6 * (alpha - MAX_EVAL) + MAX_EVAL;
    double windowRight = (double)6 * (beta - MIN_EVAL) + MIN_EVAL;
    double lowerBound = MIN_EVAL, upperBound = MAX_EVAL;
    double pre_score = MIN_EVAL;
    int remain_count = 1;
    for (int dice = 0; dice < 6; dice++) {
        Board newboard = *(this);
        newboard.dice = dice;

        //newboard.tree_depth -= 1;
        //double score = newboard.negascout(double_max(windowLeft, MIN_EVAL), double_min(windowRight, MAX_EVAL), depth - 1);
        double score = -newboard.negascout(-double_min(windowRight, MAX_EVAL), -double_max(windowLeft, MIN_EVAL), depth - 1);
        lowerBound += (score - MIN_EVAL) / (double)6;
        upperBound += (score - MAX_EVAL) / (double)6;
        
        // failed high 
        if (score >= windowRight) return lowerBound;
        //fail low
        if (score <= windowLeft) return upperBound;

        windowLeft = double_max(windowLeft, score - MAX_EVAL);
        windowRight = double_min(windowRight, score - MIN_EVAL);
        
        //if(!((newboard.piece_bits[newboard.moving_color] >> dice) & 1)){
        //    remain_count += 1;
        //    continue;
        //}
        total += remain_count * double_max(score, pre_score);
        pre_score = score;
        remain_count = 1;
    }
    return (total / (double)6);

}

// deterministic negascout
double Board::negascout(double alpha, double beta, int depth){
    // if win or remain depth = 0, return, and time control and some heuristic
    //printf("%d\n", heuristic_depth);
    if(check_winner() || depth == 0 || heuristic_depth >= 100|| time_exceed()){
    //if(check_winner() || tree_depth == 0){
        //return simulate();
        //return evaluation();
        //printf("%d > %d, %d\n", heuristic_depth + std::max(MAX_TREE_DEPTH - depth, 0), MAX_TREE_DEPTH + 15, depth);
        int count_sim = 0;
        for(int i = 0; i < MAX_SIM; i++)
            count_sim += simulate();
        return depth % 2 == ((now_max_depth % 2)) ? -count_sim : count_sim;
        //return count_sim;
        //return depth % 2 == ((now_max_depth % 2) ^ 1) ? heuristic_depth : -heuristic_depth;
        //return heuristic_depth;
    }
    //printf("heuristic_depth = %d\n", heuristic_depth);

    // table search
    uint64_t node = lookupTT(piece_position, moving_color, dice);
    if(node < MAX_TABLE && transpositionTable[node].depth >= depth){
        TTEntry entry = transpositionTable[node];
        if (entry.alpha < entry.m && entry.beta > entry.m) {
            return entry.m;
        }
        else if (entry.beta <= entry.m) {
            // beta cut, lower bound
            alpha = double_max(alpha, entry.m);
            if (alpha >= beta) return alpha;
        }
        else {
            // alpha cut, upper bound
            beta = double_min(beta, entry.m);
            if (beta <= alpha) return beta;
        }
    }

    // negascout
    generate_moves();
    double m = MIN_EVAL, n = beta, t;

    // for negascout
    for(int i = 0; i < move_count; i++){
        Board newboard = *(this);
        newboard.move(i);
        newboard.evaluation();
        //newboard.tree_depth -= 1;
        
        // alpha beta in star1
        t = -newboard.star1(-n, -double_max(alpha, m), depth);
        if (t > m) {
            if (t >= beta || n == beta || depth < 2) {
                m = t;
            } else {
                m = -newboard.star1(-beta, -t, depth); // research
            }
        }

        // cut off
        if(m >= MAX_EVAL || m >= beta)
            return m;

        // set new window
        n = double_max(alpha, m) + 1;
    }
    insertTT(piece_position, moving_color, dice, alpha, beta, m, depth);
    return m;
}

int Board::decide(){
    initZobristKeys();
    generate_moves();
    // No possible moves
    if (move_count == 0) {
        return -1;
    }

    // first step greedy
    step_counter++;
    if(step_counter == 1) return greedymoves();

    double best_score = MIN_EVAL;
    std::vector<int> best_moves;
    for(int tree_depth = 1; tree_depth <= MAX_TREE_DEPTH; tree_depth+=2){
        if(tree_depth > 1 && time_exceed()){
            if(tree_depth < MAX_TREE_DEPTH / 2) return greedymoves();
            break;
            //printf("Break\n");
        }
        //else printf("%d %d %d\n", step_counter, tree_depth, time_exceed());
        now_max_depth = tree_depth;

        for (int i = 0; i < move_count; i++) {
            //printf("%d, %d, i = %d\n", step_counter, tree_depth, i);
            Board newboard = *this;
            newboard.move(i);
            newboard.evaluation();
            //newboard.tree_depth -= 1;
            //newboard.remain_depth -= 1;

            // Evaluate the move using star1 with initial alpha and beta
            double score;
            if(tree_depth > 1)
                score = -newboard.star1(best_score - theshold, best_score + theshold, tree_depth);
            else
                score = -newboard.star1(MIN_EVAL, MAX_EVAL, tree_depth);

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
    }

    // If multiple moves have the best score, choose one randomly
    if (!best_moves.empty()) {
        std::uniform_int_distribution<int> dist(0, best_moves.size() - 1);
        return best_moves[dist(random_num)];
        //return best_moves[0];
    }
    //printf("Error No Moves\n");
    // Fallback (should not reach here)
    return -1;
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

// calculate remain time
void Board::cal_remain_time(double total_time){
    //clock_gettime(CLOCK_REALTIME, &end);
    clock_gettime(CLOCK_REALTIME, &start);
    total_remain_time = total_time / std::max(15 - step_counter, 3);
}

bool Board::time_exceed(){
    clock_gettime(CLOCK_REALTIME, &end);
    double now_time = (double)((end.tv_sec + end.tv_nsec * 1e-9) - (double)(start.tv_sec + start.tv_nsec * 1e-9));
    
    if(now_time + 1e-4 > total_remain_time) return 1;
    else return 0;
}

int Board::greedymoves(){
    int start = moves[0][0]; 
    for(int i = 0; i < move_count; i++){
        int dest = moves[i][1];
        if(start == 0 || start == 5 || start == 24 || start == 19){
            if(std::abs(start - dest) == 5) return i;    
        }
        else if(start == 1 || start == 23){
            if(std::abs(start - dest) == 1) return i;
        }
        else{
            if(std::abs(start - dest) == 6) return i;
        }
    }
    return 0;
}
