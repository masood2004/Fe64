/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file evaluate.h
 * @brief Position evaluation and scoring
 * @author Syed Masood
 * @version 4.0.0
 *
 * The Boa Constrictor style focuses on:
 * - Space control and gradual suffocation
 * - Piece restriction and mobility denial
 * - Solid pawn structures
 * - Patient maneuvering toward favorable endgames
 */

#ifndef FE64_EVALUATE_H
#define FE64_EVALUATE_H

#include "types.h"

// ============================================
//          PIECE VALUES
// ============================================

// Standard piece values (centipawns)
extern int material_values[12];

// Phase values for tapered eval
extern const int phase_values[6];

// ============================================
//          PIECE-SQUARE TABLES
// ============================================

// Middlegame piece-square tables
extern const int pawn_mg_table[64];
extern const int knight_mg_table[64];
extern const int bishop_mg_table[64];
extern const int rook_mg_table[64];
extern const int queen_mg_table[64];
extern const int king_mg_table[64];

// Endgame piece-square tables
extern const int pawn_eg_table[64];
extern const int knight_eg_table[64];
extern const int bishop_eg_table[64];
extern const int rook_eg_table[64];
extern const int queen_eg_table[64];
extern const int king_eg_table[64];

// ============================================
//      BOA CONSTRICTOR STYLE PARAMETERS
// ============================================

// Space control bonus (per controlled square in enemy territory)
extern const int space_bonus_mg;
extern const int space_bonus_eg;

// Piece restriction penalty (per square below average mobility)
extern const int restricted_piece_penalty;

// Pawn chain bonus
extern const int pawn_chain_bonus;

// Outpost bonuses
extern const int knight_outpost_bonus;
extern const int bishop_outpost_bonus;

// King tropism (pieces near enemy king)
extern const int king_tropism_bonus;

// Trade bonus when ahead (to simplify)
extern const int trade_bonus_per_100cp;

// Blockade bonus (blocking passed pawns)
extern const int blockade_bonus;

// ============================================
//          PASSED PAWN BONUSES
// ============================================

extern const int passed_pawn_bonus_mg[8];
extern const int passed_pawn_bonus_eg[8];

// ============================================
//          PAWN STRUCTURE PENALTIES
// ============================================

extern const int doubled_pawn_penalty;
extern const int isolated_pawn_penalty;
extern const int backward_pawn_penalty;

// ============================================
//          ROOK BONUSES
// ============================================

extern const int rook_open_file_bonus;
extern const int rook_semi_open_bonus;
extern const int rook_seventh_rank_bonus;
extern const int connected_rooks_bonus;

// ============================================
//          BISHOP BONUSES
// ============================================

extern const int bishop_pair_bonus;
extern const int bad_bishop_penalty;

// ============================================
//          KING SAFETY
// ============================================

extern const int pawn_shelter_bonus;
extern const int pawn_storm_bonus;
extern const int king_attack_weights[5];

// ============================================
//          MOP-UP EVALUATION
// ============================================

// Manhattan distance from center (for cornering enemy king)
extern const int center_manhattan_distance[64];

// ============================================
//          CONTEMPT FACTOR
// ============================================

extern int contempt;

// ============================================
//          EVALUATION FUNCTIONS
// ============================================

/**
 * @brief Evaluate the current position
 * @return Score in centipawns from side to move's perspective
 */
int evaluate(void);

/**
 * @brief Get material score for a side
 * @param side_to_count WHITE or BLACK
 * @return Material score in centipawns
 */
int get_material_score(int side_to_count);

/**
 * @brief Calculate game phase (0=endgame, 256=opening)
 * @return Phase value
 */
int calculate_phase(void);

/**
 * @brief Calculate space control for a side
 * @param color Side to calculate for
 * @return Number of controlled squares in enemy territory
 */
int calculate_space(int color);

/**
 * @brief Calculate piece restriction score
 * @param color Side whose opponent's pieces are restricted
 * @return Restriction score
 */
int calculate_restriction(int color);

/**
 * @brief Check if square is an outpost
 * @param square The square to check
 * @param color The side owning the piece
 * @return 1 if outpost, 0 otherwise
 */
int is_outpost(int square, int color);

/**
 * @brief Calculate pawn chain strength
 * @param color Side to calculate for
 * @return Chain bonus
 */
int calculate_pawn_chain(int color);

/**
 * @brief Calculate king tropism score
 * @param color Side to calculate for
 * @return Tropism score
 */
int calculate_king_tropism(int color);

/**
 * @brief Check if pawn is passed
 * @param square Pawn's square
 * @param color Pawn's color
 * @return 1 if passed, 0 otherwise
 */
int is_passed_pawn(int square, int color);

/**
 * @brief Mop-up evaluation for winning endgames
 * @param winning_side The side with advantage
 * @param losing_king_sq Losing king's square
 * @param winning_king_sq Winning king's square
 * @return Mop-up score
 */
int mop_up_eval(int winning_side, int losing_king_sq, int winning_king_sq);

/**
 * @brief Count attackers near enemy king
 * @param king_square Enemy king's square
 * @param attacking_side The attacking side
 * @return Number of attackers
 */
int count_king_attackers(int king_square, int attacking_side);

/**
 * @brief Calculate distance between two squares
 * @param sq1 First square
 * @param sq2 Second square
 * @return Chebyshev distance
 */
int square_distance(int sq1, int sq2);

// ============================================
//          NNUE EVALUATION
// ============================================

/**
 * @brief NNUE evaluation (if enabled and loaded)
 * @return Score in centipawns
 */
int evaluate_nnue(void);

// NNUE weights structure
typedef struct
{
    float input_weights[NNUE_INPUT_SIZE][NNUE_HIDDEN1_SIZE];
    float hidden1_bias[NNUE_HIDDEN1_SIZE];
    float hidden1_weights[NNUE_HIDDEN1_SIZE][NNUE_HIDDEN2_SIZE];
    float hidden2_bias[NNUE_HIDDEN2_SIZE];
    float hidden2_weights[NNUE_HIDDEN2_SIZE];
    float output_bias;
    int loaded;
} NNUEWeights;

extern NNUEWeights nnue_weights;
extern int use_nnue_eval;

/**
 * @brief Load NNUE weights from file
 * @param filename Path to weights file
 * @return 1 on success, 0 on failure
 */
int load_nnue(const char *filename);

/**
 * @brief Save NNUE weights to file
 * @param filename Path to save file
 * @return 1 on success, 0 on failure
 */
int save_nnue(const char *filename);

/**
 * @brief Initialize NNUE with random weights
 */
void init_nnue_random(void);

#endif // FE64_EVALUATE_H
