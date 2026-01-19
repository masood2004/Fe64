/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file hash.h
 * @brief Zobrist hashing and transposition table
 * @author Syed Masood
 * @version 4.0.0
 */

#ifndef FE64_HASH_H
#define FE64_HASH_H

#include "types.h"

// ============================================
//          ZOBRIST KEYS
// ============================================

// Piece keys [piece][square]
extern U64 piece_keys[12][64];

// Side to move key
extern U64 side_key;

// Castling keys [16 combinations]
extern U64 castle_keys[16];

// En passant keys [square]
extern U64 enpassant_keys[64];

// ============================================
//          TRANSPOSITION TABLE
// ============================================

// Default hash table size (4 million entries ≈ 100MB)
#define TT_DEFAULT_SIZE 0x400000

// Hash table
extern TTEntry *transposition_table;
extern int tt_size;
extern int tt_generation;

// ============================================
//          HASH FUNCTIONS
// ============================================

/**
 * @brief Initialize Zobrist hash keys
 */
void init_hash_keys(void);

/**
 * @brief Generate hash key from scratch
 * @return Complete hash key for current position
 */
U64 generate_hash_key(void);

/**
 * @brief Initialize transposition table
 * @param size_mb Size in megabytes
 */
void init_tt(int size_mb);

/**
 * @brief Clear transposition table
 */
void clear_tt(void);

/**
 * @brief Free transposition table memory
 */
void free_tt(void);

/**
 * @brief Probe transposition table
 * @param alpha Current alpha
 * @param beta Current beta
 * @param depth Current depth
 * @param ply Current ply from root
 * @param tt_move Pointer to store hash move
 * @return Score if valid, -INF-1 otherwise
 */
int probe_tt(int alpha, int beta, int depth, int ply, Move *tt_move);

/**
 * @brief Store position in transposition table
 * @param depth Search depth
 * @param value Score
 * @param flags Node type (EXACT, ALPHA, BETA)
 * @param move Best move
 * @param ply Ply from root
 */
void store_tt(int depth, int value, int flags, Move move, int ply);

/**
 * @brief Get best move from hash table
 * @return Best move if found, 0 otherwise
 */
Move get_tt_move(void);

/**
 * @brief Prefetch hash entry for upcoming position
 * @param key Position hash key
 */
void prefetch_tt(U64 key);

/**
 * @brief Get hash table usage percentage
 * @return Permille (0-1000) usage
 */
int get_tt_usage(void);

/**
 * @brief Age the transposition table
 */
void age_tt(void);

// ============================================
//          RANDOM NUMBER GENERATION
// ============================================

/**
 * @brief Get 32-bit pseudo-random number
 * @return Random number
 */
unsigned int get_random_U32(void);

/**
 * @brief Get 64-bit pseudo-random number
 * @return Random number
 */
U64 get_random_U64(void);

/**
 * @brief Generate sparse magic candidate
 * @return Magic number candidate
 */
U64 generate_magic_candidate(void);

// Random state
extern unsigned int random_state;

#endif // FE64_HASH_H
