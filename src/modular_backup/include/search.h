/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file search.h
 * @brief Search algorithms and move ordering
 * @author Syed Masood
 * @version 4.0.0
 *
 * Search features:
 * - Iterative deepening with aspiration windows
 * - Principal Variation Search (PVS)
 * - Null move pruning
 * - Late Move Reductions (LMR)
 * - Late Move Pruning (LMP)
 * - Futility pruning
 * - Razoring
 * - SEE pruning
 * - Killer moves, history heuristic, counter-moves
 * - Pondering support (fixed and improved)
 */

#ifndef FE64_SEARCH_H
#define FE64_SEARCH_H

#include "types.h"
#include "move.h"

// ============================================
//          SEARCH TABLES
// ============================================

// Principal Variation table
extern int pv_length[MAX_PLY];
extern Move pv_table[MAX_PLY][MAX_PLY];

// Killer moves [2][ply] - two killers per ply
extern Move killer_moves[2][MAX_PLY];

// History heuristic [piece][to_square]
extern int history_moves[12][64];

// Counter-move heuristic [piece][to_square] = counter_move
extern Move counter_moves[12][64];

// Butterfly history [side][from][to]
extern int butterfly_history[2][64][64];

// Capture history [piece][to][captured_piece_type]
extern int capture_history[12][64][6];

// Last move made at each ply (for counter-move)
extern Move last_move_made[MAX_PLY];

// Late Move Reduction table
extern int lmr_table[MAX_PLY][MAX_MOVES];

// ============================================
//          SEARCH PARAMETERS
// ============================================

// Late Move Pruning margins by depth
extern const int lmp_margins[8];

// Futility pruning margins by depth
extern const int futility_margins[7];

// Razoring margins by depth
extern const int razor_margins[4];

// Reverse futility pruning margins
extern const int rfp_margins[7];

// Singular extension margin
extern const int singular_margin;

// History maximum value (for clamping)
extern const int history_max;

// ============================================
//          SEARCH STATE
// ============================================

// Global best move found
extern Move best_move;

// Node counter
extern long long nodes;

// Search depth
extern int search_depth;

// ============================================
//          TIME MANAGEMENT
// ============================================

// Time variables
extern long long start_time;
extern long long stop_time;
extern long long time_for_move;
extern int times_up;

/**
 * @brief Get current time in milliseconds
 * @return Time in milliseconds
 */
long long get_time_ms(void);

/**
 * @brief Check if time is up and handle communication
 */
void communicate(void);

/**
 * @brief Check if input is waiting on stdin
 * @return 1 if input available, 0 otherwise
 */
int input_waiting(void);

// ============================================
//          PONDERING SUPPORT
// ============================================

// Pondering state
extern volatile int pondering;
extern volatile int stop_pondering;
extern Move ponder_move;
extern int ponder_hit;
extern long long ponder_time_for_move;

// Thread synchronization
#ifndef _WIN32
extern pthread_mutex_t search_mutex;
#endif

// ============================================
//          SEARCH FUNCTIONS
// ============================================

/**
 * @brief Initialize LMR table
 */
void init_lmr_table(void);

/**
 * @brief Initialize search tables
 */
void init_search(void);

/**
 * @brief Clear search history (for new game)
 */
void clear_search_history(void);

/**
 * @brief Age history tables (divide by 2)
 */
void age_history(void);

/**
 * @brief Score a move for ordering
 * @param move The move to score
 * @param pv_move The PV move from hash table
 * @param ply Current ply
 * @return Score for move ordering
 */
int score_move(Move move, Move pv_move, int ply);

/**
 * @brief Quiescence search
 * @param alpha Alpha bound
 * @param beta Beta bound
 * @return Quiescence score
 */
int quiescence(int alpha, int beta);

/**
 * @brief Main negamax search with alpha-beta
 * @param alpha Alpha bound
 * @param beta Beta bound
 * @param depth Remaining depth
 * @param ply Ply from root
 * @return Search score
 */
int negamax(int alpha, int beta, int depth, int ply);

/**
 * @brief Root search with iterative deepening
 * @param info Search info structure
 */
void search_position(SearchInfo *info);

/**
 * @brief Start pondering on expected move
 * @param expected_move The move we expect opponent to play
 */
void start_pondering(Move expected_move);

/**
 * @brief Stop pondering
 */
void stop_pondering_search(void);

// ============================================
//          PERFT TESTING
// ============================================

/**
 * @brief Perft driver function
 * @param depth Depth to search
 */
void perft_driver(int depth);

/**
 * @brief Perft test with move breakdown
 * @param depth Depth to search
 */
void perft_test(int depth);

#endif // FE64_SEARCH_H
