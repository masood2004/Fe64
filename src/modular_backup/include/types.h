/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file types.h
 * @brief Core type definitions and constants
 * @author Syed Masood
 * @version 4.0.0
 *
 * This file contains all fundamental type definitions,
 * constants, and enumerations used throughout the engine.
 */

#ifndef FE64_TYPES_H
#define FE64_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================
//            PLATFORM DETECTION
// ============================================

#ifdef _WIN32
#define FE64_WINDOWS 1
#include <windows.h>
#include <conio.h>
#else
#define FE64_UNIX 1
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>
#endif

// ============================================
//          FUNDAMENTAL TYPE DEFINITIONS
// ============================================

/**
 * @typedef U64
 * @brief 64-bit unsigned integer for bitboard representation
 *
 * The 64-bit integer is the heart of the engine. We use 'unsigned'
 * because we don't need negative numbers and 'long long' to
 * guarantee 64 bits on all systems.
 */
typedef unsigned long long U64;

/**
 * @typedef Score
 * @brief Type for evaluation scores in centipawns
 */
typedef int Score;

/**
 * @typedef Move
 * @brief Type for move encoding (32-bit)
 */
typedef int Move;

/**
 * @typedef Depth
 * @brief Type for search depth
 */
typedef int Depth;

/**
 * @typedef Square
 * @brief Type for board squares (0-63)
 */
typedef int Square;

/**
 * @typedef Piece
 * @brief Type for piece identification
 */
typedef int Piece;

/**
 * @typedef Color
 * @brief Type for side to move
 */
typedef int Color;

// ============================================
//          ENGINE CONFIGURATION
// ============================================

#define FE64_VERSION "4.0.0"
#define FE64_NAME "Fe64"
#define FE64_AUTHOR "Syed Masood"
#define FE64_STYLE "The Boa Constrictor - Slow Death Style"

// ============================================
//          SEARCH CONSTANTS
// ============================================

#define INF 50000
#define MATE 49000
#define MATE_IN_MAX 48900
#define MAX_PLY 128
#define MAX_MOVES 256
#define MAX_GAME_MOVES 2048

// ============================================
//          BOARD SQUARE MAPPING
// ============================================

/**
 * @enum Squares
 * @brief Board square indices using big-endian mapping
 *
 * We use "Big Endian" mapping: a8 is 0, h1 is 63.
 * This is a standard mapping for many chess engines.
 */
enum Squares
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

// ============================================
//          COLOR/SIDE ENCODING
// ============================================

/**
 * @enum Colors
 * @brief Side to move encoding
 */
enum Colors
{
    WHITE,
    BLACK,
    BOTH
};

// ============================================
//          PIECE ENCODING
// ============================================

/**
 * @enum Pieces
 * @brief Standard chess piece encoding
 *
 * White pieces: P, N, B, R, Q, K (0-5)
 * Black pieces: p, n, b, r, q, k (6-11)
 */
enum Pieces
{
    P,
    N,
    B,
    R,
    Q,
    K, // White pieces
    p,
    n,
    b,
    r,
    q,
    k // Black pieces
};

// ============================================
//          CASTLING RIGHTS
// ============================================

/**
 * @enum CastlingRights
 * @brief Binary encoding of castling rights
 *
 * wk = White King Side  (1)
 * wq = White Queen Side (2)
 * bk = Black King Side  (4)
 * bq = Black Queen Side (8)
 */
enum CastlingRights
{
    wk = 1,
    wq = 2,
    bk = 4,
    bq = 8
};

// ============================================
//          MOVE GENERATION FLAGS
// ============================================

enum MoveType
{
    ALL_MOVES,
    CAPTURES_ONLY
};

// ============================================
//          TRANSPOSITION TABLE FLAGS
// ============================================

enum TTFlags
{
    TT_EXACT = 0,
    TT_ALPHA = 1,
    TT_BETA = 2
};

// ============================================
//          NODE TYPES
// ============================================

enum NodeType
{
    PV_NODE,
    CUT_NODE,
    ALL_NODE
};

// ============================================
//          MOVE LIST STRUCTURE
// ============================================

/**
 * @struct MoveList
 * @brief Structure to hold generated moves
 */
typedef struct
{
    Move moves[MAX_MOVES];
    int count;
} MoveList;

// ============================================
//          TRANSPOSITION TABLE ENTRY
// ============================================

/**
 * @struct TTEntry
 * @brief Transposition table entry structure
 */
typedef struct
{
    U64 key;        // Position hash key
    Depth depth;    // Search depth
    int flags;      // Node type (Exact, Alpha, Beta)
    Score value;    // Evaluation score
    Move best_move; // Best move found
    int age;        // Entry age for replacement
} TTEntry;

// ============================================
//          SEARCH INFO STRUCTURE
// ============================================

/**
 * @struct SearchInfo
 * @brief Information passed to/from search
 */
typedef struct
{
    Depth depth;
    long long nodes;
    long long start_time;
    long long stop_time;
    long long time_for_move;
    int times_up;
    int quit;
    int stopped;

    // Pondering state
    int pondering;
    int ponder_hit;
    int stop_pondering;
    Move ponder_move;

    // Multi-PV support
    int multi_pv;
    int current_pv;

    // Search statistics
    int sel_depth;
    int null_cut;
    int hash_hit;
    int hash_cut;
} SearchInfo;

// ============================================
//          NNUE CONFIGURATION
// ============================================

#ifdef USE_NNUE
#define NNUE_ENABLED 1
#else
#define NNUE_ENABLED 0
#endif

#define NNUE_INPUT_SIZE 768   // 12 pieces * 64 squares
#define NNUE_HIDDEN1_SIZE 512 // First hidden layer (increased)
#define NNUE_HIDDEN2_SIZE 64  // Second hidden layer (increased)
#define NNUE_OUTPUT_SIZE 1    // Single evaluation output
#define NNUE_SCALE 400        // Scale factor for final output

// ============================================
//          OPENING BOOK CONFIGURATION
// ============================================

#define BOOK_MAX_ENTRIES 1000000

/**
 * @struct PolyglotEntry
 * @brief Polyglot opening book entry
 */
typedef struct
{
    U64 key;
    unsigned short move;
    unsigned short weight;
    unsigned int learn;
} PolyglotEntry;

// ============================================
//          BIT MANIPULATION MACROS
// ============================================

/**
 * @def get_bit(bitboard, square)
 * @brief Check if a bit is set at a given square
 */
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))

/**
 * @def set_bit(bitboard, square)
 * @brief Set a bit at a given square
 */
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))

/**
 * @def pop_bit(bitboard, square)
 * @brief Clear a bit at a given square
 */
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

// ============================================
//          FILE/RANK MASKS
// ============================================

// Masks to prevent piece wrapping around board edges
extern const U64 not_a_file;
extern const U64 not_h_file;
extern const U64 not_ab_file;
extern const U64 not_gh_file;

// File masks
extern const U64 file_masks[8];
extern const U64 rank_masks[8];

// ============================================
//          STARTING POSITION
// ============================================

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// ============================================
//          ASCII PIECE CHARACTERS
// ============================================

extern const char ascii_pieces[12];
extern const char *piece_names[12];

#endif // FE64_TYPES_H
