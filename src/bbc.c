// ============================================ \\
//       FE64 CHESS ENGINE - SOURCE CODE        \\
//    "The Boa Constrictor" - Slow Death Style  \\
// ============================================ \\

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
#endif

// ============================================ \\
//         NEURAL NETWORK (NNUE) CONFIG         \\
// ============================================ \\
// Compile with -DUSE_NNUE to enable neural network evaluation

#ifdef USE_NNUE
#define NNUE_ENABLED 1
#else
#define NNUE_ENABLED 0
#endif

// The 64-bit integer is the heart of the engine.
#define U64 unsigned long long

// NNUE Architecture Constants
#define NNUE_INPUT_SIZE 768   // 12 pieces * 64 squares
#define NNUE_HIDDEN1_SIZE 256 // First hidden layer
#define NNUE_HIDDEN2_SIZE 32  // Second hidden layer
#define NNUE_OUTPUT_SIZE 1    // Single evaluation output
#define NNUE_SCALE 400        // Scale factor for final output

// NNUE Weight structure
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

NNUEWeights nnue_weights = {0};

// NNUE Accumulator for incremental updates
typedef struct
{
    float hidden1[NNUE_HIDDEN1_SIZE];
    int valid;
} NNUEAccumulator;

NNUEAccumulator nnue_accum[2]; // [side]

// ReLU activation function
static inline float relu(float x)
{
    return x > 0 ? x : 0;
}

// Clipped ReLU (CReLU) - common in NNUE
static inline float crelu(float x)
{
    if (x < 0)
        return 0;
    if (x > 1)
        return 1;
    return x;
}

// Load NNUE weights from file
int load_nnue(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        printf("info string NNUE file not found: %s\n", filename);
        return 0;
    }

    // Read weights
    size_t read = 0;
    read += fread(nnue_weights.input_weights, sizeof(float), NNUE_INPUT_SIZE * NNUE_HIDDEN1_SIZE, f);
    read += fread(nnue_weights.hidden1_bias, sizeof(float), NNUE_HIDDEN1_SIZE, f);
    read += fread(nnue_weights.hidden1_weights, sizeof(float), NNUE_HIDDEN1_SIZE * NNUE_HIDDEN2_SIZE, f);
    read += fread(nnue_weights.hidden2_bias, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    read += fread(nnue_weights.hidden2_weights, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    read += fread(&nnue_weights.output_bias, sizeof(float), 1, f);

    fclose(f);

    nnue_weights.loaded = 1;
    printf("info string NNUE loaded successfully (%zu parameters)\n", read);
    return 1;
}

// Save NNUE weights (for training)
int save_nnue(const char *filename)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        return 0;

    fwrite(nnue_weights.input_weights, sizeof(float), NNUE_INPUT_SIZE * NNUE_HIDDEN1_SIZE, f);
    fwrite(nnue_weights.hidden1_bias, sizeof(float), NNUE_HIDDEN1_SIZE, f);
    fwrite(nnue_weights.hidden1_weights, sizeof(float), NNUE_HIDDEN1_SIZE * NNUE_HIDDEN2_SIZE, f);
    fwrite(nnue_weights.hidden2_bias, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    fwrite(nnue_weights.hidden2_weights, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    fwrite(&nnue_weights.output_bias, sizeof(float), 1, f);

    fclose(f);
    return 1;
}

// Initialize NNUE with random weights (for training from scratch)
void init_nnue_random()
{
    srand(42); // Fixed seed for reproducibility

    float scale1 = sqrtf(2.0f / NNUE_INPUT_SIZE);
    float scale2 = sqrtf(2.0f / NNUE_HIDDEN1_SIZE);
    float scale3 = sqrtf(2.0f / NNUE_HIDDEN2_SIZE);

    for (int i = 0; i < NNUE_INPUT_SIZE; i++)
        for (int j = 0; j < NNUE_HIDDEN1_SIZE; j++)
            nnue_weights.input_weights[i][j] = ((float)rand() / RAND_MAX - 0.5f) * scale1;

    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
    {
        nnue_weights.hidden1_bias[i] = 0;
        for (int j = 0; j < NNUE_HIDDEN2_SIZE; j++)
            nnue_weights.hidden1_weights[i][j] = ((float)rand() / RAND_MAX - 0.5f) * scale2;
    }

    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++)
    {
        nnue_weights.hidden2_bias[i] = 0;
        nnue_weights.hidden2_weights[i] = ((float)rand() / RAND_MAX - 0.5f) * scale3;
    }

    nnue_weights.output_bias = 0;
    nnue_weights.loaded = 1;
}

// ============================================ \\
//         POLYGLOT OPENING BOOK SUPPORT        \\
// ============================================ \\

#define BOOK_MAX_ENTRIES 500000

typedef struct
{
    U64 key;
    unsigned short move;
    unsigned short weight;
    unsigned int learn;
} PolyglotEntry;

PolyglotEntry *opening_book = NULL;
int book_entries = 0;
int use_book = 1; // Enable/disable book

// Forward declarations (implementations after game state definitions)
U64 get_polyglot_key();
int load_opening_book(const char *filename);
int find_book_entry(U64 key);
int polyglot_to_move(unsigned short poly_move);
int get_book_move();
void free_opening_book();

// Attack Tables

// -------------------------------------------- \\
//               DATA TYPES                     \\
// -------------------------------------------- \\

// Move List Structure
typedef struct
{
    int moves[256];
    int count;
} moves;

// Forward Declarations
void generate_moves(moves *move_list);
int make_move(int move, int move_flag);
void print_board();
void parse_fen(char *fen);
int parse_move(char *move_string);
int count_bits(U64 bitboard);
int get_ls1b_index(U64 bitboard);
int evaluate();

// The 64-bit integer is the heart of the engine.
// We use 'unsigned' because we don't need negative numbers.
// We use 'long long' to guarantee 64 bits on all systems.
// (Already defined above)
U64 pawn_attacks[2][64]; // [side][square]
U64 knight_attacks[64];  // [square]
U64 king_attacks[64];    // [square]

// Magic Numbers (We will fill these at startup)
U64 rook_magic_numbers[64];
U64 bishop_magic_numbers[64];

// Attack Masks (The shape of the piece's path)
U64 rook_masks[64];
U64 bishop_masks[64];

// The Massive Lookup Tables [square][occupancy_index]
// This takes about 2MB of RAM. Cheap.
U64 rook_attacks_table[64][4096];
U64 bishop_attacks_table[64][512];

// Search Constants
#define INF 50000
#define MATE 49000
#define MAX_PLY 64

// Repetition detection
#define MAX_GAME_MOVES 1024
U64 repetition_table[MAX_GAME_MOVES];
int repetition_index = 0;

// Fifty move rule counter
int fifty_move = 0;

int best_move;
long long nodes;

// PV (Principal Variation) table
int pv_length[MAX_PLY];
int pv_table[MAX_PLY][MAX_PLY];

// Late Move Reduction table
int lmr_table[MAX_PLY][64];

// ============================================ \\
//       ADVANCED SEARCH IMPROVEMENTS           \\
// ============================================ \\

// Counter-move heuristic [piece][target_square] = counter_move
int counter_moves[12][64];

// History bonus/malus for butterfly history
int butterfly_history[2][64][64]; // [side][from][to]

// Static Exchange Evaluation piece values
const int see_piece_values[12] = {
    100, 320, 330, 500, 900, 20000, // White
    100, 320, 330, 500, 900, 20000  // Black
};

// Contempt factor - avoid draws when stronger
int contempt = 10; // centipawns

// Late Move Pruning margins (more aggressive)
const int lmp_margins[8] = {0, 5, 8, 12, 18, 25, 33, 42};

// Futility pruning margins by depth
const int futility_margins[7] = {0, 100, 160, 220, 280, 340, 400};

// Razoring margins (prune hopeless positions)
const int razor_margins[4] = {0, 125, 250, 375};

// Reverse futility pruning margins (static null move pruning)
const int rfp_margins[7] = {0, 70, 140, 210, 280, 350, 420};

// Singular extension margin
const int singular_margin = 64;

// History gravity - for aging history scores
const int history_max = 16384;

// Capture history [piece][to][captured_piece]
int capture_history[12][64][6];

// UCI configurable options
int hash_size_mb = 64; // Default 64 MB hash table
int multi_pv = 1;      // Number of principal variations to search
int use_nnue_eval = 0; // Runtime flag to enable/disable NNUE (separate from compile-time)

// Initialize LMR table
void init_lmr_table()
{
    for (int depth = 1; depth < MAX_PLY; depth++)
    {
        for (int moves = 1; moves < 64; moves++)
        {
            lmr_table[depth][moves] = 0.75 + log(depth) * log(moves) / 2.25;
        }
    }
}

// -------------------------------------------- \\
//              BOARD MAPPING                   \\
// -------------------------------------------- \\

// Mapping human readable squares to board indices (0-63)
// We use "Big Endian" mapping: a8 is 0, h1 is 63.
// This is the standard for the tutorial you are following.
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

// Encoding sides (colors) for future use
enum
{
    white,
    black,
    both
};

// Encoding pieces (Standard Chess Representation)
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

// Castling Rights Binary Encoding
// wk = White King Side (1)
// wq = White Queen Side (2)
// bk = Black King Side (4)
// bq = Black Queen Side (8)
enum
{
    wk = 1,
    wq = 2,
    bk = 4,
    bq = 8
};

enum
{
    all_moves,
    only_captures
};

// -------------------------------------------- \\
//               GAME STATE GLOBALS             \\
// -------------------------------------------- \\

// START POSITION FEN
char *start_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Piece Bitboards
// We use an array of 12 bitboards (one for each piece type: P, N, B, R, Q, K, p, n, b, r, q, k)
U64 bitboards[12];

// Occupancy Bitboards
// [0] = All White Pieces
// [1] = All Black Pieces
// [2] = All Pieces (Both colors)
U64 occupancies[3];

// Game State Variables
int side;               // Current side to move (0=white, 1=black)
int en_passant = no_sq; // En passant square (if available, else 'no_sq')
int castle;             // Castling rights integer (e.g., 1111 in binary = 15)

// Search Depth
int search_depth;

// Killer moves [id][ply]
int killer_moves[2][64];

// History moves [piece][square]
int history_moves[12][64];

// Castling rights update constants
const int castling_rights[64] = {
    7, 15, 15, 15, 3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14};

// Piece-square tables to bias the engine's personality
// Higher values = piece wants to be there.
// These tables encourage center control and King-side attacks.

const int pawn_score[64] = {
    90, 90, 90, 90, 90, 90, 90, 90,
    30, 30, 30, 40, 40, 30, 30, 30,
    20, 20, 20, 30, 30, 20, 20, 20,
    10, 10, 10, 20, 20, 10, 10, 10,
    5, 5, 10, 20, 20, 5, 5, 5,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, -10, -10, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0};

const int knight_score[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20, 0, 0, 0, 0, -20, -40,
    -30, 0, 10, 15, 15, 10, 0, -30,
    -30, 5, 15, 20, 20, 15, 5, -30,
    -30, 0, 15, 20, 20, 15, 0, -30,
    -30, 5, 10, 15, 15, 10, 5, -30,
    -40, -20, 0, 5, 5, 0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50};

const int bishop_score[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10, 0, 0, 0, 0, 0, 0, -10,
    -10, 0, 5, 10, 10, 5, 0, -10,
    -10, 5, 5, 10, 10, 5, 5, -10,
    -10, 0, 10, 10, 10, 10, 0, -10,
    -10, 10, 10, 10, 10, 10, 10, -10,
    -10, 5, 0, 0, 0, 0, 5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20};

const int rook_score[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    5, 10, 10, 10, 10, 10, 10, 5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    0, 0, 0, 5, 5, 0, 0, 0};

const int king_score[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    20, 20, 0, 0, 0, 0, 20, 20,
    20, 30, 10, 0, 0, 10, 30, 20};

// ENDGAME King PST - King should centralize in endgames!
const int king_endgame_score[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10, 0, 0, -10, -20, -30,
    -30, -10, 20, 30, 30, 20, -10, -30,
    -30, -10, 30, 40, 40, 30, -10, -30,
    -30, -10, 30, 40, 40, 30, -10, -30,
    -30, -10, 20, 30, 30, 20, -10, -30,
    -30, -30, 0, 0, 0, 0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50};

// Enhanced passed pawn bonus by rank (much stronger in endgame)
const int passed_pawn_bonus_eg[8] = {0, 200, 150, 100, 60, 30, 15, 0};

// -------------------------------------------- \\
//             BITWISE MACROS                   \\
// -------------------------------------------- \\

/* THE TOOLKIT:
   - get_bit: Checks if a bit is 1 (Is there a piece?)
   - set_bit: Turns a bit to 1 (Place a piece)
   - pop_bit: Turns a bit to 0 (Remove a piece)

   LOGIC:
   1ULL << square: Creates a "mask" with only that one specific bit turned on.
*/

#define get_bit(bitboard, square) (bitboard & (1ULL << square))
#define set_bit(bitboard, square) (bitboard |= (1ULL << square))
#define pop_bit(bitboard, square) (get_bit(bitboard, square) ? (bitboard ^= (1ULL << square)) : 0)

// MAGIC BITBOARD LOOKUP MACROS
// 1. Get Occupancy: (board & mask)
// 2. Hash it: * magic
// 3. Index it: >> shift
// 4. Return: table[square][index]

#define get_bishop_attacks_magic(square, occupancy) \
    (bishop_attacks_table[square][((occupancy & bishop_masks[square]) * bishop_magic_numbers[square]) >> (64 - count_bits(bishop_masks[square]))])

#define get_rook_attacks_magic(square, occupancy) \
    (rook_attacks_table[square][((occupancy & rook_masks[square]) * rook_magic_numbers[square]) >> (64 - count_bits(rook_masks[square]))])

// -------------------------------------------- \\
//           MOVE ENCODING                      \\
// -------------------------------------------- \\

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

// Macro to encode a move into an integer
#define encode_move(source, target, piece, promoted, capture, double, enpassant, castling) \
    (source) |                                                                             \
        (target << 6) |                                                                    \
        (piece << 12) |                                                                    \
        (promoted << 16) |                                                                 \
        (capture << 20) |                                                                  \
        (double << 21) |                                                                   \
        (enpassant << 22) |                                                                \
        (castling << 23)

// Macros to decode the integer
#define get_move_source(move) (move & 0x3f)
#define get_move_target(move) ((move & 0xfc0) >> 6)
#define get_move_piece(move) ((move & 0xf000) >> 12)
#define get_move_promoted(move) ((move & 0xf0000) >> 16)
#define get_move_capture(move) (move & 0x100000)
#define get_move_double(move) (move & 0x200000)
#define get_move_enpassant(move) (move & 0x400000)
#define get_move_castling(move) (move & 0x800000)

// Helper to add a move to the list
void add_move(moves *move_list, int move)
{
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

// Helper to print a move (e.g. "e2e4")
// Note: Requires your 'square_to_coordinates' array or manual printing
void print_move(int move)
{
    printf("%c%d%c%d",
           (get_move_source(move) % 8) + 'a',
           8 - (get_move_source(move) / 8),
           (get_move_target(move) % 8) + 'a',
           8 - (get_move_target(move) / 8));

    int promoted = get_move_promoted(move);
    if (promoted)
    {
        // Map promoted piece to character
        char promo_char;
        switch (promoted)
        {
        case Q:
        case q:
            promo_char = 'q';
            break;
        case R:
        case r:
            promo_char = 'r';
            break;
        case B:
        case b:
            promo_char = 'b';
            break;
        case N:
        case n:
            promo_char = 'n';
            break;
        default:
            promo_char = 'q';
            break;
        }
        printf("%c", promo_char);
    }
}

// MVV (Most Valuable Victim) LVA (Least Valuable Attacker) [attacker][victim]
static int mvv_lva[12][12] = {
    {105, 205, 305, 405, 505, 605, 105, 205, 305, 405, 505, 605}, // P
    {104, 204, 304, 404, 504, 604, 104, 204, 304, 404, 504, 604}, // N
    {103, 203, 303, 403, 503, 603, 103, 203, 303, 403, 503, 603}, // B
    {102, 202, 302, 402, 502, 602, 102, 202, 302, 402, 502, 602}, // R
    {101, 201, 301, 401, 501, 601, 101, 201, 301, 401, 501, 601}, // Q
    {100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600}, // K
    {105, 205, 305, 405, 505, 605, 105, 205, 305, 405, 505, 605}, // p
    {104, 204, 304, 404, 504, 604, 104, 204, 304, 404, 504, 604}, // n
    {103, 203, 303, 403, 503, 603, 103, 203, 303, 403, 503, 603}, // b
    {102, 202, 302, 402, 502, 602, 102, 202, 302, 402, 502, 602}, // r
    {101, 201, 301, 401, 501, 601, 101, 201, 301, 401, 501, 601}, // q
    {100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600}, // k
};

// ============================================ \\
//   STATIC EXCHANGE EVALUATION (SEE)           \\
//   Determines if a capture sequence is good   \\
// ============================================ \\

// Get smallest attacker to a square
int get_smallest_attacker(int square, int attacker_side, int *attacker_piece)
{
    int start_piece, end_piece;

    if (attacker_side == white)
    {
        start_piece = P;
        end_piece = K;
    }
    else
    {
        start_piece = p;
        end_piece = k;
    }

    // Check pawns first (smallest)
    if (attacker_side == white)
    {
        if (pawn_attacks[white][square] & bitboards[P])
        {
            *attacker_piece = P;
            return get_ls1b_index(pawn_attacks[white][square] & bitboards[P]);
        }
    }
    else
    {
        if (pawn_attacks[black][square] & bitboards[p])
        {
            *attacker_piece = p;
            return get_ls1b_index(pawn_attacks[black][square] & bitboards[p]);
        }
    }

    // Knights
    int knight_piece = (attacker_side == white) ? N : n;
    if (knight_attacks[square] & bitboards[knight_piece])
    {
        *attacker_piece = knight_piece;
        return get_ls1b_index(knight_attacks[square] & bitboards[knight_piece]);
    }

    // Bishops
    int bishop_piece = (attacker_side == white) ? B : b;
    U64 bishop_atks = get_bishop_attacks_magic(square, occupancies[both]);
    if (bishop_atks & bitboards[bishop_piece])
    {
        *attacker_piece = bishop_piece;
        return get_ls1b_index(bishop_atks & bitboards[bishop_piece]);
    }

    // Rooks
    int rook_piece = (attacker_side == white) ? R : r;
    U64 rook_atks = get_rook_attacks_magic(square, occupancies[both]);
    if (rook_atks & bitboards[rook_piece])
    {
        *attacker_piece = rook_piece;
        return get_ls1b_index(rook_atks & bitboards[rook_piece]);
    }

    // Queens
    int queen_piece = (attacker_side == white) ? Q : q;
    U64 queen_atks = rook_atks | bishop_atks;
    if (queen_atks & bitboards[queen_piece])
    {
        *attacker_piece = queen_piece;
        return get_ls1b_index(queen_atks & bitboards[queen_piece]);
    }

    // King (only if no other options - capturing with king is risky)
    int king_piece = (attacker_side == white) ? K : k;
    if (king_attacks[square] & bitboards[king_piece])
    {
        *attacker_piece = king_piece;
        return get_ls1b_index(king_attacks[square] & bitboards[king_piece]);
    }

    return -1; // No attacker found
}

// Static Exchange Evaluation
// Returns estimated value of capture sequence
int see(int move)
{
    int from = get_move_source(move);
    int to = get_move_target(move);
    int attacker = get_move_piece(move);

    // Find captured piece
    int captured = -1;
    int start = (side == white) ? p : P;
    int end = (side == white) ? k : K;

    for (int piece = start; piece <= end; piece++)
    {
        if (get_bit(bitboards[piece], to))
        {
            captured = piece;
            break;
        }
    }

    // En passant capture
    if (get_move_enpassant(move))
    {
        captured = (side == white) ? p : P;
    }

    if (captured == -1)
        return 0; // Not a capture

    int gain[32];
    int depth = 0;

    // Initial gain is captured piece value
    gain[depth] = see_piece_values[captured % 6];

    // Simulate the capture
    int current_side = side ^ 1; // Opponent responds
    int piece_on_square = attacker;

    // Simple SEE approximation (full SEE is complex)
    // Just check if the square is defended
    int defender_piece;
    int defender_sq = get_smallest_attacker(to, current_side, &defender_piece);

    if (defender_sq == -1)
    {
        // Not defended - good capture!
        return gain[0];
    }

    // Square is defended - check if exchange is good
    // If we captured with piece worth more than victim - bad
    if (see_piece_values[attacker % 6] > gain[0])
    {
        return gain[0] - see_piece_values[attacker % 6];
    }

    return gain[0];
}

// Check if SEE value of a move is >= threshold
int see_ge(int move, int threshold)
{
    return see(move) >= threshold;
}

// Track the last move made for counter-move heuristic (declared early for score_move)
int last_move_made[MAX_PLY];

// Score moves to decide which to search first
// Uses: PV > Good Captures > Killers > Counter-move > History
int score_move(int move, int pv_move, int ply)
{
    // 1. PV Move (Highest Priority)
    if (pv_move && move == pv_move)
        return 30000;

    int piece = get_move_piece(move);
    int target = get_move_target(move);
    int from = get_move_source(move);

    // 2. Promotions - very high priority, especially queen promotions
    int promoted = get_move_promoted(move);
    if (promoted)
    {
        // Queen promotion is best
        if (promoted == Q || promoted == q)
            return 28000;
        return 25000 + promoted;
    }

    // 3. Captures - use SEE and MVV-LVA
    if (get_move_capture(move))
    {
        int victim = P;

        int start = (side == white) ? p : P;
        int end = (side == white) ? k : K;
        for (int p = start; p <= end; p++)
        {
            if (get_bit(bitboards[p], target))
            {
                victim = p;
                break;
            }
        }

        // Use SEE to determine capture quality
        int see_value = see(move);
        if (see_value >= 0)
        {
            // Good captures: 10000 + MVV-LVA + capture history bonus
            int cap_hist = capture_history[piece][target][victim % 6] / 64;
            return 15000 + mvv_lva[piece][victim] + cap_hist;
        }
        else
        {
            // Bad captures: ordered by SEE value (negative), placed after quiet moves
            return see_value;
        }
    }

    // 4. Killer Moves (1st Killer)
    if (killer_moves[0][ply] == move)
        return 9000;

    // 5. Killer Moves (2nd Killer)
    if (killer_moves[1][ply] == move)
        return 8500;

    // 6. Counter-move heuristic
    if (ply > 0 && last_move_made[ply - 1])
    {
        int last_piece = get_move_piece(last_move_made[ply - 1]);
        int last_target = get_move_target(last_move_made[ply - 1]);
        if (counter_moves[last_piece][last_target] == move)
            return 8000;
    }

    // 7. History Moves + Butterfly history bonus
    int history_score = history_moves[piece][target];
    history_score += butterfly_history[side][from][target] / 4;

    // Clamp to prevent overflow issues
    if (history_score > 7999)
        history_score = 7999;
    if (history_score < -7999)
        history_score = -7999;

    return history_score;
}

// -------------------------------------------- \\
//               TIME MANAGEMENT                \\
// -------------------------------------------- \\

// Get time in milliseconds
long long get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Global timing variables
long long start_time;
long long stop_time;
long long time_for_move;
int times_up = 0;

// ============================================ \\
//              PONDERING SUPPORT               \\
//  Think on opponent's time for free ELO!      \\
// ============================================ \\

volatile int pondering = 0;          // Is engine currently pondering?
volatile int stop_pondering = 0;     // Signal to stop pondering
int ponder_move = 0;                 // The move we're pondering on
int ponder_hit = 0;                  // Did opponent play our expected move?
long long ponder_time_for_move = -1; // Time allocated when ponderhit occurs
pthread_mutex_t search_mutex = PTHREAD_MUTEX_INITIALIZER;

// Check if input is available on stdin (non-blocking)
int input_waiting()
{
#ifdef _WIN32
    // Windows: Check if input is available
    static int init = 0;
    static HANDLE h;
    DWORD mode;
    if (!init)
    {
        init = 1;
        h = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(h, &mode);
        SetConsoleMode(h, mode & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
        FlushConsoleInputBuffer(h);
    }
    return _kbhit();
#else
    // Linux/Unix: Use select() for non-blocking check
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0;
#endif
}

// Flag to indicate if we should check stdin during search
int check_stdin_during_search = 0;

// Check if time is up OR if we should stop pondering OR if GUI sent stop
void communicate()
{
    if (times_up)
        return;

    // Check for stop signal during pondering
    if (stop_pondering)
    {
        times_up = 1;
        return;
    }

    // If pondering and got ponderhit, switch to timed search
    if (pondering && ponder_hit && ponder_time_for_move != -1)
    {
        pondering = 0;
        time_for_move = ponder_time_for_move;
        start_time = get_time_ms(); // Reset start time for new time allocation
    }

    // If still pondering (infinite), don't time out
    if (pondering)
        return;

    // Fix: If time is infinite (-1), do not abort based on time
    if (time_for_move == -1)
        return;

    if ((get_time_ms() - start_time) > time_for_move)
    {
        times_up = 1; // STOP THE SEARCH
    }
}

// -------------------------------------------- \\
//             BOARD MANIPULATION               \\
// -------------------------------------------- \\

// Macro to restore board state
#define copy_board()                             \
    U64 bitboards_copy[12], occupancies_copy[3]; \
    int side_copy, en_passant_copy, castle_copy; \
    memcpy(bitboards_copy, bitboards, 96);       \
    memcpy(occupancies_copy, occupancies, 24);   \
    side_copy = side;                            \
    en_passant_copy = en_passant;                \
    castle_copy = castle;

// Macro to restore board state
#define take_back()                            \
    memcpy(bitboards, bitboards_copy, 96);     \
    memcpy(occupancies, occupancies_copy, 24); \
    side = side_copy;                          \
    en_passant = en_passant_copy;              \
    castle = castle_copy;

// -------------------------------------------- \\
//           BIT MANIPULATION HELPERS           \\
// -------------------------------------------- \\

// Count bits in a bitboard (Population Count)
// Kernighan's Algorithm: A fast way to count set bits
int count_bits(U64 bitboard)
{
    int count = 0;

    // Repeatedly clear the least significant bit until board is empty
    while (bitboard > 0)
    {
        count++;
        bitboard &= bitboard - 1; // Resets LS1B
    }

    return count;
}

// Get Least Significant 1st Bit Index (LS1B)
// Returns the index (0-63) of the first '1' bit
int get_ls1b_index(U64 bitboard)
{
    // If board is empty, return -1 (Safety check)
    if (bitboard == 0)
        return -1;

    // Use the count_bits logic on the isolated LS1B to find its index
    // Or simpler: use a built-in GCC function if available, but let's stick to C logic for now.
    // The logic: (bitboard ^ (bitboard - 1)) isolates the LS1B.
    // Then we just count how many trailing zeros are behind it?
    // Actually, simple counting is easiest for portable C:

    return count_bits((bitboard & -bitboard) - 1);
}

// -------------------------------------------- \\
//             HELPER FUNCTIONS                 \\
// -------------------------------------------- \\

// Function to visualize the bitboard in the console
void print_bitboard(U64 bitboard)
{
    printf("\n");

    // Loop through ranks (rows) from top (0) to bottom (7)
    for (int rank = 0; rank < 8; rank++)
    {
        // Loop through files (columns) from left (0) to right (7)
        for (int file = 0; file < 8; file++)
        {

            // Print the rank label (8, 7, 6...) on the left side
            if (file == 0)
                printf("  %d ", 8 - rank);

            // Calculate the square index (0-63)
            int square = rank * 8 + file;

            // Check if the bit at this square is 1 or 0
            // logic: If get_bit returns non-zero, print 1, else print 0
            printf(" %d", get_bit(bitboard, square) ? 1 : 0);
        }
        printf("\n"); // New line after every rank
    }

    // Print file labels (a-h) and the raw decimal value
    printf("\n     a b c d e f g h \n\n");
    printf("     Bitboard: %llu\n\n", bitboard);
}

// Mapping from internal integer to ASCII character
char ascii_pieces[12] = "PNBRQKpnbrqk";

// Print the full game board
void print_board()
{
    printf("\n");

    // Loop over ranks
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;

            // Print rank labels
            if (!file)
                printf("  %d ", 8 - rank);

            // Check all 12 bitboards to see which piece is here
            int piece = -1;

            for (int bb_piece = P; bb_piece <= k; bb_piece++)
            {
                if (get_bit(bitboards[bb_piece], square))
                {
                    piece = bb_piece;
                }
            }

            // Print the piece or a dot '.'
            printf(" %c", (piece == -1) ? '.' : ascii_pieces[piece]);
        }
        printf("\n");
    }

    printf("\n     a b c d e f g h \n\n");

    // Print Side to Move
    printf("     Side:     %s\n", !side ? "white" : "black");

    // Print En Passant Square
    printf("     EnPassant:   %s\n", (en_passant != no_sq) ? "Yes" : "no"); // Simplified for now

    // Print Castling Rights
    printf("     Castling:  %c%c%c%c\n\n",
           (castle & wk) ? 'K' : '-',
           (castle & wq) ? 'Q' : '-',
           (castle & bk) ? 'k' : '-',
           (castle & bq) ? 'q' : '-');
}

// -------------------------------------------- \\
//           RANDOM NUMBER GENERATION           \\
// -------------------------------------------- \\

// Pseudo-random number state
unsigned int random_state = 1804289383;

// Generate 32-bit pseudo-random number
unsigned int get_random_U32_number()
{
    // XOR-shift algorithm
    unsigned int number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

// Generate 64-bit pseudo-random number
U64 get_random_U64_number()
{
    // Define 4 random 32-bit numbers
    U64 n1 = (U64)(get_random_U32_number()) & 0xFFFF;
    U64 n2 = (U64)(get_random_U32_number()) & 0xFFFF;
    U64 n3 = (U64)(get_random_U32_number()) & 0xFFFF;
    U64 n4 = (U64)(get_random_U32_number()) & 0xFFFF;

    // Combine them into one 64-bit number
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

// Generate "Sparse" Magic Candidates
// Magic numbers work best when they have few bits set.
// We AND three random numbers together to reduce the number of 1s.
U64 generate_magic_candidate()
{
    return get_random_U64_number() & get_random_U64_number() & get_random_U64_number();
}

// -------------------------------------------- \\
//               ZOBRIST HASHING                \\
// -------------------------------------------- \\

// Zobrist keys
U64 piece_keys[12][64];
U64 side_key;
U64 castle_keys[16];
U64 enpassant_keys[64];

// Current hash key
U64 hash_key;

// Check for repetition (draw detection)
int is_repetition()
{
    for (int i = repetition_index - 2; i >= 0; i -= 2)
    {
        if (repetition_table[i] == hash_key)
            return 1;
    }
    return 0;
}

// Initialize random keys
void init_hash_keys()
{
    random_state = 1804289383; // Reset RNG for consistency
    for (int p = P; p <= k; p++)
        for (int s = 0; s < 64; s++)
            piece_keys[p][s] = get_random_U64_number();
    side_key = get_random_U64_number();
    for (int i = 0; i < 16; i++)
        castle_keys[i] = get_random_U64_number();
    for (int i = 0; i < 64; i++)
        enpassant_keys[i] = get_random_U64_number();
}

// Generate full hash key from scratch
U64 generate_hash_key()
{
    U64 final_key = 0ULL;
    for (int p = P; p <= k; p++)
    {
        U64 bitboard = bitboards[p];
        while (bitboard)
        {
            final_key ^= piece_keys[p][get_ls1b_index(bitboard)];
            pop_bit(bitboard, get_ls1b_index(bitboard));
        }
    }
    if (side == black)
        final_key ^= side_key;
    if (en_passant != no_sq)
        final_key ^= enpassant_keys[en_passant];
    final_key ^= castle_keys[castle];
    return final_key;
}

// TT flags
#define HASH_EXACT 0
#define HASH_ALPHA 1
#define HASH_BETA 2

typedef struct
{
    U64 key;   // Unique position identifier
    int depth; // Search depth
    int flags; // Node type (Exact, Alpha, Beta)
    int value; // Score
    int best_move;
} tt_entry;

#define TT_SIZE 0x400000 // ~4 million entries (approx 100MB)
tt_entry transposition_table[TT_SIZE];

void clear_tt()
{
    memset(transposition_table, 0, sizeof(transposition_table));
}

int read_tt(int alpha, int beta, int depth, int ply)
{
    tt_entry *entry = &transposition_table[hash_key % TT_SIZE];
    if (entry->key == hash_key)
    {
        if (entry->depth >= depth)
        {
            int score = entry->value;

            // Adjust mate scores for distance from root
            if (score > MATE - 100)
                score -= ply;
            if (score < -MATE + 100)
                score += ply;

            if (entry->flags == HASH_EXACT)
                return score;
            if (entry->flags == HASH_ALPHA && score <= alpha)
                return alpha;
            if (entry->flags == HASH_BETA && score >= beta)
                return beta;
        }
    }
    return -INF - 1; // Different from -INF to distinguish "not found"
}

// Get best move from TT (for PV extraction)
int get_tt_move()
{
    tt_entry *entry = &transposition_table[hash_key % TT_SIZE];
    if (entry->key == hash_key)
        return entry->best_move;
    return 0;
}

void write_tt(int depth, int value, int flags, int move, int ply)
{
    tt_entry *entry = &transposition_table[hash_key % TT_SIZE];

    // Replacement strategy: always replace if deeper or same position
    if (entry->key != hash_key || entry->depth <= depth)
    {
        // Adjust mate scores for storage
        int score_to_store = value;
        if (value > MATE - 100)
            score_to_store += ply;
        if (value < -MATE + 100)
            score_to_store -= ply;

        entry->key = hash_key;
        entry->depth = depth;
        entry->flags = flags;
        entry->value = score_to_store;
        entry->best_move = move;
    }
}

// -------------------------------------------- \\
//           ATTACK GENERATION CONSTANTS        \\
// -------------------------------------------- \\

// These are bitboards that represent "Everything EXCEPT file A" and "Everything EXCEPT file H"
// We use these to prevent pieces from wrapping around the board edges.
const U64 not_a_file = 18374403900871474942ULL;
const U64 not_h_file = 9187201950435737471ULL;

// Knight/King Attack Masks
// NOT A/B: Masks out the first two files (A and B) to prevent "Left 2" wrap-around.
// NOT G/H: Masks out the last two files (G and H) to prevent "Right 2" wrap-around.
const U64 not_ab_file = 18229723555195321596ULL;
const U64 not_gh_file = 4557430888798830399ULL;

// -------------------------------------------- \\
//           ATTACK GENERATION FUNCTIONS        \\
// -------------------------------------------- \\

// Generate Pawn Attacks
// side: 0 (white), 1 (black)
// square: current square of the pawn (0-63)
U64 mask_pawn_attacks(int side, int square)
{

    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    // WHITE PAWNS (Move "Up" / Smaller Index)
    if (!side)
    {
        // Attack Right (North East) - Offset: -7
        // Shift >> 7 moves bits Right. We must prevent wrapping from H-file to A-file.
        // So we mask with 'not_a_file' (Keep everything except A).
        if ((bitboard >> 7) & not_a_file)
            attacks |= (bitboard >> 7);

        // Attack Left (North West) - Offset: -9
        // Shift >> 9 moves bits Left. We must prevent wrapping from A-file to H-file.
        // So we mask with 'not_h_file' (Keep everything except H).
        if ((bitboard >> 9) & not_h_file)
            attacks |= (bitboard >> 9);
    }

    // BLACK PAWNS (Move "Down" / Larger Index)
    else
    {
        // Attack Right (South East) - Offset: +9
        // Shift << 9 moves bits Right. Prevent wrapping H -> A.
        if ((bitboard << 9) & not_a_file)
            attacks |= (bitboard << 9);

        // Attack Left (South West) - Offset: +7
        // Shift << 7 moves bits Left. Prevent wrapping A -> H.
        if ((bitboard << 7) & not_h_file)
            attacks |= (bitboard << 7);
    }

    return attacks;
}

// Generate Knight Attacks
U64 mask_knight_attacks(int square)
{

    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    // 1. North-North-East (-15) & North-East-East (-6) -> Needs "Not H" and "Not GH"
    // Note: Adjust offsets based on your specific enum mapping!
    // If a8=0:
    // NNE: -17 (Up 2 Left 1) | NNW: -15 (Up 2 Right 1) ??
    // WAIT. Let's verify the enum map:
    // a8=0, b8=1 ... h1=63.
    // Up=Minus. Down=Plus. Left=Minus. Right=Plus.

    // NNE: Up 2 (-16), Right 1 (+1) = -15
    // NEE: Up 1 (-8), Right 2 (+2) = -6
    // NNW: Up 2 (-16), Left 1 (-1) = -17
    // NWW: Up 1 (-8), Left 2 (-2) = -10

    // SSE: Down 2 (+16), Right 1 (+1) = +17
    // SEE: Down 1 (+8), Right 2 (+2) = +10
    // SSW: Down 2 (+16), Left 1 (-1) = +15
    // SWW: Down 1 (+8), Left 2 (-2) = +6

    // North-North-West (-17) -> Needs Not A File
    if ((bitboard >> 17) & not_h_file)
        attacks |= (bitboard >> 17);

    // North-North-East (-15) -> Needs Not A File (Wait, -15 is Right? No. -15 is Up 2 Right 1)
    // Let's stick to the standard offsets for Little Endian Rank-File Mapping.
    // Implementation for your specific map:

    // NNW (-17) : Up 2 Left 1. (Requires Not H? No. Moving Left requires Not A).
    // WAIT. Your mapping is a8=0.
    // a8 (0) >> 17 is 0.
    // Let's use the explicit logic:

    // 1. Up 2 Left 1 (-17) [Requires NOT H file? No. Left requires Not A. But shifting right is decreasing index.]
    if ((bitboard >> 17) & not_h_file)
        attacks |= (bitboard >> 17);
    if ((bitboard >> 15) & not_a_file)
        attacks |= (bitboard >> 15);
    if ((bitboard >> 10) & not_gh_file)
        attacks |= (bitboard >> 10);
    if ((bitboard >> 6) & not_ab_file)
        attacks |= (bitboard >> 6);

    if ((bitboard << 17) & not_a_file)
        attacks |= (bitboard << 17);
    if ((bitboard << 15) & not_h_file)
        attacks |= (bitboard << 15);
    if ((bitboard << 10) & not_ab_file)
        attacks |= (bitboard << 10);
    if ((bitboard << 6) & not_gh_file)
        attacks |= (bitboard << 6);

    return attacks;
}

// Generate King Attacks (Bonus: It's very similar)
U64 mask_king_attacks(int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    // King moves 1 step in all 8 directions
    if (bitboard >> 8)
        attacks |= (bitboard >> 8); // North
    if ((bitboard >> 9) & not_h_file)
        attacks |= (bitboard >> 9); // North West
    if ((bitboard >> 7) & not_a_file)
        attacks |= (bitboard >> 7); // North East
    if ((bitboard >> 1) & not_h_file)
        attacks |= (bitboard >> 1); // West

    if (bitboard << 8)
        attacks |= (bitboard << 8); // South
    if ((bitboard << 9) & not_a_file)
        attacks |= (bitboard << 9); // South East
    if ((bitboard << 7) & not_h_file)
        attacks |= (bitboard << 7); // South West
    if ((bitboard << 1) & not_a_file)
        attacks |= (bitboard << 1); // East

    return attacks;
}

// -------------------------------------------- \\
//           SLIDING PIECE ATTACKS              \\
// -------------------------------------------- \\

// Calculate Bishop Attacks (Diagonal)
// square: where the bishop is
// block: a bitboard of ALL pieces on the board (both colors)
U64 get_bishop_attacks(int square, U64 block)
{
    U64 attacks = 0ULL;

    // Init target rank & files
    int r, f;

    // Extract current Rank and File from the square index
    int tr = square / 8;
    int tf = square % 8;

    // 1. Generate South-East Attacks
    for (r = tr + 1, f = tf + 1; r < 8 && f < 8; r++, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break; // If we hit a piece, STOP.
    }

    // 2. Generate North-East Attacks
    for (r = tr - 1, f = tf + 1; r >= 0 && f < 8; r--, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    // 3. Generate South-West Attacks
    for (r = tr + 1, f = tf - 1; r < 8 && f >= 0; r++, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    // 4. Generate North-West Attacks
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    return attacks;
}

// Calculate Rook Attacks (Straight)
U64 get_rook_attacks(int square, U64 block)
{
    U64 attacks = 0ULL;

    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    // 1. South (Rank +)
    for (r = tr + 1; r < 8; r++)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }

    // 2. North (Rank -)
    for (r = tr - 1; r >= 0; r--)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }

    // 3. East (File +)
    for (f = tf + 1; f < 8; f++)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }

    // 4. West (File -)
    for (f = tf - 1; f >= 0; f--)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }

    return attacks;
}

// Calculate Queen Attacks?
// Simple! Queen = Bishop + Rook
U64 get_queen_attacks(int square, U64 block)
{
    return get_bishop_attacks(square, block) | get_rook_attacks(square, block);
}

// Initialize all attack tables
void init_leapers_attacks()
{
    // Loop over all 64 squares
    for (int square = 0; square < 64; square++)
    {

        // Init Pawn Attacks
        pawn_attacks[white][square] = mask_pawn_attacks(white, square);
        pawn_attacks[black][square] = mask_pawn_attacks(black, square);

        // Init Knight Attacks
        knight_attacks[square] = mask_knight_attacks(square);

        // Init King Attacks
        king_attacks[square] = mask_king_attacks(square);
    }
}

// -------------------------------------------- \\
//           MAGIC BITBOARD HELPERS             \\
// -------------------------------------------- \\

// Mask relevant occupancy bits for Bishop attacks
// (Excludes the outer edges because they don't change the ray length)
U64 mask_bishop_attacks(int square)
{
    U64 attacks = 0ULL;

    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    // Scan only 1 square inward from edges
    for (r = tr + 1, f = tf + 1; r < 7 && f < 7; r++, f++)
        attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf + 1; r > 0 && f < 7; r--, f++)
        attacks |= (1ULL << (r * 8 + f));
    for (r = tr + 1, f = tf - 1; r < 7 && f > 0; r++, f--)
        attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf - 1; r > 0 && f > 0; r--, f--)
        attacks |= (1ULL << (r * 8 + f));

    return attacks;
}

// Mask relevant occupancy bits for Rook attacks
U64 mask_rook_attacks(int square)
{
    U64 attacks = 0ULL;

    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    // Stop at r < 7 (ignore Rank 8)
    for (r = tr + 1; r < 7; r++)
        attacks |= (1ULL << (r * 8 + tf));

    // Stop at r > 0 (ignore Rank 1)
    for (r = tr - 1; r > 0; r--)
        attacks |= (1ULL << (r * 8 + tf));

    // Stop at f < 7 (ignore File H)
    for (f = tf + 1; f < 7; f++)
        attacks |= (1ULL << (tr * 8 + f));

    // Stop at f > 0 (ignore File A)
    for (f = tf - 1; f > 0; f--)
        attacks |= (1ULL << (tr * 8 + f));

    return attacks;
}

// Generate Occupancy Variations
// index: The current variation index (0 to 4095 usually)
// bits_in_mask: How many bits are set in the attack mask
// attack_mask: The base mask we are varying
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask)
{

    U64 occupancy = 0ULL;

    // Loop over the range of bits in the mask
    for (int count = 0; count < bits_in_mask; count++)
    {

        // Get the square of the next bit in the mask
        int square = get_ls1b_index(attack_mask);

        // Remove that bit so we can find the next one in the next loop
        pop_bit(attack_mask, square);

        // Check if the 'index' has this specific bit set
        // If yes, map it to the actual board square
        if (index & (1 << count))
        {
            occupancy |= (1ULL << square);
        }
    }

    return occupancy;
}

// -------------------------------------------- \\
//           MAGIC NUMBER FINDER                \\
// -------------------------------------------- \\

// Find a magic number for a specific square and piece type
U64 find_magic_number(int square, int relevant_bits, int bishop)
{

    // 1. Initialize occupancies and attacks
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];

    // 2. Determine the attack mask based on piece type
    U64 attack_mask = bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);

    // 3. Calculate total variations (1 << relevant_bits)
    int occupancy_indices = 1 << relevant_bits;

    // 4. Pre-calculate all occupancy patterns and their corresponding true attacks
    for (int index = 0; index < occupancy_indices; index++)
    {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);

        attacks[index] = bishop ? get_bishop_attacks(square, occupancies[index]) : get_rook_attacks(square, occupancies[index]);
    }

    // 5. Brute force: Try random numbers until one works
    for (int random_count = 0; random_count < 100000000; random_count++)
    {

        // Generate a candidate
        U64 magic_number = generate_magic_candidate();

        // Skip unsuitable candidates (Safety check, often finding magics with enough bits helps)
        // if (count_bits((attack_mask * magic_number) & 0xFF00000000000000ULL) < 6)
        //     continue;

        // Reset the "used" array
        for (int i = 0; i < 4096; i++)
            used_attacks[i] = 0ULL;

        int index, fail = 0;

        // 6. Test the candidate against every single variation
        for (index = 0; index < occupancy_indices; index++)
        {

            // THE MAGIC FORMULA:
            // (Occupancy * Magic) >> (64 - RelevantBits) -> Hash Index
            int magic_index = (int)((occupancies[index] * magic_number) >> (64 - relevant_bits));

            // Collision Check:
            if (used_attacks[magic_index] == 0ULL)
            {
                // New index, store the attack data
                used_attacks[magic_index] = attacks[index];
            }
            else if (used_attacks[magic_index] != attacks[index])
            {
                // COLLISION!
                // Two different board states mapped to the same index but require different moves.
                // This magic number is bad. Fail.
                fail = 1;
                break;
            }
        }

        // 7. If no collision occurred, WE FOUND GOLD!
        if (!fail)
        {
            return magic_number;
        }
    }

    printf("Magic number search failed!\n");
    return 0ULL;
}

// Initialize Sliding Piece Attacks (Magic Bitboards)
// bishop: 1 = Initialize Bishops, 0 = Initialize Rooks
void init_sliders_attacks(int bishop)
{
    // Loop over all 64 squares
    for (int square = 0; square < 64; square++)
    {

        // 1. Initialize masks and relevant bits
        // Bishop masks are smaller (9 bits max), Rook masks are larger (12 bits max)
        bishop_masks[square] = mask_bishop_attacks(square);
        rook_masks[square] = mask_rook_attacks(square);

        U64 attack_mask = bishop ? bishop_masks[square] : rook_masks[square];
        int relevant_bits_count = count_bits(attack_mask);

        // 2. Find the Magic Number (The Miner)
        // Note: In a finished engine, we pre-calculate these and hardcode them constants.
        // For now, calculating at startup is fine (takes a few seconds).
        U64 magic_number = find_magic_number(square, relevant_bits_count, bishop);

        // 3. Save the Magic Number
        if (bishop)
            bishop_magic_numbers[square] = magic_number;
        else
            rook_magic_numbers[square] = magic_number;

        // 4. Populate the Lookup Table
        // We iterate through every possible blocker variation again
        int occupancy_indices = 1 << relevant_bits_count;

        for (int index = 0; index < occupancy_indices; index++)
        {

            // Generate the board state for this index
            U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);

            // Generate the "Real" attack set (using the slow function)
            int magic_index = (int)((occupancy * magic_number) >> (64 - relevant_bits_count));

            if (bishop)
            {
                bishop_attacks_table[square][magic_index] = get_bishop_attacks(square, occupancy);
            }
            else
            {
                rook_attacks_table[square][magic_index] = get_rook_attacks(square, occupancy);
            }
        }
    }
}

// -------------------------------------------- \\
//           MOVE GENERATION                    \\
// -------------------------------------------- \\

// Check if a square is attacked by a given side
// square: The square we are checking (e.g., e4)
// side: The side DOING the attacking (e.g., Black)
int is_square_attacked(int square, int side)
{

    // 1. Check if attacked by Pawns
    // We use the Pawn Attack Mask logic in reverse:
    // If we are White, we check if a Black pawn is "attacking" us (which looks like a White pawn attack pattern)
    // Actually, simpler: "If I am on 'square', where would a pawn be to attack me?"

    // Attacked by white pawns (if side is white)
    if ((side == white) && (pawn_attacks[black][square] & bitboards[P]))
        return 1;

    // Attacked by black pawns (if side is black)
    if ((side == black) && (pawn_attacks[white][square] & bitboards[p]))
        return 1;

    // 2. Check if attacked by Knights
    // If we place a knight on 'square', does it land on an enemy knight?
    if (knight_attacks[square] & ((side == white) ? bitboards[N] : bitboards[n]))
        return 1;

    // 3. Check if attacked by Kings
    if (king_attacks[square] & ((side == white) ? bitboards[K] : bitboards[k]))
        return 1;

    // 4. Check if attacked by Bishops or Queens (Diagonal)
    U64 diagonal_attackers = (side == white) ? (bitboards[B] | bitboards[Q]) : (bitboards[b] | bitboards[q]);
    if (get_bishop_attacks_magic(square, occupancies[both]) & diagonal_attackers)
        return 1;

    // 5. Check if attacked by Rooks or Queens (Straight)
    U64 straight_attackers = (side == white) ? (bitboards[R] | bitboards[Q]) : (bitboards[r] | bitboards[q]);
    if (get_rook_attacks_magic(square, occupancies[both]) & straight_attackers)
        return 1;

    return 0;
}

// Generate all legal moves
void generate_moves(moves *move_list)
{
    move_list->count = 0;

    int source_square, target_square;
    U64 bitboard, attacks;

    // -------------------------------------------- \\
    //                 PAWN MOVES                   \\
    // -------------------------------------------- \\

    if (side == white)
    {
        bitboard = bitboards[P];

        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);

            // Quiet Pushes
            target_square = source_square - 8;
            if (!(target_square < a8) && !get_bit(occupancies[both], target_square))
            {
                // Promotion (Rank 7 to 8)
                if (source_square >= a7 && source_square <= h7)
                {
                    add_move(move_list, encode_move(source_square, target_square, P, Q, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, R, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, B, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, N, 0, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, P, 0, 0, 0, 0, 0));
                    // Double Push
                    if ((source_square >= a2 && source_square <= h2) && !get_bit(occupancies[both], target_square - 8))
                    {
                        add_move(move_list, encode_move(source_square, target_square - 8, P, 0, 0, 1, 0, 0));
                    }
                }
            }

            // Captures
            attacks = pawn_attacks[white][source_square] & occupancies[black];
            while (attacks)
            {
                target_square = get_ls1b_index(attacks);
                if (source_square >= a7 && source_square <= h7)
                {
                    add_move(move_list, encode_move(source_square, target_square, P, Q, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, R, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, B, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, N, 1, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, P, 0, 1, 0, 0, 0));
                }
                pop_bit(attacks, target_square);
            }

            // En Passant
            if (en_passant != no_sq)
            {
                U64 enpassant_attacks = pawn_attacks[white][source_square] & (1ULL << en_passant);
                if (enpassant_attacks)
                {
                    int target_enpassant = get_ls1b_index(enpassant_attacks);
                    add_move(move_list, encode_move(source_square, target_enpassant, P, 0, 1, 0, 1, 0));
                }
            }
            pop_bit(bitboard, source_square);
        }
    }
    else
    { // BLACK PAWNS
        bitboard = bitboards[p];
        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);
            target_square = source_square + 8;

            if (!(target_square > h1) && !get_bit(occupancies[both], target_square))
            {
                if (source_square >= a2 && source_square <= h2)
                {
                    add_move(move_list, encode_move(source_square, target_square, p, q, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, r, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, b, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, n, 0, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, p, 0, 0, 0, 0, 0));
                    if ((source_square >= a7 && source_square <= h7) && !get_bit(occupancies[both], target_square + 8))
                    {
                        add_move(move_list, encode_move(source_square, target_square + 8, p, 0, 0, 1, 0, 0));
                    }
                }
            }
            attacks = pawn_attacks[black][source_square] & occupancies[white];
            while (attacks)
            {
                target_square = get_ls1b_index(attacks);
                if (source_square >= a2 && source_square <= h2)
                {
                    add_move(move_list, encode_move(source_square, target_square, p, q, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, r, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, b, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, n, 1, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, p, 0, 1, 0, 0, 0));
                }
                pop_bit(attacks, target_square);
            }
            if (en_passant != no_sq)
            {
                U64 enpassant_attacks = pawn_attacks[black][source_square] & (1ULL << en_passant);
                if (enpassant_attacks)
                {
                    int target_enpassant = get_ls1b_index(enpassant_attacks);
                    add_move(move_list, encode_move(source_square, target_enpassant, p, 0, 1, 0, 1, 0));
                }
            }
            pop_bit(bitboard, source_square);
        }
    }

    // -------------------------------------------- \\
    //               CASTLING MOVES                 \\
    // -------------------------------------------- \\

    if (side == white)
    {
        // King Side (e1 to g1)
        if (castle & wk)
        {
            // Check empty squares (f1, g1) and safety (e1, f1, g1 not attacked)
            if (!get_bit(occupancies[both], f1) && !get_bit(occupancies[both], g1))
            {
                if (!is_square_attacked(e1, black) && !is_square_attacked(f1, black) && !is_square_attacked(g1, black))
                { // Actually e1 check is redundant if we assume legal state, but good for safety
                    add_move(move_list, encode_move(e1, g1, K, 0, 0, 0, 0, 1));
                }
            }
        }
        // Queen Side (e1 to c1)
        if (castle & wq)
        {
            if (!get_bit(occupancies[both], d1) && !get_bit(occupancies[both], c1) && !get_bit(occupancies[both], b1))
            {
                if (!is_square_attacked(e1, black) && !is_square_attacked(d1, black) && !is_square_attacked(c1, black))
                { // c1/d1 safe logic varies by rules, usually target and cross must be safe.
                    add_move(move_list, encode_move(e1, c1, K, 0, 0, 0, 0, 1));
                }
            }
        }
    }
    else
    {
        // Black King Side
        if (castle & bk)
        {
            if (!get_bit(occupancies[both], f8) && !get_bit(occupancies[both], g8))
            {
                if (!is_square_attacked(e8, white) && !is_square_attacked(f8, white) && !is_square_attacked(g8, white))
                {
                    add_move(move_list, encode_move(e8, g8, k, 0, 0, 0, 0, 1));
                }
            }
        }
        // Black Queen Side
        if (castle & bq)
        {
            if (!get_bit(occupancies[both], d8) && !get_bit(occupancies[both], c8) && !get_bit(occupancies[both], b8))
            {
                if (!is_square_attacked(e8, white) && !is_square_attacked(d8, white) && !is_square_attacked(c8, white))
                {
                    add_move(move_list, encode_move(e8, c8, k, 0, 0, 0, 0, 1));
                }
            }
        }
    }

    // -------------------------------------------- \\
    //              PIECE MOVES                     \\
    // -------------------------------------------- \\

    // Loop for all piece types (Knight, Bishop, Rook, Queen, King)
    int p_start = (side == white) ? N : n;
    int p_end = (side == white) ? K : k;

    for (int piece = p_start; piece <= p_end; piece++)
    {
        bitboard = bitboards[piece];

        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);

            // 1. Get attacks based on piece type
            if (side == white)
            {
                if (piece == N)
                    attacks = knight_attacks[source_square];
                else if (piece == B)
                    attacks = get_bishop_attacks_magic(source_square, occupancies[both]);
                else if (piece == R)
                    attacks = get_rook_attacks_magic(source_square, occupancies[both]);
                else if (piece == Q)
                    attacks = get_queen_attacks(source_square, occupancies[both]);
                else if (piece == K)
                    attacks = king_attacks[source_square];

                // Remove own pieces from attack squares (Can't capture self)
                attacks &= ~occupancies[white];
            }
            else
            {
                if (piece == n)
                    attacks = knight_attacks[source_square];
                else if (piece == b)
                    attacks = get_bishop_attacks_magic(source_square, occupancies[both]);
                else if (piece == r)
                    attacks = get_rook_attacks_magic(source_square, occupancies[both]);
                else if (piece == q)
                    attacks = get_queen_attacks(source_square, occupancies[both]);
                else if (piece == k)
                    attacks = king_attacks[source_square];

                attacks &= ~occupancies[black];
            }

            // 2. Add moves
            while (attacks)
            {
                target_square = get_ls1b_index(attacks);

                // Check if it's a capture
                int capture = get_bit(occupancies[(!side) ? black : white], target_square) ? 1 : 0;

                // Add move
                add_move(move_list, encode_move(source_square, target_square, piece, 0, capture, 0, 0, 0));

                pop_bit(attacks, target_square);
            }

            pop_bit(bitboard, source_square);
        }
    }
}

// ============================================ \\
//    POLYGLOT OPENING BOOK IMPLEMENTATIONS     \\
// ============================================ \\

// Polyglot random numbers for hashing (standard Polyglot randoms)
const U64 polyglot_random64[781] = {
    0x9D39247E33776D41ULL, 0x2AF7398005AAA5C7ULL, 0x44DB015024623547ULL, 0x9C15F73E62A76AE2ULL,
    0x75834465489C0C89ULL, 0x3290AC3A203001BFULL, 0x0FBBAD1F61042279ULL, 0xE83A908FF2FB60CAULL,
    0x0D7E765D58755C10ULL, 0x1A083822CEAFE02DULL, 0x9605D5F0E25EC3B0ULL, 0xD021FF5CD13A2ED5ULL,
    0x40BDF15D4A672E32ULL, 0x011355146FD56395ULL, 0x5DB4832046F3D9E5ULL, 0x239F8B2D7FF719CCULL,
    0x05D1A1AE85B49AA1ULL, 0x679F848F6E8FC971ULL, 0x7449BBFF801FED0BULL, 0x7D11CDB1C3B7ADF0ULL,
    0x82C7709E781EB7CCULL, 0xF3218F1C9510786CULL, 0x331478F3AF51BBE6ULL, 0x4BB38DE5E7219443ULL,
    0xAA649C6EBCFD50FCULL, 0x8DBD98A352AFD40BULL, 0x87D2074B81D79217ULL, 0x19F3C751D3E92AE1ULL,
    0xB4AB30F062B19ABFULL, 0x7B0500AC42047AC4ULL, 0xC9452CA81A09D85DULL, 0x24AA6C514DA27500ULL,
    0x4C9F34427501B447ULL, 0x14A68FD73C910841ULL, 0xA71B9B83461CBD93ULL, 0x03488B95B0F1850FULL,
    0x637B2B34FF93C040ULL, 0x09D1BC9A3DD90A94ULL, 0x3575668334A1DD3BULL, 0x735E2B97A4C45A23ULL,
    0x18727070F1BD400BULL, 0x1FCBACD259BF02E7ULL, 0xD310A7C2CE9B6555ULL, 0xBF983FE0FE5D8244ULL,
    0x9F74D14F7454A824ULL, 0x51EBDC4AB9BA3035ULL, 0x5C82C505DB9AB0FAULL, 0xFCF7FE8A3430B241ULL,
    0x3253A729B9BA3DDEULL, 0x8C74C368081B3075ULL, 0xB9BC6C87167C33E7ULL, 0x7EF48F2B83024E20ULL,
    0x11D505D4C351BD7FULL, 0x6568FCA92C76A243ULL, 0x4DE0B0F40F32A7B8ULL, 0x96D693460CC37E5DULL,
    0x42E240CB63689F2FULL, 0x6D2BDCDAE2919661ULL, 0x42880B0236E4D951ULL, 0x5F0F4A5898171BB6ULL,
    0x39F890F579F92F88ULL, 0x93C5B5F47356388BULL, 0x63DC359D8D231B78ULL, 0xEC16CA8AEA98AD76ULL,
    0x5355F900C2A82DC7ULL, 0x07FB9F855A997142ULL, 0x5093417AA8A7ED5EULL, 0x7BCBC38DA25A7F3CULL,
    0x19FC8A768CF4B6D4ULL, 0x637A7780DECFC0D9ULL, 0x8249A47AEE0E41F7ULL, 0x79AD695501E7D1E8ULL,
    0x14ACBAF4777D5776ULL, 0xF145B6BECCDEA195ULL, 0xDABF2AC8201752FCULL, 0x24C3C94DF9C8D3F6ULL,
    0xBB6E2924F03912EAULL, 0x0CE26C0B95C980D9ULL, 0xA49CD132BFBF7CC4ULL, 0xE99D662AF4243939ULL,
    0x27E6AD7891165C3FULL, 0x8535F040B9744FF1ULL, 0x54B3F4FA5F40D873ULL, 0x72B12C32127FED2BULL,
    0xEE954D3C7B411F47ULL, 0x9A85AC909A24EAA1ULL, 0x70AC4CD9F04F21F5ULL, 0xF9B89D3E99A075C2ULL,
    0x87B3E2B2B5C907B1ULL, 0xA366E5B8C54F48B8ULL, 0xAE4A9346CC3F7CF2ULL, 0x1920C04D47267BBDULL,
    0x87BF02C6B49E2AE9ULL, 0x092237AC237F3859ULL, 0xFF07F64EF8ED14D0ULL, 0x8DE8DCA9F03CC54EULL,
    0x9C1633264DB49C89ULL, 0xB3F22C3D0B0B38EDULL, 0x390E5FB44D01144BULL, 0x5BFEA5B4712768E9ULL,
    0x1E1032911FA78984ULL, 0x9A74ACB964E78CB3ULL, 0x4F80F7A035DAFB04ULL, 0x6304D09A0B3738C4ULL,
    0x2171E64683023A08ULL, 0x5B9B63EB9CEFF80CULL, 0x506AACF489889342ULL, 0x1881AFC9A3A701D6ULL,
    0x6503080440750644ULL, 0xDFD395339CDBF4A7ULL, 0xEF927DBCF00C20F2ULL, 0x7B32F7D1E03680ECULL,
    0xB9FD7620E7316243ULL, 0x05A7E8A57DB91B77ULL, 0xB5889C6E15630A75ULL, 0x4A750A09CE9573F7ULL,
    0xCF464CEC899A2F8AULL, 0xF538639CE705B824ULL, 0x3C79A0FF5580EF7FULL, 0xEDE6C87F8477609DULL,
    0x799E81F05BC93F31ULL, 0x86536B8CF3428A8CULL, 0x97D7374C60087B73ULL, 0xA246637CFF328532ULL,
    0x043FCAE60CC0EBA0ULL, 0x920E449535DD359EULL, 0x70EB093B15B290CCULL, 0x73A1921916591CBDULL,
    0x56436C9FE1A1AA8DULL, 0xEFAC4B70633B8F81ULL, 0xBB215798D45DF7AFULL, 0x45F20042F24F1768ULL,
    0x930F80F4E8EB7462ULL, 0xFF6712FFCFD75EA1ULL, 0xAE623FD67468AA70ULL, 0xDD2C5BC84BC8D8FCULL,
    0x7EED120D54CF2DD9ULL, 0x22FE545401165F1CULL, 0xC91800E98FB99929ULL, 0x808BD68E6AC10365ULL,
    0xDEC468145B7605F6ULL, 0x1BEDE3A3AEF53302ULL, 0x43539603D6C55602ULL, 0xAA969B5C691CCB7AULL,
    0xA87832D392EFEE56ULL, 0x65942C7B3C7E11AEULL, 0xDED2D633CAD004F6ULL, 0x21F08570F420E565ULL,
    0xB415938D7DA94E3CULL, 0x91B859E59ECB6350ULL, 0x10CFF333E0ED804AULL, 0x28AED140BE0BB7DDULL,
    0xC5CC1D89724FA456ULL, 0x5648F680F11A2741ULL, 0x2D255069F0B7DAB3ULL, 0x9BC5A38EF729ABD4ULL,
    0xEF2F054308F6A2BCULL, 0xAF2042F5CC5C2858ULL, 0x480412BAB7F5BE2AULL, 0xAEF3AF4A563DFE43ULL,
    0x19AFE59AE451497FULL, 0x52593803DFF1E840ULL, 0xF4F076E65F2CE6F0ULL, 0x11379625747D5AF3ULL,
    0xBCE5D2248682C115ULL, 0x9DA4243DE836994FULL, 0x066F70B33FE09017ULL, 0x4DC4DE189B671A1CULL,
    0x51039AB7712457C3ULL, 0xC07A3F80C31FB4B4ULL, 0xB46EE9C5E64A6E7CULL, 0xB3819A42ABE61C87ULL,
    0x21A007933A522A20ULL, 0x2DF16F761598AA4FULL, 0x763C4A1371B368FDULL, 0xF793C46702E086A0ULL,
    0xD7288E012AEB8D31ULL, 0xDE336A2A4BC1C44BULL, 0x0BF692B38D079F23ULL, 0x2C604A7A177326B3ULL,
    0x4850E73E03EB6064ULL, 0xCFC447F1E53C8E1BULL, 0xB05CA3F564268D99ULL, 0x9AE182C8BC9474E8ULL,
    0xA4FC4BD4FC5558CAULL, 0xE755178D58FC4E76ULL, 0x69B97DB1A4C03DFEULL, 0xF9B5B7C4ACC67C96ULL,
    0xFC6A82D64B8655FBULL, 0x9C684CB6C4D24417ULL, 0x8EC97D2917456ED0ULL, 0x6703DF9D2924E97EULL,
    0xC547F57E42A7444EULL, 0x78E37644E7CAD29EULL, 0xFE9A44E9362F05FAULL, 0x08BD35CC38336615ULL,
    0x9315E5EB3A129ACEULL, 0x94061B871E04DF75ULL, 0xDF1D9F9D784BA010ULL, 0x3BBA57B68871B59DULL,
    0xD2B7ADEEDED1F73FULL, 0xF7A255D83BC373F8ULL, 0xD7F4F2448C0CEB81ULL, 0xD95BE88CD210FFA7ULL,
    0x336F52F8FF4728E7ULL, 0xA74049DAC312AC71ULL, 0xA2F61BB6E437FDB5ULL, 0x4F2A5CB07F6A35B3ULL,
    0x87D380BDA5BF7859ULL, 0x16B9F7E06C453A21ULL, 0x7BA2484C8A0FD54EULL, 0xF3A678CAD9A2E38CULL,
    0x39B0BF7DDE437BA2ULL, 0xFCAF55C1BF8A4424ULL, 0x18FCF680573FA594ULL, 0x4C0563B89F495AC3ULL,
    0x40E087931A00930DULL, 0x8CFFA9412EB642C1ULL, 0x68CA39053261169FULL, 0x7A1EE967D27579E2ULL,
    0x9D1D60E5076F5B6FULL, 0x3810E399B6F65BA2ULL, 0x32095B6D4AB5F9B1ULL, 0x35CAB62109DD038AULL,
    0xA90B24499FCFAFB1ULL, 0x77A225A07CC2C6BDULL, 0x513E5E634C70E331ULL, 0x4361C0CA3F692F12ULL,
    0xD941ACA44B20A45BULL, 0x528F7C8602C5807BULL, 0x52AB92BEB9613989ULL, 0x9D1DFA2EFC557F73ULL,
    0x722FF175F572C348ULL, 0x1D1260A51107FE97ULL, 0x7A249A57EC0C9BA2ULL, 0x04208FE9E8F7F2D6ULL,
    0x5A110C6058B920A0ULL, 0x0CD9A497658A5698ULL, 0x56FD23C8F9715A4CULL, 0x284C847B9D887AAEULL,
    0x04FEABFBBDB619CBULL, 0x742E1E651C60BA83ULL, 0x9A9632E65904AD3CULL, 0x881B82A13B51B9E2ULL,
    0x506E6744CD974924ULL, 0xB0183DB56FFC6A79ULL, 0x0ED9B915C66ED37EULL, 0x5E11E86D5873D484ULL,
    0xF678647E3519AC6EULL, 0x1B85D488D0F20CC5ULL, 0xDAB9FE6525D89021ULL, 0x0D151D86ADB73615ULL,
    0xA865A54EDCC0F019ULL, 0x93C42566AEF98FFBULL, 0x99E7AFEABE000731ULL, 0x48CBFF086DDF285AULL,
    0x7F9B6AF1EBF78BAFULL, 0x58627E1A149BBA21ULL, 0x2CD16E2ABD791E33ULL, 0xD363EFF5F0977996ULL,
    0x0CE2A38C344A6EEDULL, 0x1A804AADB9CFA741ULL, 0x907F30421D78C5DEULL, 0x501F65EDB3034D07ULL,
    0x37624AE5A48FA6E9ULL, 0x957BAF61700CFF4EULL, 0x3A6C27934E31188AULL, 0xD49503536ABCA345ULL,
    0x088E049589C432E0ULL, 0xF943AEE7FEBF21B8ULL, 0x6C3B8E3E336139D3ULL, 0x364F6FFA464EE52EULL,
    0xD60F6DCEDC314222ULL, 0x56963B0DCA418FC0ULL, 0x16F50EDF91E513AFULL, 0xEF1955914B609F93ULL,
    0x565601C0364E3228ULL, 0xECB53939887E8175ULL, 0xBAC7A9A18531294BULL, 0xB344C470397BBA52ULL,
    0x65D34954DAF3CEBDULL, 0xB4B81B3FA97511E2ULL, 0xB422061193D6F6A7ULL, 0x071582401C38434DULL,
    0x7A13F18BBEDC4FF5ULL, 0xBC4097B116C524D2ULL, 0x59B97885E2F2EA28ULL, 0x99170A5DC3115544ULL,
    0x6F423357E7C6A9F9ULL, 0x325928EE6E6F8794ULL, 0xD0E4366228B03343ULL, 0x565C31F7DE89EA27ULL,
    0x30F5611484119414ULL, 0xD873DB391292ED4FULL, 0x7BD94E1D8E17DEBCULL, 0xC7D9F16864A76E94ULL,
    0x947AE053EE56E63CULL, 0xC8C93882F9475F5FULL, 0x3A9BF55BA91F81CAULL, 0xD9A11FBB3D9808E4ULL,
    0x0FD22063EDC29FCAULL, 0xB3F256D8ACA0B0B9ULL, 0xB03031A8B4516E84ULL, 0x35DD37D5871448AFULL,
    0xE9F6082B05542E4EULL, 0xEBFAFA33D7254B59ULL, 0x9255ABB50D532280ULL, 0xB9AB4CE57F2D34F3ULL,
    0x693501D628297551ULL, 0xC62C58F97DD949BFULL, 0xCD454F8F19C5126AULL, 0xBBE83F4ECC2BDECBULL,
    0xDC842B7E2819E230ULL, 0xBA89142E007503B8ULL, 0xA3BC941D0A5061CBULL, 0xE9F6760E32CD8021ULL,
    0x09C7E552BC76492FULL, 0x852F54934DA55CC9ULL, 0x8107FCCF064FCF56ULL, 0x098954D51FFF6580ULL,
    0x23B70EDB1955C4BFULL, 0xC330DE426430F69DULL, 0x4715ED43E8A45C0AULL, 0xA8D7E4DAB780A08DULL,
    0x0572B974F03CE0BBULL, 0xB57D2E985E1419C7ULL, 0xE8D9ECBE2CF3D73FULL, 0x2FE4B17170E59750ULL,
    0x11317BA87905E790ULL, 0x7FBF21EC8A1F45ECULL, 0x1725CABFCB045B00ULL, 0x964E915CD5E2B207ULL,
    0x3E2B8BCBF016D66DULL, 0xBE7444E39328A0ACULL, 0xF85B2B4FBCDE44B7ULL, 0x49353FEA39BA63B1ULL,
    0x1DD01AAFCD53486AULL, 0x1FCA8A92FD719F85ULL, 0xFC7C95D827357AFAULL, 0x18A6A990C8B35EBDULL,
    0xCCCB7005C6B9C28DULL, 0x3BDBB92C43B17F26ULL, 0xAA70B5B4F89695A2ULL, 0xE94C39A54A98307FULL,
    0xB7A0B174CFF6F36EULL, 0xD4DBA84729AF48ADULL, 0x2E18BC1AD9704A68ULL, 0x2DE0966DAF2F8B1CULL,
    0xB9C11D5B1E43A07EULL, 0x64972D68DEE33360ULL, 0x94628D38D0C20584ULL, 0xDBC0D2B6AB90A559ULL,
    0xD2733C4335C6A72FULL, 0x7E75D99D94A70F4DULL, 0x6CED1983376FA72BULL, 0x97FCAACBF030BC24ULL,
    0x7B77497B32503B12ULL, 0x8547EDDFB81CCB94ULL, 0x79999CDFF70902CBULL, 0xCFFE1939438E9B24ULL,
    0x829626E3892D95D7ULL, 0x92FAE24291F2B3F1ULL, 0x63E22C147B9C3403ULL, 0xC678B6D860284A1CULL,
    0x5873888850659AE7ULL, 0x0981DCD296A8736DULL, 0x9F65789A6509A440ULL, 0x9FF38FED72E9052FULL,
    0xE479EE5B9930578CULL, 0xE7F28ECD2D49EECDULL, 0x56C074A581EA17FEULL, 0x5544F7D774B14AEFULL,
    0x7B3F0195FC6F290FULL, 0x12153635B2C0CF57ULL, 0x7F5126DBBA5E0CA7ULL, 0x7A76956C3EAFB413ULL,
    0x3D5774A11D31AB39ULL, 0x8A1B083821F40CB4ULL, 0x7B4A38E32537DF62ULL, 0x950113646D1D6E03ULL,
    0x4DA8979A0041E8A9ULL, 0x3BC36E078F7515D7ULL, 0x5D0A12F27AD310D1ULL, 0x7F9D1A2E1EBE1327ULL,
    0xDA3A361B1C5157B1ULL, 0xDCDD7D20903D0C25ULL, 0x36833336D068F707ULL, 0xCE68341F79893389ULL,
    0xAB9090168DD05F34ULL, 0x43954B3252DC25E5ULL, 0xB438C2B67F98E5E9ULL, 0x10DCD78E3851A492ULL,
    0xDBC27AB5447822BFULL, 0x9B3CDB65F82CA382ULL, 0xB67B7896167B4C84ULL, 0xBFCED1B0048EAC50ULL,
    0xA9119B60369FFEBDULL, 0x1FFF7AC80904BF45ULL, 0xAC12FB171817EEE7ULL, 0xAF08DA9177DDA93DULL,
    0x1B0CAB936E65C744ULL, 0xB559EB1D04E5E932ULL, 0xC37B45B3F8D6F2BAULL, 0xC3A9DC228CAAC9E9ULL,
    0xF3B8B6675A6507FFULL, 0x9FC477DE4ED681DAULL, 0x67378D8ECCEF96CBULL, 0x6DD856D94D259236ULL,
    0xA319CE15B0B4DB31ULL, 0x073973751F12DD5EULL, 0x8A8E849EB32781A5ULL, 0xE1925C71285279F5ULL,
    0x74C04BF1790C0EFEULL, 0x4DDA48153C94938AULL, 0x9D266D6A1CC0542CULL, 0x7440FB816508C4FEULL,
    0x13328503DF48229FULL, 0xD6BF7BAEE43CAC40ULL, 0x4838D65F6EF6748FULL, 0x1E152328F3318DEAULL,
    0x8F8419A348F296BFULL, 0x72C8834A5957B511ULL, 0xD7A023A73260B45CULL, 0x94EBC8ABCFB56DAEULL,
    0x9FC10D0F989993E0ULL, 0xDE68A2355B93CAE6ULL, 0xA44CFE79AE538BBEULL, 0x9D1D84FCCE371425ULL,
    0x51D2B1AB2DDFB636ULL, 0x2FD7E4B9E72CD38CULL, 0x65CA5B96B7552210ULL, 0xDD69A0D8AB3B546DULL,
    0x604D51B25FBF70E2ULL, 0x73AA8A564FB7AC9EULL, 0x1A8C1E992B941148ULL, 0xAAC40A2703D9BEA0ULL,
    0x764DBEAE7FA4F3A6ULL, 0x1E99B96E70A9BE8BULL, 0x2C5E9DEB57EF4743ULL, 0x3A938FEE32D29981ULL,
    0x26E6DB8FFDF5ADFEULL, 0x469356C504EC9F9DULL, 0xC8763C5B08D1908CULL, 0x3F6C6AF859D80055ULL,
    0x7F7CC39420A3A545ULL, 0x9BFB227EBDF4C5CEULL, 0x89039D79D6FC5C5CULL, 0x8FE88B57305E2AB6ULL,
    0xA09E8C8C35AB96DEULL, 0xFA7E393983325753ULL, 0xD6B6D0ECC617C699ULL, 0xDFEA21EA9E7557E3ULL,
    0xB67C1FA481680AF8ULL, 0xCA1E3785A9E724E5ULL, 0x1CFC8BED0D681639ULL, 0xD18D8549D140CAEAULL,
    0x4ED0FE7E9DC91335ULL, 0xE4DBF0634473F5D2ULL, 0x1761F93A44D5AEFEULL, 0x53898E4C3910DA55ULL,
    0x734DE8181F6EC39AULL, 0x2680B122BAA28D97ULL, 0x298AF231C85BAFABULL, 0x7983EED3740847D5ULL,
    0x66C1A2A1A60CD889ULL, 0x9E17E49642A3E4C1ULL, 0xEDB454E7BADC0805ULL, 0x50B704CAB602C329ULL,
    0x4CC317FB9CDDD023ULL, 0x66B4835D9EAFEA22ULL, 0x219B97E26FFC81BDULL, 0x261E4E4C0A333A9DULL,
    0x1FE2CCA76517DB90ULL, 0xD7504DFA8816EDBBULL, 0xB9571FA04DC089C8ULL, 0x1DDC0325259B27DEULL,
    0xCF3F4688801EB9AAULL, 0xF4F5D05C10CAB243ULL, 0x38B6525C21A42B0EULL, 0x36F60E2BA4FA6800ULL,
    0xEB3593803173E0CEULL, 0x9C4CD6257C5A3603ULL, 0xAF0C317D32ADAA8AULL, 0x258E5A80C7204C4BULL,
    0x8B889D624D44885DULL, 0xF4D14597E660F855ULL, 0xD4347F66EC8941C3ULL, 0xE699ED85B0DFB40DULL,
    0x2472F6207C2D0484ULL, 0xC2A1E7B5B459AEB5ULL, 0xAB4F6451CC1D45ECULL, 0x63767572AE3D6174ULL,
    0xA59E0BD101731A28ULL, 0x116D0016CB948F09ULL, 0x2CF9C8CA052F6E9FULL, 0x0B090A7560A968E3ULL,
    0xABEEDDB2DDE06FF1ULL, 0x58EFC10B06A2068DULL, 0xC6E57A78FBD986E0ULL, 0x2EAB8CA63CE802D7ULL,
    0x14A195640116F336ULL, 0x7C0828DD624EC390ULL, 0xD74BBE77E6116AC7ULL, 0x804456AF10F5FB53ULL,
    0xEBE9EA2ADF4321C7ULL, 0x03219A39EE587A30ULL, 0x49787FEF17AF9924ULL, 0xA1E9300CD8520548ULL,
    0x5B45E522E4B1B4EFULL, 0xB49C3B3995091A36ULL, 0xD4490AD526F14431ULL, 0x12A8F216AF9418C2ULL,
    0x001F837CC7350524ULL, 0x1877B51E57A764D5ULL, 0xA2853B80F17F58EEULL, 0x993E1DE72D36D310ULL,
    0xB3598080CE64A656ULL, 0x252F59CF0D9F04BBULL, 0xD23C8E176D113600ULL, 0x1BDA0492E7E4586EULL,
    0x21E0BD5026C619BFULL, 0x3B097ADAF088F94EULL, 0x8D14DEDB30BE846EULL, 0xF95CFFA23AF5F6F4ULL,
    0x3871700761B3F743ULL, 0xCA672B91E9E4FA16ULL, 0x64C8E531BFF53B55ULL, 0x241260ED4AD1E87DULL,
    0x106C09B972D2E822ULL, 0x7FBA195410E5CA30ULL, 0x7884D9BC6CB569D8ULL, 0x0647DFEDCD894A29ULL,
    0x63573FF03E224774ULL, 0x4FC8E9560F91B123ULL, 0x1DB956E450275779ULL, 0xB8D91274B9E9D4FBULL,
    0xA2EBEE47E2FBFCE1ULL, 0xD9F1F30CCD97FB09ULL, 0xEFED53D75FD64E6BULL, 0x2E6D02C36017F67FULL,
    0xA9AA4D20DB084E9BULL, 0xB64BE8D8B25396C1ULL, 0x70CB6AF7C2D5BCF0ULL, 0x98F076A4F7A2322EULL,
    0xBF84470805E69B5FULL, 0x94C3251F06F90CF3ULL, 0x3E003E616A6591E9ULL, 0xB925A6CD0421AFF3ULL,
    0x61BDD1307C66E300ULL, 0xBF8D5108E27E0D48ULL, 0x240AB57A8B888B20ULL, 0xFC87614BAF287E07ULL,
    0xEF02CDD06FFDB432ULL, 0xA1082C0466DF6C0AULL, 0x8215E577001332C8ULL, 0xD39BB9C3A48DB6CFULL,
    0x2738259634305C14ULL, 0x61CF4F94C97DF93DULL, 0x1B6BACA2AE4E125BULL, 0x758F450C88572E0BULL,
    0x959F587D507A8359ULL, 0xB063E962E045F54DULL, 0x60E8ED72C0DFF5D1ULL, 0x7B64978555326F9FULL,
    0xFD080D236DA814BAULL, 0x8C90FD9B083F4558ULL, 0x106F72FE81E2C590ULL, 0x7976033A39F7D952ULL,
    0xA4EC0132764CA04BULL, 0x733EA705FAE4FA77ULL, 0xB4D8F77BC3E56167ULL, 0x9E21F4F903B33FD9ULL,
    0x9D765E419FB69F6DULL, 0xD30C088BA61EA5EFULL, 0x5D94337FBFAF7F5BULL, 0x1A4E4822EB4D7A59ULL,
    0x6FFE73E81B637FB3ULL, 0xDDF957BC36D8B9CAULL, 0x64D0E29EEA8838B3ULL, 0x08DD9BDFD96B9F63ULL,
    0x087E79E5A57D1D13ULL, 0xE328E230E3E2B3FBULL, 0x1C2559E30F0946BEULL, 0x720BF5F26F4D2EAAULL,
    0xB0774D261CC609DBULL, 0x443F64EC5A371195ULL, 0x4112CF68649A260EULL, 0xD813F2FAB7F5C5CAULL,
    0x660D3257380841EEULL, 0x59AC2C7873F910A3ULL, 0xE846963877671A17ULL, 0x93B633ABFA3469F8ULL,
    0xC0C0F5A60EF4CDCFULL, 0xCAF21ECD4377B28CULL, 0x57277707199B8175ULL, 0x506C11B9D90E8B1DULL,
    0xD83CC2687A19255FULL, 0x4A29C6465A314CD1ULL, 0xED2DF21216235097ULL, 0xB5635C95FF7296E2ULL,
    0x22AF003AB672E811ULL, 0x52E762596BF68235ULL, 0x9AEBA33AC6ECC6B0ULL, 0x944F6DE09134DFB6ULL,
    0x6C47BEC883A7DE39ULL, 0x6AD047C430A12104ULL, 0xA5B1CFDBA0AB4067ULL, 0x7C45D833AFF07862ULL,
    0x5092EF950A16DA0BULL, 0x9338E69C052B8E7BULL, 0x455A4B4CFE30E3F5ULL, 0x6B02E63195AD0CF8ULL,
    0x6B17B224BAD6BF27ULL, 0xD1E0CCD25BB9C169ULL, 0xDE0C89A556B9AE70ULL, 0x50065E535A213CF6ULL,
    0x9C1169FA2777B874ULL, 0x78EDEFD694AF1EEDULL, 0x6DC93D9526A50E68ULL, 0xEE97F453F06791EDULL,
    0x32AB0EDB696703D3ULL, 0x3A6853C7E70757A7ULL, 0x31865CED6120F37DULL, 0x67FEF95D92607890ULL,
    0x1F2B1D1F15F6DC9CULL, 0xB69E38A8965C6B65ULL, 0xAA9119FF184CCCF4ULL, 0xF43C732873F24C13ULL,
    0xFB4A3D794A9A80D2ULL, 0x3550C2321FD6109CULL, 0x371F77E76BB8417EULL, 0x6BFA9AAE5EC05779ULL,
    0xCD04F3FF001A4778ULL, 0xE3273522064480CAULL, 0x9F91508BFFCFC14AULL, 0x049A7F41061A9E60ULL,
    0xFCB6BE43A9F2FE9BULL, 0x08DE8A1C7797DA9BULL, 0x8F9887E6078735A1ULL, 0xB5B4071DBFC73A66ULL,
    0x230E343DFBA08D33ULL, 0x43ED7F5A0FAE657DULL, 0x3A88A0FBBCB05C63ULL, 0x21874B8B4D2DBC4FULL,
    0x1BDEA12E35F6A8C9ULL, 0x53C065C6C8E63528ULL, 0xE34A1D250E7A8D6BULL, 0xD6B04D3B7651DD7EULL,
    0x5E90277E7CB39E2DULL, 0x2C046F22062DC67DULL, 0xB10BB459132D0A26ULL, 0x3FA9DDFB67E2F199ULL,
    0x0E09B88E1914F7AFULL, 0x10E8B35AF3EEAB37ULL, 0x9EEDECA8E272B933ULL, 0xD4C718BC4AE8AE5FULL,
    0x81536D601170FC20ULL, 0x91B534F885818A06ULL, 0xEC8177F83F900978ULL, 0x190E714FADA5156EULL,
    0xB592BF39B0364963ULL, 0x89C350C893AE7DC1ULL, 0xAC042E70F8B383F2ULL, 0xB49B52E587A1EE60ULL,
    0xFB152FE3FF26DA89ULL, 0x3E666E6F69AE2C15ULL, 0x3B544EBE544C19F9ULL, 0xE805A1E290CF2456ULL,
    0x24B33C9D7ED25117ULL, 0xE74733427B72F0C1ULL, 0x0A804D18B7097475ULL, 0x57E3306D881EDB4FULL,
    0x4AE7D6A36EB5DBCBULL, 0x2D8D5432157064C8ULL, 0xD1E649DE1E7F268BULL, 0x8A328A1CEDFE552CULL,
    0x07A3AEC79624C7DAULL, 0x84547DDC3E203C94ULL, 0x990A98FD5071D263ULL, 0x1A4FF12616EEFC89ULL,
    0xF6F7FD1431714200ULL, 0x30C05B1BA332F41CULL, 0x8D2636B81555A786ULL, 0x46C9FEB55D120902ULL,
    0xCCEC0A73B49C9921ULL, 0x4E9D2827355FC492ULL, 0x19EBB029435DCB0FULL, 0x4659D2B743848A2CULL,
    0x963EF2C96B33BE31ULL, 0x74F85198B05A2E7DULL, 0x5A0F544DD2B1FB18ULL, 0x03727073C2E134B1ULL,
    0xC7F6AA2DE59AEA61ULL, 0x352787BAA0D7C22FULL, 0x9853EAB63B5E0B35ULL, 0xABBDCDD7ED5C0860ULL,
    0xCF05DAF5AC8D77B0ULL, 0x49CAD48CEBF4A71EULL, 0x7A4C10EC2158C4A6ULL, 0xD9E92AA246BF719EULL,
    0x13AE978D09FE5557ULL, 0x730499AF921549FFULL, 0x4E4B705B92903BA4ULL, 0xFF577222C14F0A3AULL,
    0x55B6344CF97AAFAEULL, 0xB862225B055B6960ULL, 0xCAC09AFBDDD2CDB4ULL, 0xDAF8E9829FE96B5FULL,
    0xB5FDFC5D3132C498ULL, 0x310CB380DB6F7503ULL, 0xE87FBB46217A360EULL, 0x2102AE466EBB1148ULL,
    0xF8549E1A3AA5E00DULL, 0x07A69AFDCC42261AULL, 0xC4C118BFE78FEAAEULL, 0xF9F4892ED96BD438ULL,
    0x1AF3DBE25D8F45DAULL, 0xF5B4B0B0D2DEEEB4ULL, 0x962ACEEFA82E1C84ULL, 0x046E3ECAAF453CE9ULL,
    0xF05D129681949A4CULL, 0x964781CE734B3C84ULL, 0x9C2ED44081CE5FBDULL, 0x522E23F3925E319EULL,
    0x177E00F9FC32F791ULL, 0x2BC60A63A6F3B3F2ULL, 0x222BBFAE61725606ULL, 0x486289DDCC3D6780ULL,
    0x7DC7785B8EFDFC80ULL, 0x8AF38731C02BA980ULL, 0x1FAB64EA29A2DDF7ULL, 0xE4D9429322CD065AULL,
    0x9DA058C67844F20CULL, 0x24C0E332B70019B0ULL, 0x233003B5A6CFE6ADULL, 0xD586BD01C5C217F6ULL,
    0x5E5637885F29BC2BULL, 0x7EBA726D8C94094BULL, 0x0A56A5F0BFE39272ULL, 0xD79476A84EE20D06ULL,
    0x9E4C1269BAA4BF37ULL, 0x17EFEE45B0DEE640ULL, 0x1D95B0A5FCF90BC6ULL, 0x93CBE0B699C2585DULL,
    0x65FA4F227A2B6D79ULL, 0xD5F9E858292504D5ULL, 0xC2B5A03F71471A6FULL, 0x59300222B4561E00ULL,
    0xCE2F8642CA0712DCULL, 0x7CA9723FBB2E8988ULL, 0x2785338347F2BA08ULL, 0xC61BB3A141E50E8CULL,
    0x150F361DAB9DEC26ULL, 0x9F6A419D382595F4ULL, 0x64A53DC924FE7AC9ULL, 0x142DE49FFF7A7C3DULL,
    0x0C335248857FA9E7ULL, 0x0A9C32D5EAE45305ULL, 0xE6C42178C4BBB92EULL, 0x71F1CE2490D20B07ULL,
    0xF1BCC3D275AFE51AULL, 0xE728E8C83C334074ULL, 0x96FBF83A12884624ULL, 0x81A1549FD6573DA5ULL,
    0x5FA7867CAF35E149ULL, 0x56986E2EF3ED091BULL, 0x917F1DD5F8886C61ULL, 0xD20D8C88C8FFE65FULL,
    0x31D71DCE64B2C310ULL, 0xF165B587DF898190ULL, 0xA57E6339DD2CF3A0ULL, 0x1EF6E6DBB1961EC9ULL,
    0x70CC73D90BC26E24ULL, 0xE21A6B35DF0C3AD7ULL, 0x003A93D8B2806962ULL, 0x1C99DED33CB890A1ULL,
    0xCF3145DE0ADD4289ULL, 0xD0E4427A5514FB72ULL, 0x77C621CC9FB3A483ULL, 0x67A34DAC4356550BULL,
    0xF8D626AAAF278509ULL};

// Generate Polyglot hash key (different from our Zobrist key!)
U64 get_polyglot_key()
{
    U64 key = 0ULL;

    // Polyglot piece indexing: bp=0, wp=1, bn=2, wn=3, bb=4, wb=5, br=6, wr=7, bq=8, wq=9, bk=10, wk=11
    // Our pieces: P=0, N=1, B=2, R=3, Q=4, K=5, p=6, n=7, b=8, r=9, q=10, k=11
    static const int poly_piece_map[12] = {
        1,  // P (white pawn) -> wp=1
        3,  // N (white knight) -> wn=3
        5,  // B (white bishop) -> wb=5
        7,  // R (white rook) -> wr=7
        9,  // Q (white queen) -> wq=9
        11, // K (white king) -> wk=11
        0,  // p (black pawn) -> bp=0
        2,  // n (black knight) -> bn=2
        4,  // b (black bishop) -> bb=4
        6,  // r (black rook) -> br=6
        8,  // q (black queen) -> bq=8
        10  // k (black king) -> bk=10
    };

    // Pieces on squares
    for (int piece = P; piece <= k; piece++)
    {
        U64 bb = bitboards[piece];
        while (bb)
        {
            int sq = get_ls1b_index(bb);
            // Convert our square to Polyglot square (flip rank)
            // Our square: a1=0, h1=7, a8=56, h8=63
            // Polyglot square: a1=0, h1=7, a8=56, h8=63 (same, but rank from white's view)
            int poly_sq = sq ^ 56; // Flip rank: sq XOR 56 flips rank (0-7 -> 7-0)
            int poly_piece = poly_piece_map[piece];
            key ^= polyglot_random64[64 * poly_piece + poly_sq];
            pop_bit(bb, sq);
        }
    }

    // Castling rights
    if (castle & wk)
        key ^= polyglot_random64[768];
    if (castle & wq)
        key ^= polyglot_random64[769];
    if (castle & bk)
        key ^= polyglot_random64[770];
    if (castle & bq)
        key ^= polyglot_random64[771];

    // En passant (only if capture is possible)
    if (en_passant != no_sq)
    {
        int ep_file = en_passant % 8;
        key ^= polyglot_random64[772 + ep_file];
    }

    // Side to move: In Polyglot, key is XORed when WHITE is to move
    if (side == white)
        key ^= polyglot_random64[780];

    return key;
}

// Load Polyglot opening book
int load_opening_book(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        printf("info string Opening book not found: %s\n", filename);
        return 0;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    book_entries = size / 16; // Each entry is 16 bytes
    if (book_entries > BOOK_MAX_ENTRIES)
        book_entries = BOOK_MAX_ENTRIES;

    opening_book = (PolyglotEntry *)malloc(book_entries * sizeof(PolyglotEntry));
    if (!opening_book)
    {
        fclose(f);
        return 0;
    }

    // Read entries (Polyglot is big-endian!)
    for (int i = 0; i < book_entries; i++)
    {
        unsigned char data[16];
        if (fread(data, 1, 16, f) != 16)
            break;

        // Convert from big-endian
        opening_book[i].key = ((U64)data[0] << 56) | ((U64)data[1] << 48) |
                              ((U64)data[2] << 40) | ((U64)data[3] << 32) |
                              ((U64)data[4] << 24) | ((U64)data[5] << 16) |
                              ((U64)data[6] << 8) | (U64)data[7];
        opening_book[i].move = (data[8] << 8) | data[9];
        opening_book[i].weight = (data[10] << 8) | data[11];
        opening_book[i].learn = (data[12] << 24) | (data[13] << 16) |
                                (data[14] << 8) | data[15];
    }

    fclose(f);
    printf("info string Loaded %d book entries from %s\n", book_entries, filename);
    return 1;
}

// Binary search in opening book
int find_book_entry(U64 key)
{
    if (!opening_book || book_entries == 0)
        return -1;

    int low = 0, high = book_entries - 1;
    while (low <= high)
    {
        int mid = (low + high) / 2;
        if (opening_book[mid].key < key)
            low = mid + 1;
        else if (opening_book[mid].key > key)
            high = mid - 1;
        else
            return mid;
    }
    return -1;
}

// Convert Polyglot move to our internal format
int polyglot_to_move(unsigned short poly_move)
{
    // Polyglot move encoding:
    // bits 0-5: destination square
    // bits 6-11: origin square
    // bits 12-14: promotion piece (0=none, 1=knight, 2=bishop, 3=rook, 4=queen)

    int to_file = poly_move & 7;
    int to_rank = (poly_move >> 3) & 7;
    int from_file = (poly_move >> 6) & 7;
    int from_rank = (poly_move >> 9) & 7;
    int promo = (poly_move >> 12) & 7;

    // Convert to our square format (a8=0)
    int from_sq = (7 - from_rank) * 8 + from_file;
    int to_sq = (7 - to_rank) * 8 + to_file;

    // Generate all legal moves and find matching one
    moves move_list[1];
    generate_moves(move_list);

    for (int i = 0; i < move_list->count; i++)
    {
        int move = move_list->moves[i];
        if (get_move_source(move) == from_sq && get_move_target(move) == to_sq)
        {
            // Check promotion
            if (promo)
            {
                int our_promo = get_move_promoted(move);
                int promo_type = (our_promo == N || our_promo == n) ? 1 : (our_promo == B || our_promo == b) ? 2
                                                                      : (our_promo == R || our_promo == r)   ? 3
                                                                      : (our_promo == Q || our_promo == q)   ? 4
                                                                                                             : 0;
                if (promo_type != promo)
                    continue;
            }

            // Verify move is legal
            copy_board();
            if (make_move(move, all_moves))
            {
                take_back();
                return move;
            }
            take_back();
        }
    }
    return 0;
}

// Get a move from the opening book
int get_book_move()
{
    if (!use_book || !opening_book || book_entries == 0)
        return 0;

    U64 key = get_polyglot_key();
    int idx = find_book_entry(key);

    if (idx < 0)
        return 0;

    // Collect all moves for this position
    int candidate_moves[64];
    int candidate_weights[64];
    int num_candidates = 0;
    int total_weight = 0;

    // Go backwards to find first entry with this key
    while (idx > 0 && opening_book[idx - 1].key == key)
        idx--;

    // Collect all moves
    while (idx < book_entries && opening_book[idx].key == key && num_candidates < 64)
    {
        int move = polyglot_to_move(opening_book[idx].move);
        if (move)
        {
            candidate_moves[num_candidates] = move;
            candidate_weights[num_candidates] = opening_book[idx].weight;
            total_weight += opening_book[idx].weight;
            num_candidates++;
        }
        idx++;
    }

    if (num_candidates == 0)
        return 0;

    // Weighted random selection
    if (total_weight > 0)
    {
        int r = rand() % total_weight;
        int cumulative = 0;
        for (int i = 0; i < num_candidates; i++)
        {
            cumulative += candidate_weights[i];
            if (r < cumulative)
                return candidate_moves[i];
        }
    }

    // Fallback to first move
    return candidate_moves[0];
}

// Free opening book memory
void free_opening_book()
{
    if (opening_book)
    {
        free(opening_book);
        opening_book = NULL;
        book_entries = 0;
    }
}

// Make move function
// move: The encoded move integer
// move_flag: 0 = all moves, 1 = only captures (for later)
// Returns: 1 if legal, 0 if illegal (king left in check)

int make_move(int move, int move_flag)
{
    // 1. Quiet Moves check
    if (move_flag == all_moves)
    {
        // Continue
    }
    else
    {
        if (!get_move_capture(move))
            return 0;
    }

    // 2. Backup Board
    copy_board();

    // 3. Parse Move
    int source_square = get_move_source(move);
    int target_square = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted_piece = get_move_promoted(move);
    int capture = get_move_capture(move);
    int double_push = get_move_double(move);
    int enpass = get_move_enpassant(move);
    int castling = get_move_castling(move);

    // 4. Move Piece & Update Hash
    pop_bit(bitboards[piece], source_square);
    set_bit(bitboards[piece], target_square);

    // HASH UPDATE: Position Change
    hash_key ^= piece_keys[piece][source_square]; // Remove from source
    hash_key ^= piece_keys[piece][target_square]; // Add to target

    // 5. Handle Captures
    if (capture)
    {
        int start_piece, end_piece;
        if (side == white)
        {
            start_piece = p;
            end_piece = k;
        }
        else
        {
            start_piece = P;
            end_piece = K;
        }

        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            if (get_bit(bitboards[bb_piece], target_square))
            {
                pop_bit(bitboards[bb_piece], target_square);

                // HASH UPDATE: Remove captured piece
                hash_key ^= piece_keys[bb_piece][target_square];
                break;
            }
        }
    }

    // 6. Handle Promotions
    if (promoted_piece)
    {
        // Remove the pawn from board (we already moved it to target in step 4)
        pop_bit(bitboards[(side == white) ? P : p], target_square);
        // Add promoted piece
        set_bit(bitboards[promoted_piece], target_square);

        // HASH UPDATE: Change Pawn to Promoted Piece
        hash_key ^= piece_keys[(side == white) ? P : p][target_square]; // Remove Pawn
        hash_key ^= piece_keys[promoted_piece][target_square];          // Add Queen/Knight...
    }

    // 7. Handle En Passant Capture
    if (enpass)
    {
        if (side == white)
        {
            pop_bit(bitboards[p], target_square + 8);
            // HASH UPDATE: Remove the captured pawn
            hash_key ^= piece_keys[p][target_square + 8];
        }
        else
        {
            pop_bit(bitboards[P], target_square - 8);
            // HASH UPDATE: Remove the captured pawn
            hash_key ^= piece_keys[P][target_square - 8];
        }
    }

    // 8. Handle En Passant State Update
    if (en_passant != no_sq)
        hash_key ^= enpassant_keys[en_passant]; // Remove old En Passant Key

    en_passant = no_sq;

    if (double_push)
    {
        if (side == white)
        {
            en_passant = target_square + 8;
            hash_key ^= enpassant_keys[target_square + 8]; // Add new En Passant Key
        }
        else
        {
            en_passant = target_square - 8;
            hash_key ^= enpassant_keys[target_square - 8]; // Add new En Passant Key
        }
    }

    // 9. Handle Castling (Move Rooks)
    if (castling)
    {
        switch (target_square)
        {
        case g1: // White King Side
            pop_bit(bitboards[R], h1);
            set_bit(bitboards[R], f1);
            // HASH UPDATE: Rook Move
            hash_key ^= piece_keys[R][h1];
            hash_key ^= piece_keys[R][f1];
            break;
        case c1: // White Queen Side
            pop_bit(bitboards[R], a1);
            set_bit(bitboards[R], d1);
            // HASH UPDATE: Rook Move
            hash_key ^= piece_keys[R][a1];
            hash_key ^= piece_keys[R][d1];
            break;
        case g8: // Black King Side
            pop_bit(bitboards[r], h8);
            set_bit(bitboards[r], f8);
            // HASH UPDATE: Rook Move
            hash_key ^= piece_keys[r][h8];
            hash_key ^= piece_keys[r][f8];
            break;
        case c8: // Black Queen Side
            pop_bit(bitboards[r], a8);
            set_bit(bitboards[r], d8);
            // HASH UPDATE: Rook Move
            hash_key ^= piece_keys[r][a8];
            hash_key ^= piece_keys[r][d8];
            break;
        }
    }

    // 10. Update Castling Rights
    hash_key ^= castle_keys[castle]; // Remove old Castling Key

    castle &= castling_rights[source_square];
    castle &= castling_rights[target_square];

    hash_key ^= castle_keys[castle]; // Add new Castling Key

    // 11. Update Occupancies
    for (int i = 0; i < 3; i++)
        occupancies[i] = 0ULL;
    for (int bb_piece = P; bb_piece <= K; bb_piece++)
        occupancies[white] |= bitboards[bb_piece];
    for (int bb_piece = p; bb_piece <= k; bb_piece++)
        occupancies[black] |= bitboards[bb_piece];
    occupancies[both] = occupancies[white] | occupancies[black];

    // 12. Change Side
    side ^= 1;
    hash_key ^= side_key;

    // 13. Legality Check
    if (is_square_attacked((side == white) ? get_ls1b_index(bitboards[k]) : get_ls1b_index(bitboards[K]), side))
    {
        take_back();
        return 0;
    }
    return 1;
}

// -------------------------------------------- \\
//              INPUT / OUTPUT                  \\
// -------------------------------------------- \\

void parse_fen(char *fen)
{
    // 1. Clear Board State
    for (int i = 0; i < 12; i++)
        bitboards[i] = 0ULL;
    for (int i = 0; i < 3; i++)
        occupancies[i] = 0ULL;
    side = 0;
    en_passant = no_sq;
    castle = 0;

    // 2. Parse Pieces with bounds checking
    int rank = 0;
    int file = 0;

    while (rank < 8 && *fen && *fen != ' ')
    {
        int square = rank * 8 + file;

        // Match Pieces (Letters)
        if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
        {
            int piece = -1;
            switch (*fen)
            {
            case 'P':
                piece = P;
                break;
            case 'N':
                piece = N;
                break;
            case 'B':
                piece = B;
                break;
            case 'R':
                piece = R;
                break;
            case 'Q':
                piece = Q;
                break;
            case 'K':
                piece = K;
                break;
            case 'p':
                piece = p;
                break;
            case 'n':
                piece = n;
                break;
            case 'b':
                piece = b;
                break;
            case 'r':
                piece = r;
                break;
            case 'q':
                piece = q;
                break;
            case 'k':
                piece = k;
                break;
            }

            if (piece != -1)
                set_bit(bitboards[piece], square);
            file++;
            fen++;
        }
        // Match Empty Squares (Numbers)
        else if (*fen >= '0' && *fen <= '9')
        {
            int offset = *fen - '0';
            file += offset;
            fen++;
        }
        // Match Rank Separator (Slash)
        else if (*fen == '/')
        {
            file = 0;
            rank++;
            fen++;
        }
        else
        {
            fen++;
        }
    }

    // 3-6: Same as original...
    fen++;
    side = (*fen == 'w') ? white : black;
    fen += 2;

    while (*fen != ' ')
    {
        switch (*fen)
        {
        case 'K':
            castle |= wk;
            break;
        case 'Q':
            castle |= wq;
            break;
        case 'k':
            castle |= bk;
            break;
        case 'q':
            castle |= bq;
            break;
        case '-':
            break;
        }
        fen++;
    }

    fen++;
    if (*fen != '-')
    {
        int file = fen[0] - 'a';
        int rank = 8 - (fen[1] - '0');
        en_passant = rank * 8 + file;
    }
    else
    {
        en_passant = no_sq;
    }

    for (int piece = P; piece <= K; piece++)
        occupancies[white] |= bitboards[piece];
    for (int piece = p; piece <= k; piece++)
        occupancies[black] |= bitboards[piece];
    occupancies[both] = occupancies[white] | occupancies[black];

    hash_key = generate_hash_key();
}
// Parse move string (e.g. "e2e4") into move integer
int parse_move(char *move_string)
{
    // SAFETY CHECK: Validate string length
    if (!move_string || strlen(move_string) < 4)
        return 0;

    moves move_list[1];
    generate_moves(move_list);

    int source_file = move_string[0] - 'a';
    int source_rank = 8 - (move_string[1] - '0');
    int target_file = move_string[2] - 'a';
    int target_rank = 8 - (move_string[3] - '0');

    // BOUNDS CHECK
    if (source_file < 0 || source_file > 7 ||
        source_rank < 0 || source_rank > 7 ||
        target_file < 0 || target_file > 7 ||
        target_rank < 0 || target_rank > 7)
        return 0;

    int source = source_rank * 8 + source_file;
    int target = target_rank * 8 + target_file;

    for (int count = 0; count < move_list->count; count++)
    {
        int move = move_list->moves[count];

        if (get_move_source(move) == source && get_move_target(move) == target)
        {
            int promoted = get_move_promoted(move);

            if (promoted)
            {
                // Check if string has promotion character
                if (strlen(move_string) < 5)
                    continue;

                if ((promoted == Q || promoted == q) && move_string[4] == 'q')
                    return move;
                if ((promoted == R || promoted == r) && move_string[4] == 'r')
                    return move;
                if ((promoted == B || promoted == b) && move_string[4] == 'b')
                    return move;
                if ((promoted == N || promoted == n) && move_string[4] == 'n')
                    return move;
                continue;
            }

            return move;
        }
    }
    return 0;
}
// Parse UCI "position" command
void parse_position(char *command)
{
    // 1. Shift pointer to the command content
    command += 9;
    char *current_char = command;

    // Reset repetition index
    repetition_index = 0;

    // 2. Parse "startpos"
    if (strncmp(command, "startpos", 8) == 0)
    {
        parse_fen(start_position);
    }
    // 3. Parse "fen" (if setting a specific puzzle)
    else
    {
        // Assume "fen " comes after "position "
        current_char = strstr(command, "fen");
        if (current_char == NULL)
        {
            parse_fen(start_position); // Fallback
        }
        else
        {
            current_char += 4; // Skip "fen "
            parse_fen(current_char);
        }
    }

    // Store initial position in repetition table
    repetition_table[repetition_index] = hash_key;

    // 4. Parse "moves" (Play the moves on the board)
    current_char = strstr(command, "moves");
    if (current_char != NULL)
    {
        current_char += 6; // Skip "moves "

        // Loop through all moves in the string
        while (*current_char)
        {
            int move = parse_move(current_char);

            if (move == 0)
                break; // Safety break

            make_move(move, all_moves);

            // Store position in repetition table
            repetition_index++;
            repetition_table[repetition_index] = hash_key;

            // Move pointer to the end of the current move string
            while (*current_char && *current_char != ' ')
                current_char++;

            // Skip space to get to the next move
            current_char++;
        }
    }

    // 5. Update Board State
    // print_board();
}

// -------------------------------------------- \\
//       Evaluation & Negamax Algorithm         \\
// -------------------------------------------- \\

// Piece values - Tal-style: slightly overvalue attacking pieces
int material_weights[12] = {
    100, 320, 330, 500, 950, 20000,      // White P, N, B, R, Q, K
    -100, -320, -330, -500, -950, -20000 // Black p, n, b, r, q, k
};

// Piece mobility bonuses
const int mobility_bonus[5] = {0, 4, 3, 2, 1}; // N, B, R, Q (per square)

// King attack weights for Tal-style aggression
const int king_attack_weights[5] = {0, 20, 20, 40, 80}; // N, B, R, Q

// Passed pawn bonus by rank
const int passed_pawn_bonus[8] = {0, 120, 80, 50, 30, 15, 15, 0};

// Doubled/isolated pawn penalty
const int doubled_pawn_penalty = 10;
const int isolated_pawn_penalty = 20;

// Rook on open/semi-open file bonus
const int rook_open_file_bonus = 25;
const int rook_semi_open_bonus = 15;

// Bishop pair bonus
const int bishop_pair_bonus = 50;

// ============================================ \\
//     BOA CONSTRICTOR STYLE EVALUATION         \\
//   "Slowly bleed the opponent to death"       \\
// ============================================ \\

// Space advantage bonus - reward controlling squares
const int space_bonus_mg = 2; // Per controlled square in opponent's half
const int space_bonus_eg = 1;

// Piece restriction penalty - punish opponent's trapped/restricted pieces
const int restricted_piece_penalty = 8; // Per square below average mobility

// Pawn chain bonus - reward solid pawn structures
const int pawn_chain_bonus = 10;

// Outpost bonus - reward pieces on outpost squares
const int knight_outpost_bonus = 25;
const int bishop_outpost_bonus = 15;

// King tropism - reward pieces near enemy king
const int king_tropism_bonus = 3; // Per distance unit closer

// Trade bonus when ahead - simplify when winning
const int trade_bonus_per_100cp = 5;

// Blockade bonus - reward blocking passed pawns
const int blockade_bonus = 20;

// Mop-up evaluation constants (for winning endgames)
// Push enemy king to corner when we have winning material
const int center_manhattan_distance[64] = {
    6, 5, 4, 3, 3, 4, 5, 6,
    5, 4, 3, 2, 2, 3, 4, 5,
    4, 3, 2, 1, 1, 2, 3, 4,
    3, 2, 1, 0, 0, 1, 2, 3,
    3, 2, 1, 0, 0, 1, 2, 3,
    4, 3, 2, 1, 1, 2, 3, 4,
    5, 4, 3, 2, 2, 3, 4, 5,
    6, 5, 4, 3, 3, 4, 5, 6};

// Distance between two squares (Chebyshev distance) - needed for mop_up_eval
static inline int mop_up_square_distance(int sq1, int sq2)
{
    int r1 = sq1 / 8, f1 = sq1 % 8;
    int r2 = sq2 / 8, f2 = sq2 % 8;
    int dr = abs(r1 - r2);
    int df = abs(f1 - f2);
    return (dr > df) ? dr : df;
}

// Mop-up: Drive enemy king to corner when winning
int mop_up_eval(int winning_side, int losing_king_sq, int winning_king_sq)
{
    int score = 0;

    // 1. Push enemy king to corner (away from center)
    score += center_manhattan_distance[losing_king_sq] * 10;

    // 2. Bring our king closer to enemy king
    int king_distance = mop_up_square_distance(winning_king_sq, losing_king_sq);
    score += (14 - king_distance) * 4;

    return score;
}

// File/Rank control
const int seventh_rank_rook_bonus = 30;
const int connected_rooks_bonus = 15;

// King safety shelter
const int pawn_shelter_bonus = 10;
const int pawn_storm_bonus = 5;

// Calculate space control (squares in opponent's territory we attack)
int calculate_space(int color)
{
    int space = 0;
    U64 our_attacks = 0ULL;
    U64 their_territory = (color == white) ? 0x00000000FFFFFFFFULL : // Ranks 1-4 for black
                              0xFFFFFFFF00000000ULL;                 // Ranks 5-8 for white

    // Accumulate all our piece attacks
    if (color == white)
    {
        U64 pawns = bitboards[P];
        while (pawns)
        {
            int sq = get_ls1b_index(pawns);
            our_attacks |= pawn_attacks[white][sq];
            pop_bit(pawns, sq);
        }
        U64 knights = bitboards[N];
        while (knights)
        {
            int sq = get_ls1b_index(knights);
            our_attacks |= knight_attacks[sq];
            pop_bit(knights, sq);
        }
        U64 bishops = bitboards[B];
        while (bishops)
        {
            int sq = get_ls1b_index(bishops);
            our_attacks |= get_bishop_attacks_magic(sq, occupancies[both]);
            pop_bit(bishops, sq);
        }
        U64 rooks = bitboards[R];
        while (rooks)
        {
            int sq = get_ls1b_index(rooks);
            our_attacks |= get_rook_attacks_magic(sq, occupancies[both]);
            pop_bit(rooks, sq);
        }
        U64 queens = bitboards[Q];
        while (queens)
        {
            int sq = get_ls1b_index(queens);
            our_attacks |= get_queen_attacks(sq, occupancies[both]);
            pop_bit(queens, sq);
        }
    }
    else
    {
        U64 pawns = bitboards[p];
        while (pawns)
        {
            int sq = get_ls1b_index(pawns);
            our_attacks |= pawn_attacks[black][sq];
            pop_bit(pawns, sq);
        }
        U64 knights = bitboards[n];
        while (knights)
        {
            int sq = get_ls1b_index(knights);
            our_attacks |= knight_attacks[sq];
            pop_bit(knights, sq);
        }
        U64 bishops = bitboards[b];
        while (bishops)
        {
            int sq = get_ls1b_index(bishops);
            our_attacks |= get_bishop_attacks_magic(sq, occupancies[both]);
            pop_bit(bishops, sq);
        }
        U64 rooks = bitboards[r];
        while (rooks)
        {
            int sq = get_ls1b_index(rooks);
            our_attacks |= get_rook_attacks_magic(sq, occupancies[both]);
            pop_bit(rooks, sq);
        }
        U64 queens = bitboards[q];
        while (queens)
        {
            int sq = get_ls1b_index(queens);
            our_attacks |= get_queen_attacks(sq, occupancies[both]);
            pop_bit(queens, sq);
        }
    }

    space = count_bits(our_attacks & their_territory);
    return space;
}

// Calculate piece mobility restriction (how much we limit opponent's pieces)
int calculate_restriction(int color)
{
    int restriction = 0;

    // Average expected mobility
    const int avg_knight_mobility = 5;
    const int avg_bishop_mobility = 7;
    const int avg_rook_mobility = 11;
    const int avg_queen_mobility = 20;

    if (color == white)
    {
        // Check black piece restrictions
        U64 knights = bitboards[n];
        while (knights)
        {
            int sq = get_ls1b_index(knights);
            int mobility = count_bits(knight_attacks[sq] & ~occupancies[black]);
            if (mobility < avg_knight_mobility)
                restriction += (avg_knight_mobility - mobility) * restricted_piece_penalty;
            pop_bit(knights, sq);
        }
        U64 bishops = bitboards[b];
        while (bishops)
        {
            int sq = get_ls1b_index(bishops);
            int mobility = count_bits(get_bishop_attacks_magic(sq, occupancies[both]) & ~occupancies[black]);
            if (mobility < avg_bishop_mobility)
                restriction += (avg_bishop_mobility - mobility) * restricted_piece_penalty;
            pop_bit(bishops, sq);
        }
    }
    else
    {
        // Check white piece restrictions
        U64 knights = bitboards[N];
        while (knights)
        {
            int sq = get_ls1b_index(knights);
            int mobility = count_bits(knight_attacks[sq] & ~occupancies[white]);
            if (mobility < avg_knight_mobility)
                restriction += (avg_knight_mobility - mobility) * restricted_piece_penalty;
            pop_bit(knights, sq);
        }
        U64 bishops = bitboards[B];
        while (bishops)
        {
            int sq = get_ls1b_index(bishops);
            int mobility = count_bits(get_bishop_attacks_magic(sq, occupancies[both]) & ~occupancies[white]);
            if (mobility < avg_bishop_mobility)
                restriction += (avg_bishop_mobility - mobility) * restricted_piece_penalty;
            pop_bit(bishops, sq);
        }
    }

    return restriction;
}

// Check if square is an outpost (protected by pawn, can't be attacked by enemy pawns)
int is_outpost(int square, int color)
{
    int file = square % 8;
    int rank = square / 8;

    // Must be in opponent's half
    if (color == white && rank > 3)
        return 0;
    if (color == black && rank < 4)
        return 0;

    // Must be protected by our pawn
    U64 our_pawns = (color == white) ? bitboards[P] : bitboards[p];
    U64 pawn_defenders = (color == white) ? pawn_attacks[black][square] : pawn_attacks[white][square];
    if (!(pawn_defenders & our_pawns))
        return 0;

    // Can't be attacked by enemy pawns (no enemy pawns on adjacent files ahead)
    U64 enemy_pawns = (color == white) ? bitboards[p] : bitboards[P];

    for (int r = (color == white) ? rank - 1 : rank + 1;
         (color == white) ? r >= 0 : r <= 7;
         r += (color == white) ? -1 : 1)
    {
        for (int f = file - 1; f <= file + 1; f += 2)
        {
            if (f >= 0 && f <= 7)
            {
                if (get_bit(enemy_pawns, r * 8 + f))
                    return 0;
            }
        }
    }

    return 1;
}

// Calculate pawn chain strength
int calculate_pawn_chain(int color)
{
    int bonus = 0;
    U64 pawns = (color == white) ? bitboards[P] : bitboards[p];
    U64 original_pawns = pawns;

    while (pawns)
    {
        int sq = get_ls1b_index(pawns);

        // Check if protected by another pawn
        U64 defenders = (color == white) ? pawn_attacks[black][sq] : pawn_attacks[white][sq];
        if (defenders & original_pawns)
            bonus += pawn_chain_bonus;

        pop_bit(pawns, sq);
    }

    return bonus;
}

// Distance between two squares (Chebyshev distance)
int square_distance(int sq1, int sq2)
{
    int r1 = sq1 / 8, f1 = sq1 % 8;
    int r2 = sq2 / 8, f2 = sq2 % 8;
    int dr = abs(r1 - r2);
    int df = abs(f1 - f2);
    return (dr > df) ? dr : df;
}

// Calculate king tropism (pieces gravitating toward enemy king)
int calculate_king_tropism(int color)
{
    int tropism = 0;
    int enemy_king = (color == white) ? get_ls1b_index(bitboards[k]) : get_ls1b_index(bitboards[K]);

    // Knights
    U64 knights = (color == white) ? bitboards[N] : bitboards[n];
    while (knights)
    {
        int sq = get_ls1b_index(knights);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus;
        pop_bit(knights, sq);
    }

    // Bishops
    U64 bishops = (color == white) ? bitboards[B] : bitboards[b];
    while (bishops)
    {
        int sq = get_ls1b_index(bishops);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus;
        pop_bit(bishops, sq);
    }

    // Rooks
    U64 rooks = (color == white) ? bitboards[R] : bitboards[r];
    while (rooks)
    {
        int sq = get_ls1b_index(rooks);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus / 2;
        pop_bit(rooks, sq);
    }

    // Queens
    U64 queens = (color == white) ? bitboards[Q] : bitboards[q];
    while (queens)
    {
        int sq = get_ls1b_index(queens);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus * 2;
        pop_bit(queens, sq);
    }

    return tropism;
}

// NNUE Evaluation (if enabled and loaded)
// OPTIMIZED: Only iterate over active pieces (~32 max), not all 768 inputs!
int evaluate_nnue()
{
    if (!nnue_weights.loaded)
        return 0;

    // Collect active piece indices (much faster than iterating 768 inputs)
    int active_indices[32];
    int num_active = 0;

    for (int piece = P; piece <= k; piece++)
    {
        U64 bb = bitboards[piece];
        while (bb)
        {
            int sq = get_ls1b_index(bb);
            int idx = piece * 64 + sq;
            if (idx < NNUE_INPUT_SIZE && num_active < 32)
                active_indices[num_active++] = idx;
            pop_bit(bb, sq);
        }
    }

    // Forward pass - Layer 1 (OPTIMIZED: only add active weights)
    float hidden1[NNUE_HIDDEN1_SIZE];

    // Start with biases
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
        hidden1[i] = nnue_weights.hidden1_bias[i];

    // Only add weights for active pieces (32 pieces max vs 768!)
    for (int a = 0; a < num_active; a++)
    {
        int j = active_indices[a];
        for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
            hidden1[i] += nnue_weights.input_weights[j][i];
    }

    // Apply activation
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
        hidden1[i] = crelu(hidden1[i]);

    // Forward pass - Layer 2
    float hidden2[NNUE_HIDDEN2_SIZE];
    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++)
    {
        hidden2[i] = nnue_weights.hidden2_bias[i];
        for (int j = 0; j < NNUE_HIDDEN1_SIZE; j++)
        {
            hidden2[i] += hidden1[j] * nnue_weights.hidden1_weights[j][i];
        }
        hidden2[i] = crelu(hidden2[i]);
    }

    // Output layer
    float output = nnue_weights.output_bias;
    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++)
    {
        output += hidden2[i] * nnue_weights.hidden2_weights[i];
    }

    // Scale and return
    int score = (int)(output * NNUE_SCALE);
    return (side == white) ? score : -score;
}

// King safety tables (Tal-style: reward attacking near enemy king)
int count_king_attackers(int king_square, int attacking_side)
{
    int attackers = 0;
    int king_zone = king_attacks[king_square] | (1ULL << king_square);

    if (attacking_side == white)
    {
        // Count white pieces attacking black king zone
        attackers += count_bits(knight_attacks[king_square] & bitboards[N]);
        attackers += count_bits(get_bishop_attacks_magic(king_square, occupancies[both]) & (bitboards[B] | bitboards[Q]));
        attackers += count_bits(get_rook_attacks_magic(king_square, occupancies[both]) & (bitboards[R] | bitboards[Q]));
    }
    else
    {
        attackers += count_bits(knight_attacks[king_square] & bitboards[n]);
        attackers += count_bits(get_bishop_attacks_magic(king_square, occupancies[both]) & (bitboards[b] | bitboards[q]));
        attackers += count_bits(get_rook_attacks_magic(king_square, occupancies[both]) & (bitboards[r] | bitboards[q]));
    }
    return attackers;
}

// Check if pawn is passed
int is_passed_pawn(int square, int color)
{
    int file = square % 8;
    int rank = square / 8;

    if (color == white)
    {
        // Check if there are any enemy pawns blocking or guarding
        for (int r = rank - 1; r >= 0; r--)
        {
            for (int f = file - 1; f <= file + 1; f++)
            {
                if (f >= 0 && f <= 7)
                {
                    if (get_bit(bitboards[p], r * 8 + f))
                        return 0;
                }
            }
        }
        return 1;
    }
    else
    {
        for (int r = rank + 1; r <= 7; r++)
        {
            for (int f = file - 1; f <= file + 1; f++)
            {
                if (f >= 0 && f <= 7)
                {
                    if (get_bit(bitboards[P], r * 8 + f))
                        return 0;
                }
            }
        }
        return 1;
    }
}

int evaluate()
{
    // Try NNUE evaluation first (if enabled at runtime AND loaded)
    if (use_nnue_eval && nnue_weights.loaded)
    {
        return evaluate_nnue();
    }

    int score = 0;
    int mg_score = 0; // Middlegame score
    int eg_score = 0; // Endgame score
    U64 bitboard;
    int square;

    // Phase calculation for tapered eval
    int phase = 0;
    phase += count_bits(bitboards[N] | bitboards[n]) * 1;
    phase += count_bits(bitboards[B] | bitboards[b]) * 1;
    phase += count_bits(bitboards[R] | bitboards[r]) * 2;
    phase += count_bits(bitboards[Q] | bitboards[q]) * 4;
    int total_phase = 24;
    int phase_score = (phase * 256 + total_phase / 2) / total_phase;

    // Bishop pair bonus
    if (count_bits(bitboards[B]) >= 2)
        score += bishop_pair_bonus;
    if (count_bits(bitboards[b]) >= 2)
        score -= bishop_pair_bonus;

    // King positions for king safety
    int white_king_sq = get_ls1b_index(bitboards[K]);
    int black_king_sq = get_ls1b_index(bitboards[k]);

    // Tal-style king attack bonus
    int white_king_attackers = count_king_attackers(black_king_sq, white);
    int black_king_attackers = count_king_attackers(white_king_sq, black);
    score += white_king_attackers * 15;
    score -= black_king_attackers * 15;

    // ============================================
    // BOA CONSTRICTOR EVALUATION - Slow Suffocation
    // ============================================

    // Space advantage - control opponent's territory
    int white_space = calculate_space(white);
    int black_space = calculate_space(black);
    score += (white_space - black_space) * space_bonus_mg;

    // Piece restriction - reward limiting opponent's mobility
    int white_restriction = calculate_restriction(white);
    int black_restriction = calculate_restriction(black);
    score -= white_restriction;
    score += black_restriction;

    // Pawn chains - reward solid structures
    score += calculate_pawn_chain(white);
    score -= calculate_pawn_chain(black);

    // King tropism - pieces gravitating toward enemy king
    score += calculate_king_tropism(white);
    score -= calculate_king_tropism(black);

    // Trade bonus when ahead - simplify when winning
    int material_imbalance = 0;
    for (int piece = P; piece <= k; piece++)
    {
        material_imbalance += material_weights[piece] * count_bits(bitboards[piece]);
    }
    if (abs(material_imbalance) >= 100)
    {
        // Encourage trades when ahead
        int num_pieces = count_bits(occupancies[both]);
        int trade_bonus = (32 - num_pieces) * trade_bonus_per_100cp * abs(material_imbalance) / 100;
        if (material_imbalance > 0)
            score += trade_bonus; // White is ahead
        else
            score -= trade_bonus; // Black is ahead
    }

    for (int piece = P; piece <= k; piece++)
    {
        bitboard = bitboards[piece];
        while (bitboard)
        {
            square = get_ls1b_index(bitboard);

            // Add Material weight
            score += material_weights[piece];

            // Add Positional weight
            switch (piece)
            {
            case P:
                score += pawn_score[square];
                // Passed pawn bonus - use endgame bonus when fewer pieces
                if (is_passed_pawn(square, white))
                {
                    if (phase_score <= 128)
                        score += passed_pawn_bonus_eg[square / 8];
                    else
                        score += passed_pawn_bonus[square / 8];
                }
                break;
            case N:
                score += knight_score[square];
                // Mobility bonus - knights love outposts
                score += count_bits(knight_attacks[square] & ~occupancies[white]) * 4;
                // Outpost bonus - Boa Constrictor style
                if (is_outpost(square, white))
                    score += knight_outpost_bonus;
                break;
            case B:
                score += bishop_score[square];
                // Mobility bonus - bishops love diagonals
                score += count_bits(get_bishop_attacks_magic(square, occupancies[both]) & ~occupancies[white]) * 5;
                // Outpost bonus
                if (is_outpost(square, white))
                    score += bishop_outpost_bonus;
                break;
            case R:
                score += rook_score[square];
                {
                    int file = square % 8;
                    int rank = square / 8;
                    U64 file_mask = 0x0101010101010101ULL << file;
                    // Open file bonus
                    if (!(file_mask & (bitboards[P] | bitboards[p])))
                        score += rook_open_file_bonus;
                    else if (!(file_mask & bitboards[P]))
                        score += rook_semi_open_bonus;
                    // 7th rank bonus (Boa Constrictor loves rooks on 7th!)
                    if (rank == 1) // 7th rank for white
                        score += seventh_rank_rook_bonus;
                }
                // Mobility
                score += count_bits(get_rook_attacks_magic(square, occupancies[both]) & ~occupancies[white]) * 2;
                break;
            case Q:
                // Queen mobility (Tal's favorite piece!)
                score += count_bits(get_queen_attacks(square, occupancies[both]) & ~occupancies[white]) * 1;
                break;
            case K:
                // Use endgame king table when few pieces remain
                if (phase_score <= 128)
                    score += king_endgame_score[square];
                else
                {
                    score += king_score[square];
                    // Pawn shelter bonus in middlegame
                    U64 shelter_mask = king_attacks[square] & bitboards[P];
                    score += count_bits(shelter_mask) * pawn_shelter_bonus;
                }
                break;

            // Mirror indices for black pieces (subtracting because black is negative)
            case p:
                score -= pawn_score[square ^ 56];
                // Use stronger endgame passed pawn bonus
                if (is_passed_pawn(square, black))
                {
                    if (phase_score <= 128)
                        score -= passed_pawn_bonus_eg[7 - (square / 8)];
                    else
                        score -= passed_pawn_bonus[7 - (square / 8)];
                }
                break;
            case n:
                score -= knight_score[square ^ 56];
                score -= count_bits(knight_attacks[square] & ~occupancies[black]) * 4;
                if (is_outpost(square, black))
                    score -= knight_outpost_bonus;
                break;
            case b:
                score -= bishop_score[square ^ 56];
                score -= count_bits(get_bishop_attacks_magic(square, occupancies[both]) & ~occupancies[black]) * 5;
                if (is_outpost(square, black))
                    score -= bishop_outpost_bonus;
                break;
            case r:
                score -= rook_score[square ^ 56];
                {
                    int file = square % 8;
                    int rank = square / 8;
                    U64 file_mask = 0x0101010101010101ULL << file;
                    if (!(file_mask & (bitboards[P] | bitboards[p])))
                        score -= rook_open_file_bonus;
                    else if (!(file_mask & bitboards[p]))
                        score -= rook_semi_open_bonus;
                    // 2nd rank for black (their 7th)
                    if (rank == 6)
                        score -= seventh_rank_rook_bonus;
                }
                score -= count_bits(get_rook_attacks_magic(square, occupancies[both]) & ~occupancies[black]) * 2;
                break;
            case q:
                score -= count_bits(get_queen_attacks(square, occupancies[both]) & ~occupancies[black]) * 1;
                break;
            case k:
                // Use endgame king table when few pieces remain
                if (phase_score <= 128)
                    score -= king_endgame_score[square ^ 56];
                else
                {
                    score -= king_score[square ^ 56];
                    U64 shelter_mask = king_attacks[square] & bitboards[p];
                    score -= count_bits(shelter_mask) * pawn_shelter_bonus;
                }
                break;
            }
            pop_bit(bitboard, square);
        }
    }

    // ============================================
    // MOP-UP EVALUATION - For winning endgames
    // Push enemy king to corner when we have big material advantage
    // ============================================
    if (phase_score <= 128 && abs(material_imbalance) >= 400)
    {
        // We are in endgame with significant material advantage
        if (material_imbalance > 0)
        {
            // White is winning - push black king to corner
            score += mop_up_eval(white, black_king_sq, white_king_sq);
        }
        else
        {
            // Black is winning - push white king to corner
            score -= mop_up_eval(black, white_king_sq, black_king_sq);
        }
    }

    // Tempo bonus for side to move
    score += (side == white) ? 10 : -10;

    return (side == white) ? score : -score;
}

int quiescence(int alpha, int beta)
{
    // Time check
    if ((nodes & 2047) == 0)
        communicate();
    if (times_up)
        return 0;

    nodes++;

    int stand_pat = evaluate();

    // Standing pat cutoff
    if (stand_pat >= beta)
        return beta;

    // Delta pruning - if we can't possibly improve alpha even with a queen capture
    const int BIG_DELTA = 975; // Queen value
    if (stand_pat + BIG_DELTA < alpha)
        return alpha;

    if (alpha < stand_pat)
        alpha = stand_pat;

    moves move_list[1];
    generate_moves(move_list);

    // Score and sort captures only
    int scores[256];
    for (int i = 0; i < move_list->count; i++)
    {
        if (get_move_capture(move_list->moves[i]))
            scores[i] = score_move(move_list->moves[i], 0, 0);
        else
            scores[i] = -1000000; // Non-captures get very low score
    }

    for (int count = 0; count < move_list->count; count++)
    {
        // Selection sort for captures only
        int best_idx = count;
        for (int next = count + 1; next < move_list->count; next++)
        {
            if (scores[next] > scores[best_idx])
                best_idx = next;
        }

        // Swap moves and scores
        if (best_idx != count)
        {
            int temp_move = move_list->moves[count];
            move_list->moves[count] = move_list->moves[best_idx];
            move_list->moves[best_idx] = temp_move;

            int temp_score = scores[count];
            scores[count] = scores[best_idx];
            scores[best_idx] = temp_score;
        }

        // Skip non-captures
        if (!get_move_capture(move_list->moves[count]))
            continue;

        // SEE pruning in quiescence - skip bad captures
        if (!see_ge(move_list->moves[count], 0))
            continue;

        copy_board();
        if (!make_move(move_list->moves[count], only_captures))
            continue;

        int score = -quiescence(-beta, -alpha);
        take_back();

        if (times_up)
            return 0;

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }
    return alpha;
}

// Negamax with Alpha-Beta, Killer Moves, History, and Counter-moves
int negamax(int alpha, int beta, int depth, int ply)
{
    // Initialize PV length
    pv_length[ply] = ply;

    // Is this a PV node (principal variation)?
    int pv_node = (beta - alpha > 1);

    // 1. Time Check (Every 2048 nodes to save performance)
    if ((nodes & 2047) == 0)
    {
        communicate();
    }
    // Abort if time is up
    if (times_up)
        return 0;

    // Repetition detection - return draw score
    if (ply > 0 && is_repetition())
        return 0;

    // 1. Check Transposition Table
    int tt_score = read_tt(alpha, beta, depth, ply);
    int pv_move = get_tt_move();

    // If we have a stored value and we are not at the root (ply 0), use it
    if (ply && tt_score != -INF - 1)
        return tt_score;

    // 2. Base Case: Quiescence Search
    if (depth <= 0)
        return quiescence(alpha, beta);

    nodes++;

    // Safety check for array bounds
    if (ply >= MAX_PLY - 1)
        return evaluate();

    // Check if King is in check
    int in_check = is_square_attacked((side == white) ? get_ls1b_index(bitboards[K]) : get_ls1b_index(bitboards[k]), side ^ 1);

    // Check extension - search deeper when in check
    if (in_check)
        depth++;

    // --- NULL MOVE PRUNING ---
    // Only if not in check and have some pieces (avoid zugzwang)
    int non_pawn_material = (side == white) ? (count_bits(bitboards[N]) + count_bits(bitboards[B]) + count_bits(bitboards[R]) + count_bits(bitboards[Q])) : (count_bits(bitboards[n]) + count_bits(bitboards[b]) + count_bits(bitboards[r]) + count_bits(bitboards[q]));

    if (depth >= 3 && !in_check && ply > 0 && non_pawn_material > 0)
    {
        copy_board();

        // Store repetition index
        int old_rep_index = repetition_index;

        // Make Null Move
        side ^= 1;
        hash_key ^= side_key;
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        if (en_passant != no_sq)
        {
            hash_key ^= enpassant_keys[en_passant];
            en_passant = no_sq;
        }

        // Adaptive null move reduction
        int R = 3 + depth / 6;
        if (R > depth - 1)
            R = depth - 1;

        int score = -negamax(-beta, -beta + 1, depth - 1 - R, ply + 1);

        // Restore
        repetition_index = old_rep_index;
        take_back();

        if (times_up)
            return 0;

        if (score >= beta)
            return beta;
    }
    // -------------------------

    // --- RAZORING (aggressive pruning for Tal-style play) ---
    if (depth <= 3 && !in_check && ply > 0)
    {
        int razor_margin = 300 + 60 * depth;
        int eval = evaluate();
        if (eval + razor_margin < alpha)
        {
            int razor_score = quiescence(alpha - razor_margin, beta - razor_margin);
            if (razor_score + razor_margin <= alpha)
                return alpha;
        }
    }

    // --- REVERSE FUTILITY PRUNING ---
    if (depth <= 6 && !in_check && ply > 0)
    {
        int eval = evaluate();
        int futility_margin = 80 * depth;
        if (eval - futility_margin >= beta)
            return eval - futility_margin;
    }

    moves move_list[1];
    generate_moves(move_list);

    // Score all moves for ordering
    int scores[256];
    for (int i = 0; i < move_list->count; i++)
    {
        scores[i] = score_move(move_list->moves[i], pv_move, ply);
    }

    int moves_searched = 0;
    int best_so_far = -INF;
    int best_move_found = 0;
    int old_alpha = alpha;

    for (int count = 0; count < move_list->count; count++)
    {
        // Selection sort
        int best_idx = count;
        for (int next = count + 1; next < move_list->count; next++)
        {
            if (scores[next] > scores[best_idx])
                best_idx = next;
        }

        // Swap
        if (best_idx != count)
        {
            int temp = move_list->moves[count];
            move_list->moves[count] = move_list->moves[best_idx];
            move_list->moves[best_idx] = temp;

            int temp_score = scores[count];
            scores[count] = scores[best_idx];
            scores[best_idx] = temp_score;
        }

        copy_board();

        // Store for repetition detection
        int old_rep_index = repetition_index;

        if (!make_move(move_list->moves[count], all_moves))
            continue;

        // Add to repetition table
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        // Track last move for counter-move heuristic
        last_move_made[ply] = move_list->moves[count];

        moves_searched++;
        int score;

        int is_capture = get_move_capture(move_list->moves[count]);
        int is_promotion = get_move_promoted(move_list->moves[count]);
        int is_quiet = !is_capture && !is_promotion;
        int gives_check = is_square_attacked(
            (side == white) ? get_ls1b_index(bitboards[k]) : get_ls1b_index(bitboards[K]),
            side);

        // --- LATE MOVE PRUNING (LMP) ---
        // Skip late quiet moves at low depths
        if (depth <= 7 && !pv_node && !in_check && !gives_check && is_quiet && moves_searched > lmp_margins[depth < 8 ? depth : 7])
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // --- FUTILITY PRUNING at move level ---
        // Skip quiet moves that can't possibly raise alpha
        if (depth <= 6 && !pv_node && !in_check && !gives_check && is_quiet && moves_searched > 1)
        {
            int static_eval = evaluate();
            if (static_eval + futility_margins[depth] <= alpha)
            {
                repetition_index = old_rep_index;
                take_back();
                continue;
            }
        }

        // --- SEE PRUNING for bad captures ---
        if (depth <= 8 && !pv_node && is_capture && !see_ge(move_list->moves[count], -30 * depth * depth))
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // --- EXTENSIONS ---
        int extension = 0;

        // Check extension (already applied earlier, but boost for discovered checks)
        if (gives_check)
            extension = 1;

        // Passed pawn extension
        if (get_move_piece(move_list->moves[count]) == P || get_move_piece(move_list->moves[count]) == p)
        {
            int target = get_move_target(move_list->moves[count]);
            int rank = target / 8;
            // 7th rank push (white) or 2nd rank push (black)
            if ((side == black && rank == 1) || (side == white && rank == 6))
                extension = 1;
        }

        // PVS + LMR
        if (moves_searched == 1)
        {
            // First move - full window search
            score = -negamax(-beta, -alpha, depth - 1 + extension, ply + 1);
        }
        else
        {
            // Late Move Reductions
            int reduction = 0;

            if (moves_searched >= 3 && depth >= 3 && !in_check && is_quiet)
            {
                // Use LMR table - base reduction
                reduction = lmr_table[depth < MAX_PLY ? depth : MAX_PLY - 1][moves_searched < 64 ? moves_searched : 63];

                // Reduce less for PV nodes
                if (pv_node)
                    reduction--;

                // Reduce less for killer moves
                if (move_list->moves[count] == killer_moves[0][ply] ||
                    move_list->moves[count] == killer_moves[1][ply])
                    reduction--;

                // Reduce less for counter-moves
                if (ply > 0 && last_move_made[ply - 1])
                {
                    int lm = last_move_made[ply - 1];
                    if (counter_moves[get_move_piece(lm)][get_move_target(lm)] == move_list->moves[count])
                        reduction--;
                }

                // Reduce less for moves with good history
                int hist = history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])];
                if (hist > 1000)
                    reduction--;
                else if (hist < -1000)
                    reduction++;

                // Reduce more for non-PV nodes at high depths
                if (!pv_node && depth > 8)
                    reduction++;

                // Don't reduce into qsearch, minimum depth 1
                if (reduction > depth - 2)
                    reduction = depth - 2;
                if (reduction < 0)
                    reduction = 0;
            }

            // Zero window search with possible reduction
            score = -negamax(-alpha - 1, -alpha, depth - 1 - reduction + extension, ply + 1);

            // Re-search with full window if score improved
            if (score > alpha && (reduction > 0 || score < beta))
            {
                score = -negamax(-beta, -alpha, depth - 1 + extension, ply + 1);
            }
        }

        // Restore repetition index
        repetition_index = old_rep_index;
        take_back();

        if (times_up)
            return 0;

        if (score > best_so_far)
        {
            best_so_far = score;
            best_move_found = move_list->moves[count];

            // Update PV
            pv_table[ply][ply] = move_list->moves[count];
            for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++)
            {
                pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
            }
            pv_length[ply] = pv_length[ply + 1];
        }

        if (score >= beta)
        {
            int move = move_list->moves[count];
            int piece = get_move_piece(move);
            int target = get_move_target(move);
            int from = get_move_source(move);

            // Depth-based bonus (quadratic scaling is stronger)
            int bonus = depth * depth;
            if (bonus > 400)
                bonus = 400; // Cap to prevent overflow

            if (is_capture)
            {
                // Update capture history for good captures
                int victim = P;
                int start = (side == white) ? p : P;
                int end = (side == white) ? k : K;
                for (int p = start; p <= end; p++)
                {
                    if (get_bit(bitboards[p], target))
                    {
                        victim = p;
                        break;
                    }
                }
                capture_history[piece][target][victim % 6] += bonus * 4;
                // Clamp
                if (capture_history[piece][target][victim % 6] > history_max)
                    capture_history[piece][target][victim % 6] = history_max;
            }
            else
            {
                // Quiet move caused cutoff - update killer, history, counter-move

                // Killer moves
                if (move != killer_moves[0][ply])
                {
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = move;
                }

                // History bonus with gravity
                history_moves[piece][target] += bonus;
                if (history_moves[piece][target] > history_max)
                    history_moves[piece][target] = history_max;

                // Butterfly history
                butterfly_history[side][from][target] += bonus;
                if (butterfly_history[side][from][target] > history_max)
                    butterfly_history[side][from][target] = history_max;

                // Counter-move
                if (ply > 0 && last_move_made[ply - 1])
                {
                    int lm = last_move_made[ply - 1];
                    counter_moves[get_move_piece(lm)][get_move_target(lm)] = move;
                }

                // History malus for all other quiet moves searched (they failed to cause cutoff)
                for (int i = 0; i < count; i++)
                {
                    int bad_move = move_list->moves[i];
                    if (!get_move_capture(bad_move) && bad_move != move)
                    {
                        history_moves[get_move_piece(bad_move)][get_move_target(bad_move)] -= bonus / 2;
                        if (history_moves[get_move_piece(bad_move)][get_move_target(bad_move)] < -history_max)
                            history_moves[get_move_piece(bad_move)][get_move_target(bad_move)] = -history_max;
                    }
                }
            }

            write_tt(depth, beta, HASH_BETA, move, ply);
            return beta;
        }

        if (score > alpha)
        {
            alpha = score;
            if (ply == 0)
                best_move = move_list->moves[count];
        }
    }

    // No legal moves
    if (moves_searched == 0)
    {
        if (in_check)
            return -MATE + ply;
        else
            return contempt; // Stalemate - use contempt factor
    }

    // Store in TT
    int flag = (alpha > old_alpha) ? HASH_EXACT : HASH_ALPHA;
    write_tt(depth, alpha, flag, best_move_found, ply);

    return alpha;
}

// -------------------------------------------- \\
//                PERFORMANCE TEST              \\
// -------------------------------------------- \\

// Leaf nodes count

// Perft driver
static inline void perft_driver(int depth)
{
    if (depth == 0)
    {
        nodes++;
        return;
    }

    moves move_list[1];
    generate_moves(move_list);

    for (int count = 0; count < move_list->count; count++)
    {
        copy_board();

        if (!make_move(move_list->moves[count], all_moves))
        {
            continue;
        }

        perft_driver(depth - 1);
        take_back();
    }
}

// Perft test (Top level)
void perft_test(int depth)
{
    nodes = 0LL;
    printf("\n  Performance test\n\n");

    moves move_list[1];
    generate_moves(move_list);

    for (int count = 0; count < move_list->count; count++)
    {
        copy_board();

        if (!make_move(move_list->moves[count], all_moves))
        {
            continue;
        }

        long cumulative_nodes = nodes;
        perft_driver(depth - 1);
        take_back();

        long old_nodes = nodes - cumulative_nodes;
        printf("  move: %d  ", count + 1);
        print_move(move_list->moves[count]);
        printf("  nodes: %ld\n", old_nodes);
    }

    printf("\n  Depth: %d\n", depth);
    printf("  Nodes: %lld\n", nodes);
}

// -------------------------------------------- \\
//                MAIN DRIVER                   \\
// -------------------------------------------- \\

// UCI loop
void uci_loop()
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    char input[2000];

    while (1)
    {
        memset(input, 0, sizeof(input));
        fflush(stdout);

        if (!fgets(input, 2000, stdin))
            continue;
        if (input[0] == '\n')
            continue;

        // Handle "stop" command - stop pondering or search
        if (strncmp(input, "stop", 4) == 0)
        {
            stop_pondering = 1;
            times_up = 1;
            // Note: The search will stop on next communicate() check
            // and output bestmove in the go handler
            continue;
        }

        // Handle "ponderhit" - opponent played our expected move!
        if (strncmp(input, "ponderhit", 9) == 0)
        {
            // Signal that opponent played expected move
            // Search will continue with proper time management
            ponder_hit = 1;
            // Time will be set from the subsequent go command or use default
            continue;
        }

        if (strncmp(input, "isready", 7) == 0)
        {
            // If currently searching/pondering, signal stop first
            stop_pondering = 1;
            times_up = 1;
            // Small delay to let search finish cleanly
            usleep(10000); // 10ms
            pondering = 0;
            stop_pondering = 0;
            printf("readyok\n");
            fflush(stdout);
            continue;
        }
        else if (strncmp(input, "setoption", 9) == 0)
        {
            // Parse UCI options
            if (strstr(input, "OwnBook"))
            {
                use_book = (strstr(input, "true") != NULL);
                printf("info string Book %s\n", use_book ? "enabled" : "disabled");
            }
            else if (strstr(input, "BookFile"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    value += 6; // Skip "value "
                    // Trim newline
                    char filename[256];
                    sscanf(value, "%255s", filename);
                    load_opening_book(filename);
                }
            }
            else if (strstr(input, "UseNNUE"))
            {
                int use_nnue = (strstr(input, "true") != NULL);
                use_nnue_eval = use_nnue; // Set the runtime flag
                if (use_nnue && !nnue_weights.loaded)
                {
                    printf("info string NNUE not loaded, trying nnue.bin\n");
                    if (load_nnue("nnue.bin"))
                    {
                        printf("info string NNUE enabled\n");
                    }
                    else
                    {
                        printf("info string NNUE file not found, using HCE\n");
                        use_nnue_eval = 0;
                    }
                }
                else if (use_nnue)
                {
                    printf("info string NNUE enabled\n");
                }
                else
                {
                    printf("info string NNUE disabled, using HCE\n");
                }
            }
            else if (strstr(input, "NNUEFile"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    value += 6;
                    char filename[256];
                    sscanf(value, "%255s", filename);
                    if (load_nnue(filename))
                    {
                        use_nnue_eval = 1; // Auto-enable when file loaded successfully
                        printf("info string NNUE file loaded and enabled\n");
                    }
                }
            }
            else if (strstr(input, "Hash"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    hash_size_mb = atoi(value + 6);
                    if (hash_size_mb < 1)
                        hash_size_mb = 1;
                    if (hash_size_mb > 4096)
                        hash_size_mb = 4096;
                    printf("info string Hash set to %d MB\n", hash_size_mb);
                    // Note: In a full implementation, you'd resize the TT here
                }
            }
            else if (strstr(input, "Contempt"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    contempt = atoi(value + 6);
                    printf("info string Contempt set to %d cp\n", contempt);
                }
            }
            else if (strstr(input, "MultiPV"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    multi_pv = atoi(value + 6);
                    if (multi_pv < 1)
                        multi_pv = 1;
                    if (multi_pv > 10)
                        multi_pv = 10;
                    printf("info string MultiPV set to %d\n", multi_pv);
                }
            }
            continue;
        }
        else if (strncmp(input, "position", 8) == 0)
        {
            parse_position(input);
            // Causing 3 Fold Repetions
            // // Reset repetition tracking for new position
            // repetition_index = 0;
            // repetition_table[repetition_index] = hash_key;
        }
        else if (strncmp(input, "ucinewgame", 10) == 0)
        {
            parse_position("position startpos");
            clear_tt();
            memset(killer_moves, 0, sizeof(killer_moves));
            memset(history_moves, 0, sizeof(history_moves));
            memset(counter_moves, 0, sizeof(counter_moves));
            memset(butterfly_history, 0, sizeof(butterfly_history));
            repetition_index = 0;
        }

        else if (strncmp(input, "go", 2) == 0)
        {
            // Reset pondering state for new search
            stop_pondering = 0;
            ponder_hit = 0;
            times_up = 0;

            // Check if this is a ponder command
            int is_ponder = (strstr(input, "ponder") != NULL);
            pondering = is_ponder;

            // Check opening book first (but not when pondering)
            if (use_book && !is_ponder)
            {
                int book_move = get_book_move();
                if (book_move)
                {
                    printf("info string Book move\n");
                    printf("bestmove ");
                    print_move(book_move);
                    printf("\n");
                    fflush(stdout);
                    continue;
                }
            }

            // 1. Initialize Time Variables
            int depth = -1;
            int movestogo = 30;
            int movetime = -1;
            int time = -1;
            int inc = 0;
            char *ptr = NULL;

            // 2. Parse Depth (if fixed depth requested)
            if ((ptr = strstr(input, "depth")))
                depth = atoi(ptr + 6);

            // 3. Parse infinite or ponder
            int infinite = (strstr(input, "infinite") != NULL) || is_ponder;

            // 4. Parse Time (wtime/btime)
            if ((ptr = strstr(input, "movestogo")))
                movestogo = atoi(ptr + 10);
            if ((ptr = strstr(input, "movetime")))
                movetime = atoi(ptr + 9);

            if (side == white)
            {
                if ((ptr = strstr(input, "wtime")))
                    time = atoi(ptr + 6);
                if ((ptr = strstr(input, "winc")))
                    inc = atoi(ptr + 5);
            }
            else
            {
                if ((ptr = strstr(input, "btime")))
                    time = atoi(ptr + 6);
                if ((ptr = strstr(input, "binc")))
                    inc = atoi(ptr + 5);
            }

            // 5. Calculate Time to Spend - IMPROVED TIME MANAGEMENT
            if (movetime != -1)
            {
                // Fixed time per move
                time_for_move = movetime - 50;
                if (time_for_move < 10)
                    time_for_move = 10;
            }
            else if (time != -1 && !infinite)
            {
                // ========================================
                // IMPROVED TIME MANAGEMENT
                // ========================================

                // Estimate game phase based on material
                int phase = count_bits(bitboards[N] | bitboards[n]) +
                            count_bits(bitboards[B] | bitboards[b]) +
                            count_bits(bitboards[R] | bitboards[r]) * 2 +
                            count_bits(bitboards[Q] | bitboards[q]) * 4;

                // Expected moves to end of game
                int expected_moves;
                if (movestogo > 0)
                {
                    expected_moves = movestogo;
                }
                else
                {
                    // Dynamic estimate based on game phase
                    // Early game (full material): expect ~40 more moves
                    // Endgame (low material): expect ~20 more moves
                    expected_moves = 20 + phase;
                    if (expected_moves > 50)
                        expected_moves = 50;
                    if (expected_moves < 15)
                        expected_moves = 15;
                }

                // Base time allocation
                time_for_move = time / expected_moves;

                // Add increment (use most of it)
                if (inc > 0)
                    time_for_move += inc * 4 / 5;

                // Use more time in complex positions (when we have more pieces)
                if (phase > 16)
                    time_for_move = time_for_move * 11 / 10;

                // Cap at reasonable fraction of remaining time
                long long max_time;
                if (time > 60000)
                    max_time = time / 5; // > 1 min: use up to 20%
                else if (time > 10000)
                    max_time = time / 6; // > 10s: use up to 16%
                else if (time > 3000)
                    max_time = time / 8; // > 3s: use up to 12%
                else
                    max_time = time / 10; // Low time: use up to 10%

                if (time_for_move > max_time)
                    time_for_move = max_time;

                // Safety buffer
                int safety = 30;
                if (time < 3000)
                    safety = 10;
                if (time < 1000)
                    safety = 5;
                time_for_move -= safety;

                // Minimum time
                if (time_for_move < 10)
                    time_for_move = 10;
            }
            else
            {
                time_for_move = -1; // Infinite
            }

            if (depth == -1)
                search_depth = MAX_PLY - 1;
            else
                search_depth = depth;

            // 6. Setup Search Globals
            start_time = get_time_ms();
            times_up = 0;
            nodes = 0;

            // Age history tables (prevent overflow, help with move ordering freshness)
            for (int i = 0; i < 12; i++)
            {
                for (int j = 0; j < 64; j++)
                {
                    history_moves[i][j] /= 2;
                    for (int k = 0; k < 6; k++)
                        capture_history[i][j][k] /= 2;
                }
            }
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 64; j++)
                    for (int k = 0; k < 64; k++)
                        butterfly_history[i][j][k] /= 2;

            printf("info string Time allocated: %lld ms\n", time_for_move);

            // 7. Iterative Deepening with Aspiration Windows
            int prev_score = 0;
            int aspiration_window = 25; // Smaller window for faster search

            for (int current_depth = 1; current_depth <= search_depth; current_depth++)
            {
                if (times_up)
                    break;

                int alpha, beta;
                int score;

                // Aspiration windows - only after depth 5
                if (current_depth >= 5)
                {
                    alpha = prev_score - aspiration_window;
                    beta = prev_score + aspiration_window;

                    score = negamax(alpha, beta, current_depth, 0);

                    // Failed low - widen window and re-search
                    if (!times_up && score <= alpha)
                    {
                        alpha = prev_score - aspiration_window * 4;
                        if (alpha < -INF)
                            alpha = -INF;
                        score = negamax(alpha, beta, current_depth, 0);
                    }
                    // Failed high - widen window and re-search
                    if (!times_up && score >= beta)
                    {
                        beta = prev_score + aspiration_window * 4;
                        if (beta > INF)
                            beta = INF;
                        score = negamax(alpha, beta, current_depth, 0);
                    }
                    // Still failing - full window search
                    if (!times_up && (score <= alpha || score >= beta))
                    {
                        score = negamax(-INF, INF, current_depth, 0);
                    }
                }
                else
                {
                    score = negamax(-INF, INF, current_depth, 0);
                }

                if (times_up)
                    break;

                prev_score = score;

                // Calculate NPS and time
                long long elapsed = get_time_ms() - start_time;
                if (elapsed < 1)
                    elapsed = 1;
                long long nps = nodes * 1000 / elapsed;

                // Output info with PV
                printf("info depth %d score ", current_depth);

                // Output mate score properly
                if (score > MATE - 100)
                    printf("mate %d", (MATE - score + 1) / 2);
                else if (score < -MATE + 100)
                    printf("mate %d", -(MATE + score + 1) / 2);
                else
                    printf("cp %d", score);

                printf(" nodes %lld nps %lld time %lld pv ", nodes, nps, elapsed);

                // Print PV
                for (int i = 0; i < pv_length[0]; i++)
                {
                    print_move(pv_table[0][i]);
                    printf(" ");
                }
                printf("\n");
                fflush(stdout);

                // Soft time management with smarter conditions
                if (time_for_move != -1)
                {
                    // If we found a mate, keep searching until time runs out
                    if (score > MATE - 100 || score < -MATE + 100)
                        continue;

                    // If we've used > 60% of time and depth >= 8, consider stopping
                    if (elapsed > time_for_move * 6 / 10 && current_depth >= 8)
                        break;

                    // If we've used > 80% of time, definitely stop
                    if (elapsed > time_for_move * 8 / 10)
                        break;
                }
            }

            // 8. Output Final Best Move with ponder move if available
            pondering = 0; // Reset pondering state
            printf("bestmove ");
            if (best_move)
                print_move(best_move);
            else
                printf("0000"); // Fallback if no move found

            // Output ponder move (second move in PV) if we have one
            if (pv_length[0] >= 2 && pv_table[0][1])
            {
                printf(" ponder ");
                print_move(pv_table[0][1]);
                ponder_move = pv_table[0][1];
            }
            printf("\n");
            fflush(stdout); // Ensure output is sent immediately!
        }

        else if (strncmp(input, "quit", 4) == 0)
        {
            stop_pondering = 1;
            times_up = 1;
            break;
        }
        else if (strncmp(input, "uci", 3) == 0)
        {
            printf("id name Fe64-Boa v3.0\\n");
            printf("id author Syed Masood\n");
            printf("option name Hash type spin default 64 min 1 max 4096\n");
            printf("option name Contempt type spin default 10 min -100 max 100\n");
            printf("option name MultiPV type spin default 1 min 1 max 10\n");
            printf("option name OwnBook type check default true\n");
            printf("option name BookFile type string default book.bin\n");
            printf("option name UseNNUE type check default false\n");
            printf("option name NNUEFile type string default nnue.bin\n");
            printf("option name Ponder type check default true\n");
            printf("option name SyzygyPath type string default <empty>\n");
            printf("uciok\n");
        }
        // Custom commands for training/debugging
        else if (strncmp(input, "loadbook", 8) == 0)
        {
            char filename[256] = "book.bin";
            sscanf(input + 9, "%255s", filename);
            load_opening_book(filename);
        }
        else if (strncmp(input, "loadnnue", 8) == 0)
        {
            char filename[256] = "nnue.bin";
            sscanf(input + 9, "%255s", filename);
            load_nnue(filename);
        }
        else if (strncmp(input, "savennue", 8) == 0)
        {
            char filename[256] = "nnue.bin";
            sscanf(input + 9, "%255s", filename);
            save_nnue(filename);
            printf("info string NNUE saved to %s\n", filename);
        }
        else if (strncmp(input, "initnnue", 8) == 0)
        {
            init_nnue_random();
            printf("info string NNUE initialized with random weights\n");
        }
        else if (strncmp(input, "eval", 4) == 0)
        {
            printf("info string Static eval: %d cp\n", evaluate());
        }
    }

    // Cleanup
    free_opening_book();
}

int main(int argc, char *argv[])
{
    // CRITICAL: Initialize attack tables BEFORE using them!
    init_leapers_attacks();  // Initialize pawn, knight, king attacks
    init_sliders_attacks(1); // Initialize bishop attacks (1 = bishop)
    init_sliders_attacks(0); // Initialize rook attacks (0 = rook)

    init_hash_keys();
    init_lmr_table(); // Initialize Late Move Reduction table

    hash_key = generate_hash_key();
    clear_tt();

    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));
    memset(counter_moves, 0, sizeof(counter_moves));
    memset(butterfly_history, 0, sizeof(butterfly_history));
    memset(last_move_made, 0, sizeof(last_move_made));

    repetition_index = 0;

    // Try to load opening book from default location
    load_opening_book("book.bin");

    // Try to load NNUE if available
    if (NNUE_ENABLED)
    {
        load_nnue("nnue.bin");
    }

    // Command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-book") == 0 && i + 1 < argc)
        {
            load_opening_book(argv[++i]);
        }
        else if (strcmp(argv[i], "-nnue") == 0 && i + 1 < argc)
        {
            load_nnue(argv[++i]);
        }
        else if (strcmp(argv[i], "-nobook") == 0)
        {
            use_book = 0;
        }
    }

    uci_loop();

    return 0;
}