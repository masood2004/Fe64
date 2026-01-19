/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file board.h
 * @brief Board representation and manipulation
 * @author Syed Masood
 * @version 4.0.0
 */

#ifndef FE64_BOARD_H
#define FE64_BOARD_H

#include "types.h"
#include "bitboard.h"

// ============================================
//          BOARD STATE GLOBALS
// ============================================

// Piece bitboards (12 total: 6 white + 6 black)
extern U64 bitboards[12];

// Occupancy bitboards [0]=white, [1]=black, [2]=both
extern U64 occupancies[3];

// Side to move (WHITE=0, BLACK=1)
extern int side;

// En passant square (no_sq if none)
extern int en_passant;

// Castling rights (4-bit: wk, wq, bk, bq)
extern int castle;

// Fifty-move rule counter
extern int fifty_move;

// Full move counter
extern int full_moves;

// Hash key for current position
extern U64 hash_key;

// Repetition detection table
extern U64 repetition_table[MAX_GAME_MOVES];
extern int repetition_index;

// ============================================
//          CASTLING RIGHTS UPDATE TABLE
// ============================================

extern const int castling_rights[64];

// ============================================
//          BOARD STATE COPY/RESTORE MACROS
// ============================================

/**
 * @def copy_board()
 * @brief Copy current board state for make/unmake move
 */
#define copy_board()                             \
    U64 bitboards_copy[12], occupancies_copy[3]; \
    int side_copy, en_passant_copy, castle_copy; \
    int fifty_copy;                              \
    U64 hash_copy;                               \
    memcpy(bitboards_copy, bitboards, 96);       \
    memcpy(occupancies_copy, occupancies, 24);   \
    side_copy = side;                            \
    en_passant_copy = en_passant;                \
    castle_copy = castle;                        \
    fifty_copy = fifty_move;                     \
    hash_copy = hash_key;

/**
 * @def take_back()
 * @brief Restore board state after make move
 */
#define take_back()                            \
    memcpy(bitboards, bitboards_copy, 96);     \
    memcpy(occupancies, occupancies_copy, 24); \
    side = side_copy;                          \
    en_passant = en_passant_copy;              \
    castle = castle_copy;                      \
    fifty_move = fifty_copy;                   \
    hash_key = hash_copy;

// ============================================
//          BOARD MANIPULATION FUNCTIONS
// ============================================

/**
 * @brief Check if a square is attacked by a given side
 * @param square The square to check
 * @param attacking_side The side doing the attacking
 * @return 1 if attacked, 0 otherwise
 */
int is_square_attacked(int square, int attacking_side);

/**
 * @brief Print the current board state
 */
void print_board(void);

/**
 * @brief Parse a FEN string and setup the board
 * @param fen The FEN string
 */
void parse_fen(const char *fen);

/**
 * @brief Check if current position is a repetition
 * @return 1 if repetition, 0 otherwise
 */
int is_repetition(void);

/**
 * @brief Check if king is in check
 * @return 1 if in check, 0 otherwise
 */
int in_check(void);

/**
 * @brief Get the piece at a given square
 * @param square The square to check
 * @return Piece type or -1 if empty
 */
int get_piece_at_square(int square);

/**
 * @brief Check if position is a draw
 * @return 1 if draw, 0 otherwise
 */
int is_draw(void);

/**
 * @brief Check for insufficient material
 * @return 1 if insufficient, 0 otherwise
 */
int is_insufficient_material(void);

#endif // FE64_BOARD_H
