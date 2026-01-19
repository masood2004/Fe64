/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file bitboard.h
 * @brief Bitboard operations and attack table generation
 * @author Syed Masood
 * @version 4.0.0
 */

#ifndef FE64_BITBOARD_H
#define FE64_BITBOARD_H

#include "types.h"

// ============================================
//          ATTACK TABLES
// ============================================

// Pawn attack tables [side][square]
extern U64 pawn_attacks[2][64];

// Knight attack table [square]
extern U64 knight_attacks[64];

// King attack table [square]
extern U64 king_attacks[64];

// Magic numbers for sliding pieces
extern U64 rook_magic_numbers[64];
extern U64 bishop_magic_numbers[64];

// Attack masks for sliding pieces
extern U64 rook_masks[64];
extern U64 bishop_masks[64];

// Sliding piece attack tables
extern U64 rook_attacks_table[64][4096];
extern U64 bishop_attacks_table[64][512];

// ============================================
//          BIT MANIPULATION FUNCTIONS
// ============================================

/**
 * @brief Count the number of set bits in a bitboard
 * @param bitboard The bitboard to count
 * @return Number of set bits
 */
int count_bits(U64 bitboard);

/**
 * @brief Get the index of the least significant set bit
 * @param bitboard The bitboard to check
 * @return Index of the LS1B, or -1 if empty
 */
int get_ls1b_index(U64 bitboard);

// ============================================
//          ATTACK GENERATION FUNCTIONS
// ============================================

/**
 * @brief Generate pawn attack mask
 * @param side The side (WHITE or BLACK)
 * @param square The pawn's square
 * @return Bitboard of attacked squares
 */
U64 mask_pawn_attacks(int side, int square);

/**
 * @brief Generate knight attack mask
 * @param square The knight's square
 * @return Bitboard of attacked squares
 */
U64 mask_knight_attacks(int square);

/**
 * @brief Generate king attack mask
 * @param square The king's square
 * @return Bitboard of attacked squares
 */
U64 mask_king_attacks(int square);

/**
 * @brief Generate bishop attacks on the fly
 * @param square The bishop's square
 * @param block Occupied squares bitboard
 * @return Bitboard of attacked squares
 */
U64 get_bishop_attacks(int square, U64 block);

/**
 * @brief Generate rook attacks on the fly
 * @param square The rook's square
 * @param block Occupied squares bitboard
 * @return Bitboard of attacked squares
 */
U64 get_rook_attacks(int square, U64 block);

/**
 * @brief Generate queen attacks on the fly
 * @param square The queen's square
 * @param block Occupied squares bitboard
 * @return Bitboard of attacked squares
 */
U64 get_queen_attacks(int square, U64 block);

// ============================================
//          MAGIC BITBOARD FUNCTIONS
// ============================================

/**
 * @brief Generate bishop attack mask (without edges)
 * @param square The bishop's square
 * @return Relevant occupancy mask
 */
U64 mask_bishop_attacks_occupancy(int square);

/**
 * @brief Generate rook attack mask (without edges)
 * @param square The rook's square
 * @return Relevant occupancy mask
 */
U64 mask_rook_attacks_occupancy(int square);

/**
 * @brief Generate occupancy variation
 * @param index Variation index
 * @param bits_in_mask Number of bits in mask
 * @param attack_mask The attack mask
 * @return Occupancy variation
 */
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask);

/**
 * @brief Find magic number for a square
 * @param square The square
 * @param relevant_bits Number of relevant bits
 * @param bishop 1 for bishop, 0 for rook
 * @return Magic number
 */
U64 find_magic_number(int square, int relevant_bits, int bishop);

// ============================================
//          INITIALIZATION FUNCTIONS
// ============================================

/**
 * @brief Initialize leaper piece attack tables (pawn, knight, king)
 */
void init_leapers_attacks(void);

/**
 * @brief Initialize sliding piece attack tables (bishop, rook)
 * @param bishop 1 for bishop, 0 for rook
 */
void init_sliders_attacks(int bishop);

/**
 * @brief Initialize all attack tables
 */
void init_attack_tables(void);

// ============================================
//          MAGIC BITBOARD LOOKUP MACROS
// ============================================

/**
 * @def get_bishop_attacks_magic(square, occupancy)
 * @brief Fast bishop attack lookup using magic bitboards
 */
#define get_bishop_attacks_magic(square, occupancy) \
    (bishop_attacks_table[square][((occupancy & bishop_masks[square]) * bishop_magic_numbers[square]) >> (64 - count_bits(bishop_masks[square]))])

/**
 * @def get_rook_attacks_magic(square, occupancy)
 * @brief Fast rook attack lookup using magic bitboards
 */
#define get_rook_attacks_magic(square, occupancy) \
    (rook_attacks_table[square][((occupancy & rook_masks[square]) * rook_magic_numbers[square]) >> (64 - count_bits(rook_masks[square]))])

// ============================================
//          UTILITY FUNCTIONS
// ============================================

/**
 * @brief Print a bitboard to console
 * @param bitboard The bitboard to print
 */
void print_bitboard(U64 bitboard);

#endif // FE64_BITBOARD_H
