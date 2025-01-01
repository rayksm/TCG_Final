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
#include <iostream>


// You should use mersenne twister and a random_device seed for the pseudo-random generator
// Call random_num()%num to randomly pick number from 0 to num-1
std::mt19937 random_num(std::random_device{}());

int inserts = 0;
int step_counter = 0, now_max_depth;
double theshold = 25.0;
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
unsigned int ZobristPiece[MAX_COLOR * MAX_DICE][MAP_SIZE * MAP_SIZE + 1];                 // for indexing by piece type (0, 1) * (0, 1, 2, 3, 4, 5)
unsigned int ZobristColor[MAX_COLOR];                  // for indexing colors (0, 1)
unsigned int ZobristDice[MAX_DICE];                    // For dice values (0, 1, 2, 3, 4, 5)

// Initialize Zobrist keys (call once at program start)
void initZobristKeys() {
    // Initialize piece-position keys
    for (int r = 0; r < MAX_COLOR * MAX_DICE; r++) {
        for (int c = 0; c < MAP_SIZE * MAP_SIZE + 1; c++) {
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
    unsigned int h = 0;

    // XOR in piece positions
    for (int r = 0; r < MAX_COLOR; r++) {
        for (int c = 0; c < MAX_DICE; c++) {
            int piece = piece_position[r][c];
            if (piece > -1) {
                h ^= ZobristPiece[r * MAX_DICE + c][piece];
            }
			else{
				h ^= ZobristPiece[r * MAX_DICE + c][MAP_SIZE * MAP_SIZE];
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
void insertTT(int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice, double alpha, double beta, double m, int depth) {
    unsigned int hashKey = computeZobristHash(piece_position, moving_color, dice);
    TTEntry entry;
    
    //entry.alpha = alpha;
    //entry.beta = beta;
    entry.m = m;
    entry.depth = depth;
    
    entry.realkey = hashKey;
    entry.flag = true;
    
	if(transpositionTable[hashKey % MAX_TABLE].realkey != hashKey || transpositionTable[hashKey % MAX_TABLE].depth >= depth)
    	transpositionTable[hashKey % MAX_TABLE] = entry;
	//else if(!transpositionTable[hashKey % MAX_TABLE].flag)
	//	transpositionTable[hashKey % MAX_TABLE] = entry;
	
	inserts++;
}

// Check if a position is in the transposition table
unsigned int lookupTT(int piece_position[MAX_COLOR][MAX_DICE], int moving_color, int dice) {
    unsigned int hashKey = computeZobristHash(piece_position, moving_color, dice);
    if (transpositionTable[hashKey % MAX_TABLE].flag) {
        // collision
        if(transpositionTable[hashKey % MAX_TABLE].realkey != hashKey){
            //printf("Collision\n");
			return MAX_TABLE + 1;
        }
            
        return hashKey % MAX_TABLE;
    }
    return MAX_TABLE + 1;
}

//---------------------------------------------------------------------------------------------------------------
// heuristic evaluation
// for heuristic initialization

/*
int redmap[25] = {
    4,4,4,4,4, 4,3,3,3,3, 4,3,2,2,2, 4,3,2,1,1, 4,3,2,1,0
};

int bluemap[25] = {
    0,1,2,3,4, 1,1,2,3,4, 2,2,2,3,4, 3,3,3,3,4, 4,4,4,4,4
};
*/
int redmap[25] = {
	9, 10, 11, 12, 13,  10, 5, 6, 7, 8,  11, 6, 2, 3, 4,  12, 7, 3, 1, 1,  13, 8, 4, 1, 0
};
int bluemap[25] = {
	0, 1, 4, 8, 13,  1, 1, 3, 7, 12,  4, 3, 2, 6, 11,  8, 7, 6, 5, 10,  13, 12, 11, 10, 9
};

double Board::evaluation(){
    double evaluation_estimate = 0;
    int opponent = (moving_color ^ 1), mypieces = 0;
    for(int pcs = 0; pcs < PIECE_NUM; pcs++) {
      int pos = piece_position[moving_color][pcs];
      if(pos != -1) {
        evaluation_estimate -= redmap[pos];
		mypieces++;
      }

      int op_pos = piece_position[opponent][pcs];
      if(op_pos != -1) {
        evaluation_estimate += bluemap[op_pos];
      }
    }
    // larger means better
    return evaluation_estimate;
}

//---------------------------------------------------------------------------------------------------------------
// stochastic use star1
double Board::star1(double alpha, double beta, int depth){
    double total = 0;
    double windowLeft = (double)6 * (alpha - MAX_EVAL) + MAX_EVAL;
    double windowRight = (double)6 * (beta - MIN_EVAL) + MIN_EVAL;
    double lowerBound = MIN_EVAL, upperBound = MAX_EVAL;

    for (int dice = 0; dice < 6; dice++) {
        Board newboard = *(this);
        newboard.dice = dice;

        double score = newboard.negascout(double_max(windowLeft, MIN_EVAL), double_min(windowRight, MAX_EVAL), depth);
        lowerBound += (score - MIN_EVAL) / (double)6;
        upperBound += (score - MAX_EVAL) / (double)6;
        
        // failed high 
        if (score >= windowRight) return lowerBound;
        //fail low
        if (score <= windowLeft) return upperBound;

        windowLeft += (MAX_EVAL - score);
        windowRight += (MIN_EVAL - score);
        total += score;
    }
    return (total / (double)6);

}

// deterministic negascout
double Board::negascout(double alpha, double beta, int depth){
    // table search
    unsigned int node = lookupTT(piece_position, moving_color, dice);
    // if same board but with higher remaining depth, we will choose it, for higher possibility
	int remain_depth = MAX_TREE_DEPTH - now_max_depth + depth;
    if(node < MAX_TABLE && transpositionTable[node].depth <= remain_depth){
        TTEntry entry = transpositionTable[node];
        return entry.m;       
    }

    // if win or remain depth = 0, time control and heuristics, return simulation value
    if(check_winner() || depth == 0 || time_exceed()){    
        if(check_winner()){
            double m;
            m = MIN_EVAL;
            insertTT(piece_position, moving_color, dice, alpha, beta, m, remain_depth);
            return m;
        }
        // do simulations winnings as evaluations
        else{
            double m;
            m = evaluation();
            insertTT(piece_position, moving_color, dice, alpha, beta, m, remain_depth);
            return m;
        }
    }

    generate_moves();
    double m = MIN_EVAL, n = beta, t;

    // for negascout
    for(int i = 0; i < move_count; i++){
        Board newboard = *(this);
        newboard.move(i);
        
        // alpha beta in star1
        t = -newboard.star1(-n, -double_max(alpha, m), depth - 1);
        if (t > m) {
            if (t >= beta || n == beta) {
                m = t;
            } else {
                m = -newboard.star1(-beta, -t, depth - 1); // research
            }
        }

        // cut off
        if(m >= MAX_EVAL || m >= beta){
            insertTT(piece_position, moving_color, dice, alpha, beta, m, remain_depth);
            return m;
        }

        // set new window
        n = double_max(alpha, m) + 1;
    }

    // insert node into table
    insertTT(piece_position, moving_color, dice, alpha, beta, m, remain_depth);
    return m;
}
//---------------------------------------------------------------------------------------------------------------
// deciding function
int Board::decide(){
    generate_moves();
    
    // No possible moves
    if (move_count == 0) {
        return -1;
    }

    // first step init hash
    step_counter++;
	printf("%d\n", inserts);
    if(step_counter == 1) initZobristKeys();

    double best_score = MIN_EVAL;
    std::vector<int> best_moves;

    //iterative deepening
    for(int tree_depth = 5; tree_depth <= MAX_TREE_DEPTH; tree_depth++){
        //printf("%d\n", inserts);
        // if time is exceed
        if(tree_depth > 5 && time_exceed()){
            break;
        }
        now_max_depth = tree_depth;
       	//printf("%d %d > ", tree_depth, move_count); 
        for (int i = 0; i < move_count; i++) {
            Board newboard = *this;
            newboard.move(i);

            // Evaluate the move using star1 with initial alpha and beta
            double score;
           // if(tree_depth > 5)
           //     score = -newboard.star1(double_max(MIN_EVAL, -std::abs(best_score)), double_min(MAX_EVAL, best_score + std::abs(best_score * 2)), tree_depth);
           // else
                score = -newboard.star1(MIN_EVAL, MAX_EVAL, tree_depth);
			
			//printf("%lf ", score);
            if (score > best_score){
                best_score = score;
                best_moves.clear();
                best_moves.push_back(i);
				//printf("Update\n");
            }
            else if (score == best_score){
                best_moves.push_back(i);
            }
			//printf("\n");
        }
    }

    // If multiple moves have the best score, choose one randomly
    if (!best_moves.empty()) {
        //printf("Search!\n");
		//std::uniform_int_distribution<int> dist(0, best_moves.size() - 1);
        //return best_moves[dist(random_num)];
		return best_moves[0]; 
    }

    return 0;
}

//---------------------------------------------------------------------------------------------------------------

// calculate remain time
void Board::cal_remain_time(double total_time){
    clock_gettime(CLOCK_REALTIME, &start);
    total_remain_time = total_time / std::max(10 - step_counter, 3);
}

// calculate if time exceed
bool Board::time_exceed(){
    clock_gettime(CLOCK_REALTIME, &end);
    double now_time = (double)((end.tv_sec + end.tv_nsec * 1e-9) - (double)(start.tv_sec + start.tv_nsec * 1e-9));
    
    if(now_time + 1e-4 > total_remain_time) return 1;
    else return 0;
}
