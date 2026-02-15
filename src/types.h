// ============================================ \\
//       FE64 CHESS ENGINE - TYPES HEADER       \\
//    "The Boa Constrictor" - Slow Death Style  \\
// ============================================ \\

#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>

// Platform-specific includes for non-blocking input
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#endif

// ============================================ \\
//              CORE DEFINITIONS                \\
// ============================================ \\

// The 64-bit integer is the heart of the engine.
#define U64 unsigned long long

// Search Constants
#define INF 50000
#define MATE 49000
#define MAX_PLY 128
#define MAX_GAME_MOVES 2048

// ============================================ \\
//              ENUMERATIONS                    \\
// ============================================ \\

// Square mapping (Big Endian: a8 = 0, h1 = 63)
enum
{
    a8,
    b8,
    c8,
    d8,
    e8,
    f8,
    g8,
    h8,
    a7,
    b7,
    c7,
    d7,
    e7,
    f7,
    g7,
    h7,
    a6,
    b6,
    c6,
    d6,
    e6,
    f6,
    g6,
    h6,
    a5,
    b5,
    c5,
    d5,
    e5,
    f5,
    g5,
    h5,
    a4,
    b4,
    c4,
    d4,
    e4,
    f4,
    g4,
    h4,
    a3,
    b3,
    c3,
    d3,
    e3,
    f3,
    g3,
    h3,
    a2,
    b2,
    c2,
    d2,
    e2,
    f2,
    g2,
    h2,
    a1,
    b1,
    c1,
    d1,
    e1,
    f1,
    g1,
    h1,
    no_sq
};

// Side colors
enum
{
    white,
    black,
    both
};

// Piece encoding
enum
{
    P,
    N,
    B,
    R,
    Q,
    K,
    p,
    n,
    b,
    r,
    q,
    k
};

// Castling rights
enum
{
    wk = 1,
    wq = 2,
    bk = 4,
    bq = 8
};

// Move types
enum
{
    all_moves,
    only_captures
};

// TT flags
#define HASH_EXACT 0
#define HASH_ALPHA 1
#define HASH_BETA 2

// ============================================ \\
//              DATA STRUCTURES                 \\
// ============================================ \\

// Move List Structure
typedef struct
{
    int moves[256];
    int count;
} moves;

// Transposition Table Entry
typedef struct
{
    U64 key;
    int depth;
    int flags;
    int value;
    int best_move;
} tt_entry;

// ============================================ \\
//              BITWISE MACROS                  \\
// ============================================ \\

#define get_bit(bitboard, square) (bitboard & (1ULL << square))
#define set_bit(bitboard, square) (bitboard |= (1ULL << square))
#define pop_bit(bitboard, square) (get_bit(bitboard, square) ? (bitboard ^= (1ULL << square)) : 0)

// ============================================ \\
//           MOVE ENCODING MACROS               \\
// ============================================ \\

/*
  Move Integer Encoding (32-bit int):
  0000 0000 0000 0000 0011 1111    Source Square (6 bits)
  0000 0000 0000 1111 1100 0000    Target Square (6 bits)
  0000 0000 1111 0000 0000 0000    Piece (4 bits)
  0000 1111 0000 0000 0000 0000    Promoted Piece (4 bits)
  0001 0000 0000 0000 0000 0000    Capture Flag (1 bit)
  0010 0000 0000 0000 0000 0000    Double Push Flag (1 bit)
  0100 0000 0000 0000 0000 0000    En Passant Flag (1 bit)
  1000 0000 0000 0000 0000 0000    Castling Flag (1 bit)
*/

#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    (source) |                                                                                  \
        (target << 6) |                                                                         \
        (piece << 12) |                                                                         \
        (promoted << 16) |                                                                      \
        (capture << 20) |                                                                       \
        (double_push << 21) |                                                                   \
        (enpassant << 22) |                                                                     \
        (castling << 23)

#define get_move_source(move) (move & 0x3f)
#define get_move_target(move) ((move & 0xfc0) >> 6)
#define get_move_piece(move) ((move & 0xf000) >> 12)
#define get_move_promoted(move) ((move & 0xf0000) >> 16)
#define get_move_capture(move) (move & 0x100000)
#define get_move_double(move) (move & 0x200000)
#define get_move_enpassant(move) (move & 0x400000)
#define get_move_castling(move) (move & 0x800000)

// ============================================ \\
//           BOARD STATE MACROS                 \\
// ============================================ \\

#define copy_board()                             \
    U64 bitboards_copy[12], occupancies_copy[3]; \
    int side_copy, en_passant_copy, castle_copy; \
    U64 hash_key_copy;                           \
    memcpy(bitboards_copy, bitboards, 96);       \
    memcpy(occupancies_copy, occupancies, 24);   \
    side_copy = side;                            \
    en_passant_copy = en_passant;                \
    castle_copy = castle;                        \
    hash_key_copy = hash_key;

#define take_back()                            \
    memcpy(bitboards, bitboards_copy, 96);     \
    memcpy(occupancies, occupancies_copy, 24); \
    side = side_copy;                          \
    en_passant = en_passant_copy;              \
    castle = castle_copy;                      \
    hash_key = hash_key_copy;

// ============================================ \\
//           GLOBAL EXTERN DECLARATIONS         \\
// ============================================ \\

// Attack Tables
extern U64 pawn_attacks[2][64];
extern U64 knight_attacks[64];
extern U64 king_attacks[64];
extern U64 bishop_masks[64];
extern U64 rook_masks[64];
extern U64 bishop_attacks_table[64][512];
extern U64 rook_attacks_table[64][4096];
extern U64 bishop_magic_numbers[64];
extern U64 rook_magic_numbers[64];

// Board State
extern U64 bitboards[12];
extern U64 occupancies[3];
extern int side;
extern int en_passant;
extern int castle;

// Zobrist Hashing
extern U64 piece_keys[12][64];
extern U64 side_key;
extern U64 castle_keys[16];
extern U64 enpassant_keys[64];
extern U64 hash_key;

// Repetition Detection
extern U64 repetition_table[MAX_GAME_MOVES];
extern int repetition_index;

// Transposition Table
#define TT_DEFAULT_SIZE 0x400000
extern tt_entry *transposition_table;
extern U64 tt_num_entries;
extern int tt_generation;
extern void init_tt(int mb);
extern void resize_tt(int mb);

// Search State
extern int best_move;
extern long long nodes;
extern int pv_length[MAX_PLY];
extern int pv_table[MAX_PLY][MAX_PLY];
extern int killer_moves[2][64];
extern int history_moves[12][64];
extern int counter_moves[12][64];
extern int butterfly_history[2][64][64];
extern int capture_history[12][64][6];
extern int last_move_made[MAX_PLY];
extern int lmr_table[MAX_PLY][64];
extern int static_eval_stack[MAX_PLY];
extern int excluded_move[MAX_PLY];

// Timing
extern long long start_time;
extern long long stop_time;
extern long long time_for_move;
extern int times_up;

// Pondering
extern volatile int pondering;
extern volatile int stop_pondering;
extern int ponder_move;
extern int ponder_hit;
extern long long ponder_time_for_move;
extern pthread_mutex_t search_mutex;

// UCI Options
extern int hash_size_mb;
extern int multi_pv;
extern int use_nnue_eval;
extern int contempt;
extern int use_book;
extern int probcut_margin;

// Constants
extern char *start_position;
extern char ascii_pieces[12];
extern const int castling_rights[64];
extern const int see_piece_values[12];
extern const int lmp_margins[8];
extern const int futility_margins[7];
extern const int razor_margins[4];
extern const int rfp_margins[7];
extern const int history_max;

// Piece-Square Tables
extern const int pawn_score[64];
extern const int knight_score[64];
extern const int bishop_score[64];
extern const int rook_score[64];
extern const int king_score[64];
extern const int king_endgame_score[64];
extern const int passed_pawn_bonus[8];
extern const int passed_pawn_bonus_eg[8];

// Material Weights
extern int material_weights[12];

// ============================================ \\
//           INLINE UTILITY FUNCTIONS           \\
// ============================================ \\

// Count bits in a bitboard (population count)
static inline int count_bits(U64 bitboard)
{
    int count = 0;
    while (bitboard)
    {
        count++;
        bitboard &= bitboard - 1;
    }
    return count;
}

// Get index of least significant 1 bit
static inline int get_ls1b_index(U64 bitboard)
{
    if (bitboard)
        return count_bits((bitboard & -bitboard) - 1);
    return -1;
}

#endif // TYPES_H
