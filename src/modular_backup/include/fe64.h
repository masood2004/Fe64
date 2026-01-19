/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file fe64.h
 * @brief Main header file - includes all engine components
 * @author Syed Masood
 * @version 4.0.0
 *
 * Fe64: A world-class chess engine featuring:
 *
 * UNIQUE PLAYING STYLE - "The Boa Constrictor":
 * - Gradually restricts opponent's piece mobility
 * - Controls space and slowly suffocates
 * - Favors solid pawn structures
 * - Patient maneuvering toward favorable endgames
 * - Excels at converting small advantages
 *
 * TECHNICAL FEATURES:
 * - Magic bitboard move generation
 * - Advanced search with LMR, NMP, SEE pruning
 * - Tapered evaluation with king safety
 * - Polyglot opening book support
 * - Optional NNUE evaluation
 * - Full UCI protocol compliance
 * - Proper pondering support
 */

#ifndef FE64_H
#define FE64_H

// Core type definitions and constants
#include "types.h"

// Bitboard operations and attack generation
#include "bitboard.h"

// Board representation and manipulation
#include "board.h"

// Move encoding and generation
#include "move.h"

// Zobrist hashing and transposition table
#include "hash.h"

// Position evaluation
#include "evaluate.h"

// Search algorithms
#include "search.h"

// Opening book support
#include "book.h"

// UCI protocol
#include "uci.h"

// ============================================
//          ENGINE INITIALIZATION
// ============================================

/**
 * @brief Initialize all engine components
 *
 * This function must be called before any other engine functions.
 * It initializes:
 * - Attack tables (leaper and slider pieces)
 * - Zobrist hash keys
 * - LMR reduction tables
 * - Transposition table
 * - Search history tables
 */
void init_engine(void);

/**
 * @brief Cleanup and free engine resources
 */
void cleanup_engine(void);

// ============================================
//          VERSION INFORMATION
// ============================================

/**
 * @brief Get engine version string
 * @return Version string (e.g., "4.0.0")
 */
const char *get_version(void);

/**
 * @brief Get engine name
 * @return Engine name ("Fe64")
 */
const char *get_engine_name(void);

/**
 * @brief Get author name
 * @return Author name
 */
const char *get_author(void);

/**
 * @brief Get playing style description
 * @return Style description
 */
const char *get_style(void);

/**
 * @brief Print engine banner
 */
void print_banner(void);

#endif // FE64_H
