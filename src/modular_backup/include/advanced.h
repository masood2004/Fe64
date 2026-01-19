/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file advanced.h
 * @brief Advanced search and evaluation features for 4000+ ELO
 * @author Syed Masood
 * @version 4.0.0
 */

#ifndef FE64_ADVANCED_H
#define FE64_ADVANCED_H

#include "types.h"

// ============================================
//      SINGULAR EXTENSIONS
// ============================================

#define SE_DEPTH_LIMIT 8
#define SE_MARGIN 50

// ============================================
//      LATE MOVE PRUNING (LMP)
// ============================================

extern const int lmp_margin[2][9];

// ============================================
//      FUTILITY PRUNING MARGINS
// ============================================

extern const int futility_margin[7];
extern const int razoring_margin[4];

// ============================================
//      REDUCTION TABLES
// ============================================

// Late Move Reductions
extern int lmr_reductions[64][64];

// Initialize reduction tables
void init_reductions(void);

// ============================================
//      MOVE ORDERING CONSTANTS
// ============================================

#define HASH_MOVE_SCORE 1000000
#define PROMOTION_SCORE 900000
#define GOOD_CAPTURE_BASE 800000
#define KILLER1_SCORE 700000
#define KILLER2_SCORE 600000
#define COUNTER_SCORE 500000
#define BAD_CAPTURE_BASE 100000

// ============================================
//      HISTORY & STATISTICS
// ============================================

// History heuristic with butterfly boards
extern int quiet_history[2][64][64];        // [side][from][to]
extern int capture_history[12][64][6];      // [piece][to][captured_type]
extern int counter_history[12][64][12][64]; // [prev_piece][prev_to][piece][to]

// History bonus calculation
static inline int history_bonus(int depth)
{
    return depth > 13 ? 32 : 16 * depth * depth + 32 * depth;
}

// Update history
static inline void update_history(int *entry, int bonus)
{
    *entry += bonus - (*entry * abs(bonus) / 16384);
}

// ============================================
//      CONTEMPT SETTINGS
// ============================================

extern int contempt_value;

// ============================================
//      EVAL ADJUSTMENTS
// ============================================

// Piece-specific adjustments
#define TEMPO_BONUS 20
#define BISHOP_PAIR_BONUS 50
#define ROOK_OPEN_FILE 20
#define ROOK_SEMI_OPEN 10
#define ROOK_SEVENTH_RANK 20

// King safety constants
#define KING_ATTACK_WEIGHT_KNIGHT 2
#define KING_ATTACK_WEIGHT_BISHOP 2
#define KING_ATTACK_WEIGHT_ROOK 3
#define KING_ATTACK_WEIGHT_QUEEN 5

// ============================================
//      SEARCH EXTENSIONS
// ============================================

#define CHECK_EXTENSION 1
#define PASSED_PAWN_EXTENSION 1
#define SINGULAR_EXTENSION 1
#define RECAPTURE_EXTENSION 1

// ============================================
//      PRUNING THRESHOLDS
// ============================================

// Null move pruning
#define NMP_BASE_REDUCTION 3
#define NMP_DIVISOR 6
#define NMP_EVAL_THRESHOLD 0

// Reverse futility pruning
#define RFP_DEPTH 8
#define RFP_MARGIN 100

// SEE pruning thresholds
#define SEE_QUIET_MARGIN -80
#define SEE_NOISY_MARGIN -20

// ============================================
//      ASPIRATION WINDOWS
// ============================================

#define ASP_WINDOW 12
#define ASP_MIN_DEPTH 5

// ============================================
//      INTERNAL ITERATIVE DEEPENING
// ============================================

#define IID_DEPTH 4
#define IID_REDUCTION 4

// ============================================
//      MULTI-CUT PRUNING
// ============================================

#define MC_DEPTH 8
#define MC_MOVES 6
#define MC_CUTOFFS 3
#define MC_REDUCTION 4

// ============================================
//      PROBCUT
// ============================================

#define PROBCUT_DEPTH 5
#define PROBCUT_MARGIN 200

// ============================================
//      SYZYGY TABLEBASES (stub)
// ============================================

// Tablebase support would go here
// Requires external Fathom library

#endif // FE64_ADVANCED_H
