/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file move.h
 * @brief Move encoding, decoding, and generation
 * @author Syed Masood
 * @version 4.0.0
 */

#ifndef FE64_MOVE_H
#define FE64_MOVE_H

#include "types.h"

// ============================================
//          MOVE ENCODING FORMAT
// ============================================

/**
 * Move Integer Encoding (32-bit):
 *
 * Bits  0-5:   Source Square (6 bits, 0-63)
 * Bits  6-11:  Target Square (6 bits, 0-63)
 * Bits 12-15:  Piece (4 bits)
 * Bits 16-19:  Promoted Piece (4 bits, 0=none)
 * Bit  20:     Capture Flag (1 bit)
 * Bit  21:     Double Push Flag (1 bit)
 * Bit  22:     En Passant Flag (1 bit)
 * Bit  23:     Castling Flag (1 bit)
 */

// ============================================
//          MOVE ENCODING MACRO
// ============================================

/**
 * @def encode_move(source, target, piece, promoted, capture, double, enpassant, castling)
 * @brief Encode a move into a 32-bit integer
 */
#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    ((source) |                                                                                 \
     ((target) << 6) |                                                                          \
     ((piece) << 12) |                                                                          \
     ((promoted) << 16) |                                                                       \
     ((capture) << 20) |                                                                        \
     ((double_push) << 21) |                                                                    \
     ((enpassant) << 22) |                                                                      \
     ((castling) << 23))

// ============================================
//          MOVE DECODING MACROS
// ============================================

#define get_move_source(move) ((move) & 0x3f)
#define get_move_target(move) (((move) & 0xfc0) >> 6)
#define get_move_piece(move) (((move) & 0xf000) >> 12)
#define get_move_promoted(move) (((move) & 0xf0000) >> 16)
#define get_move_capture(move) ((move) & 0x100000)
#define get_move_double(move) ((move) & 0x200000)
#define get_move_enpassant(move) ((move) & 0x400000)
#define get_move_castling(move) ((move) & 0x800000)

// Helper macros for move types
#define is_capture(move) get_move_capture(move)
#define is_promotion(move) get_move_promoted(move)
#define is_quiet(move) (!get_move_capture(move) && !get_move_promoted(move))
#define is_tactical(move) (get_move_capture(move) || get_move_promoted(move))

// ============================================
//          MOVE GENERATION FUNCTIONS
// ============================================

/**
 * @brief Generate all pseudo-legal moves
 * @param move_list Pointer to move list to populate
 */
void generate_moves(MoveList *move_list);

/**
 * @brief Generate only capture moves
 * @param move_list Pointer to move list to populate
 */
void generate_captures(MoveList *move_list);

/**
 * @brief Generate only quiet moves
 * @param move_list Pointer to move list to populate
 */
void generate_quiets(MoveList *move_list);

/**
 * @brief Add a move to the move list
 * @param move_list Pointer to move list
 * @param move The move to add
 */
void add_move(MoveList *move_list, Move move);

// ============================================
//          MOVE MAKING FUNCTIONS
// ============================================

/**
 * @brief Make a move on the board
 * @param move The move to make
 * @param move_flag ALL_MOVES or CAPTURES_ONLY
 * @return 1 if legal, 0 if illegal
 */
int make_move(Move move, int move_flag);

/**
 * @brief Make a null move (pass turn)
 */
void make_null_move(void);

/**
 * @brief Unmake a null move
 */
void unmake_null_move(void);

// ============================================
//          MOVE UTILITY FUNCTIONS
// ============================================

/**
 * @brief Print a move in algebraic notation
 * @param move The move to print
 */
void print_move(Move move);

/**
 * @brief Get move string in algebraic notation
 * @param move The move
 * @param str Buffer to store string (at least 6 chars)
 */
void move_to_string(Move move, char *str);

/**
 * @brief Parse a move string (e.g., "e2e4")
 * @param move_string The move string
 * @return The encoded move, or 0 if invalid
 */
Move parse_move(char *move_string);

// ============================================
//          MVV-LVA TABLE
// ============================================

// Most Valuable Victim - Least Valuable Attacker
// [attacker][victim] -> score
extern int mvv_lva[12][12];

// ============================================
//          STATIC EXCHANGE EVALUATION
// ============================================

/**
 * @brief Static Exchange Evaluation for a capture
 * @param move The capture move
 * @return Estimated exchange value
 */
int see(Move move);

/**
 * @brief Check if SEE value >= threshold
 * @param move The capture move
 * @param threshold Minimum acceptable value
 * @return 1 if SEE >= threshold, 0 otherwise
 */
int see_ge(Move move, int threshold);

// SEE piece values
extern const int see_piece_values[12];

#endif // FE64_MOVE_H
