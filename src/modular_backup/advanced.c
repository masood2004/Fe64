/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file advanced.c
 * @brief Advanced search implementations for 4000+ ELO
 * @author Syed Masood
 * @version 4.0.0
 */

#include "include/advanced.h"
#include <math.h>
#include <string.h>

// ============================================
//      LATE MOVE PRUNING MARGINS
// ============================================

// [improving][depth]
const int lmp_margin[2][9] = {
    {0, 3, 4, 6, 10, 14, 19, 25, 32}, // not improving
    {0, 5, 7, 11, 17, 24, 32, 41, 51} // improving
};

// ============================================
//      FUTILITY MARGINS
// ============================================

const int futility_margin[7] = {
    0, 100, 160, 220, 280, 340, 400};

const int razoring_margin[4] = {
    0, 125, 250, 375};

// ============================================
//      REDUCTION TABLES
// ============================================

int lmr_reductions[64][64];

void init_reductions(void)
{
    for (int depth = 1; depth < 64; depth++)
    {
        for (int moves = 1; moves < 64; moves++)
        {
            // Base LMR formula
            double r = 0.75 + log((double)depth) * log((double)moves) / 2.25;
            lmr_reductions[depth][moves] = (int)r;

            // Cap at depth-1 to ensure some search
            if (lmr_reductions[depth][moves] > depth - 1)
                lmr_reductions[depth][moves] = depth - 1;
        }
    }
}

// ============================================
//      HISTORY TABLES
// ============================================

int quiet_history[2][64][64];
int capture_history[12][64][6];
int counter_history[12][64][12][64];

// Contempt
int contempt_value = 24;

/**
 * Clear all history tables
 */
void clear_history(void)
{
    memset(quiet_history, 0, sizeof(quiet_history));
    memset(capture_history, 0, sizeof(capture_history));
    memset(counter_history, 0, sizeof(counter_history));
}

/**
 * Age history tables (for new game)
 */
void age_history(void)
{
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 64; j++)
            for (int k = 0; k < 64; k++)
                quiet_history[i][j][k] /= 8;

    for (int i = 0; i < 12; i++)
        for (int j = 0; j < 64; j++)
            for (int k = 0; k < 6; k++)
                capture_history[i][j][k] /= 8;
}
