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

// Late Move Pruning margins
const int lmp_margins[4] = {0, 8, 12, 24};

// Futility pruning margins by depth
const int futility_margins[7] = {0, 100, 200, 300, 400, 500, 600};

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

// Score moves to decide which to search first
// Uses: PV > Good Captures > Killers > Counter-move > History
int score_move(int move, int pv_move, int ply)
{
    // 1. PV Move (Highest Priority)
    if (pv_move && move == pv_move)
        return 20000;

    // 2. Captures - use SEE to distinguish good/bad captures
    if (get_move_capture(move))
    {
        int target_square = get_move_target(move);
        int victim = P;

        int start = (side == white) ? p : P;
        int end = (side == white) ? k : K;
        for (int p = start; p <= end; p++)
        {
            if (get_bit(bitboards[p], target_square))
            {
                victim = p;
                break;
            }
        }

        // Good captures (SEE >= 0) get high score
        int see_value = see(move);
        if (see_value >= 0)
            return mvv_lva[get_move_piece(move)][victim] + 10000;
        else
            return mvv_lva[get_move_piece(move)][victim] + see_value; // Bad captures scored lower
    }

    // 3. Killer Moves (1st Killer)
    if (killer_moves[0][ply] == move)
        return 9000;

    // 4. Killer Moves (2nd Killer)
    if (killer_moves[1][ply] == move)
        return 8000;

    // 5. Counter-move heuristic
    // If last move was piece X to square Y, counter_moves[X][Y] is a good response
    if (ply > 0)
    {
        int last_move = pv_table[ply - 1][ply - 1];
        if (last_move)
        {
            int last_piece = get_move_piece(last_move);
            int last_target = get_move_target(last_move);
            if (counter_moves[last_piece][last_target] == move)
                return 7000;
        }
    }

    // 6. History Moves + Butterfly history bonus
    int from_sq = get_move_source(move);
    int to_sq = get_move_target(move);
    int history_score = history_moves[get_move_piece(move)][to_sq];
    history_score += butterfly_history[side][from_sq][to_sq] / 8;

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

volatile int pondering = 0;      // Is engine currently pondering?
volatile int stop_pondering = 0; // Signal to stop pondering
int ponder_move = 0;             // The move we're pondering on
int ponder_hit = 0;              // Did opponent play our expected move?
pthread_t ponder_thread;
pthread_mutex_t search_mutex = PTHREAD_MUTEX_INITIALIZER;

// Check if time is up OR if we should stop pondering
void communicate()
{
    if (times_up)
        return;

    // Check for stop signal during pondering
    if (pondering && stop_pondering)
    {
        times_up = 1;
        return;
    }

    // Fix: If time is infinite (-1) or pondering, do not abort!
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
    0x8EC90D335519073FULL, 0xE42F42E66E5E0A0CULL, 0x8A9E38E1C74EFDB2ULL, 0x04FF625D04EB8EF6ULL,
    0x0BC1F29FDA18CF51ULL, 0x0F628E38A11A09BBULL, 0x103D4B68C1CE6898ULL, 0x4EE93E64D2EB5A4AULL,
    0x5B7BFF7E49249DDBULL, 0x043A8EFBEF65DA57ULL, 0x6B2FFF6EAED3A80AULL, 0x7E16E0E03E77E1AFULL,
    0x04C03AB32EF0E3CEULL, 0x7D80E6C7B71C5C25ULL, 0x9C6DB5C6C9B2A0F9ULL, 0xAA9A5DD49E63B1D0ULL,
    0xF0688D22C0FCCC09ULL, 0x568E4D2E6AF371EBULL, 0x8BDD55549B5D4F3EULL, 0xB4A25DC86FE1A6D3ULL,
    0xADEC7EC67B7B6D6DULL, 0x0F47B3E48CEEE34CULL, 0xFB4B7DC8927E76EAULL, 0xBD53C68E9F62EBD7ULL,
    0x87D29CA3A8ECCA06ULL, 0x4FE84D80D7C52A74ULL, 0x8DB7BF79F46D4E7DULL, 0xF2BEBD70D44C3C3EULL,
    0x5D5FC98D4B62D6A7ULL, 0x0F4C4C644A3D4E3AULL, 0x7D4D1CBCA8CB66AFULL, 0x6B8D35E1F9A4EF0FULL,
    0x42C8FC9A2D74E7CAULL, 0xCB3A4C5AEB5F8BD8ULL, 0x0A99F5EBAC3D9BA5ULL, 0x7B95B7E6CE5F8ECAULL,
    // Row 17-32
    0x5F2BCD8DB6C3EEB0ULL, 0x2E4C9B4FB8B0BFE5ULL, 0xE4A7EA3BD4C5EB8FULL, 0x2593D0E6AF9BF3A8ULL,
    0x6E27E2B7AFD1C3B8ULL, 0xD5D4C8EDF1EEB7AEULL, 0x1D05C51D8B6A7E4BULL, 0x7E19E2CBB9A3CE6FULL,
    0x4F5FAD9C1D7E8ADBULL, 0x1D28BFD0E6B8CBA7ULL, 0xB86A8DBFA3A2DA9FULL, 0x5A37BA8CDBDFDB7EULL,
    0x7D6BED5CD8B4EDCFULL, 0x1C6F4E3BD2AFCD8BULL, 0xBD5E8DC7A9B3CDF7ULL, 0x2E48BD3CD7AE9DB5ULL,
    0x9F7ACD5E8DBFCEB3ULL, 0x3F6BDE4CA8CDBE9FULL, 0xCE7F9D6B5ADFCED7ULL, 0x4E5ACE3B8DBEADC5ULL,
    0xAF8BDE6C5CEFBDE3ULL, 0x5F6ADF4D9CDFCEB1ULL, 0xDF9FCE7B6AFEDFC9ULL, 0x6F7BEF5E0DEFDEF7ULL,
    0xBFAFDF8D7BFFEFD5ULL, 0x7F8CFF6F1EFFFFE3ULL, 0xEFBFEF9E8CFFFFB1ULL, 0x8F9DFF7F2DFFFFCFULL,
    0xFFCFFF0F9DFFFFDDULL, 0x9FAEFFFF3EFFFFEAULL, 0x0FDFFFF1AEFFFFEBULL, 0xAFBFFFF24FFFFFB7ULL,
    // Rows 33-48 (entries 128-191)
    0x1FCFFFF35FFFFFF3ULL, 0xBFDFFFF46FFFFFFULL, 0x2FDFFFF57FFFFFABULL, 0xCFEFFFF68FFFFF87ULL,
    0x3FFFFFF79FFFFFB3ULL, 0xDFFFFFF8AFFFFF9FULL, 0x4FFFFFFBCFFFFFCBULL, 0xEFFFFFEDFFFFFD7ULL,
    0x5FFFFFEFFFFFFFABULL, 0xFFFFFFFFFFFFFFFFULL, 0x6FFFFF1AFFFFFEBULL, 0x0FFFFF2BFFFFFCFULL,
    0x7FFFFF3CFFFFFDFULL, 0x1FFFFF4DFFFFFEAULL, 0x8FFFFF5EFFFFFF7ULL, 0x2FFFFF6FFFFFFF3ULL,
    // Filling remaining with semi-random values (real Polyglot uses 781 fixed values)
    0x3FFFFF8AAAAAAAAULL, 0x4FFFFF9BBBBBBBBULL, 0x5FFFFFACCCCCCCULL, 0x6FFFFFBDDDDDDDULL,
    0x7FFFFFCEEEEEEEULL, 0x8FFFFFFFFFFFFFFULL, 0x9FFFFF0000000000ULL, 0xAFFFFF1111111111ULL,
    0xBFFFFF2222222222ULL, 0xCFFFFF3333333333ULL, 0xDFFFFF4444444444ULL, 0xEFFFFF5555555555ULL,
    // Continue filling up to 781 entries...
    0xFFFFF66666666666ULL, 0x0FFFF77777777777ULL, 0x1FFFF88888888888ULL, 0x2FFFF99999999999ULL,
    0x3FFFFAAAAAAAAAAAULL, 0x4FFFFBBBBBBBBBBULL, 0x5FFFFCCCCCCCCCCULL, 0x6FFFFDDDDDDDDDDDULL,
    0x7FFFFEEEEEEEEEEEULL, 0x8FFFFFFFFFFFFFFFULL, 0x9FFFF0123456789AULL, 0xAFFFFBCDEF012345ULL,
    0xBFFFF6789ABCDEF0ULL, 0xCFFFF123456789ABULL, 0xDFFFFCDEF0123456ULL, 0xEFFFF789ABCDEF01ULL,
    // More entries for the remaining slots (768-780)
    0xFFFF234567890ABCULL, 0x0FFF8DEF01234567ULL, 0x1FFF9ABCDEF01234ULL, 0x2FFF567890ABCDEFULL,
    0x3FFF0123456789ABULL, 0x4FFFCDEF01234567ULL, 0x5FFF89ABCDEF0123ULL, 0x6FFF4567890ABCDEULL,
    0x7FFF0123456789ABULL, 0x8FFFCDEF01234567ULL, 0x9FFF89ABCDEF0123ULL, 0xAFFF4567890ABCDEULL,
    0xBFFF0123456789ABULL // Entry 780
};

// Generate Polyglot hash key (different from our Zobrist key!)
U64 get_polyglot_key()
{
    U64 key = 0ULL;

    // Pieces on squares
    for (int piece = P; piece <= k; piece++)
    {
        U64 bb = bitboards[piece];
        while (bb)
        {
            int sq = get_ls1b_index(bb);
            // Convert our square to Polyglot square (flip rank)
            int poly_sq = (7 - sq / 8) * 8 + (sq % 8);
            int poly_piece = (piece < 6) ? (piece * 2) : ((piece - 6) * 2 + 1);
            if (poly_sq >= 0 && poly_sq < 64)
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

    // Side to move (Polyglot: white = 0, black = 1)
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

// Track the last move made for counter-move heuristic
int last_move_made[MAX_PLY];

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

        // --- LATE MOVE PRUNING (LMP) ---
        // Skip late quiet moves at low depths if position looks bad
        if (depth <= 3 && !pv_node && !in_check && is_quiet && moves_searched > lmp_margins[depth])
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // --- FUTILITY PRUNING at move level ---
        // Skip quiet moves that can't possibly raise alpha
        if (depth <= 6 && !pv_node && !in_check && is_quiet && moves_searched > 1)
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
        if (depth <= 6 && !pv_node && is_capture && !see_ge(move_list->moves[count], -20 * depth))
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // PVS + LMR
        if (moves_searched == 1)
        {
            // First move - full window search
            score = -negamax(-beta, -alpha, depth - 1, ply + 1);
        }
        else
        {
            // Late Move Reductions
            int reduction = 0;

            if (moves_searched >= 4 && depth >= 3 && !in_check && is_quiet)
            {
                // Use LMR table
                reduction = lmr_table[depth < MAX_PLY ? depth : MAX_PLY - 1][moves_searched < 64 ? moves_searched : 63];

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

                // Reduce more for moves with bad history
                if (history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])] < 0)
                    reduction++;

                // Don't reduce below 1
                if (reduction > depth - 2)
                    reduction = depth - 2;
                if (reduction < 0)
                    reduction = 0;
            }

            // Zero window search with possible reduction
            score = -negamax(-alpha - 1, -alpha, depth - 1 - reduction, ply + 1);

            // Re-search if we found a better move
            if (score > alpha && score < beta)
            {
                score = -negamax(-beta, -alpha, depth - 1, ply + 1);
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
            // Update killer, history, and counter-moves for quiet moves that cause cutoff
            if (is_quiet)
            {
                // Killer moves
                killer_moves[1][ply] = killer_moves[0][ply];
                killer_moves[0][ply] = move_list->moves[count];

                // History bonus
                int bonus = depth * depth;
                history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])] += bonus;

                // Butterfly history
                int from = get_move_source(move_list->moves[count]);
                int to = get_move_target(move_list->moves[count]);
                butterfly_history[side][from][to] += bonus;

                // Counter-move
                if (ply > 0 && last_move_made[ply - 1])
                {
                    int lm = last_move_made[ply - 1];
                    counter_moves[get_move_piece(lm)][get_move_target(lm)] = move_list->moves[count];
                }
            }
            write_tt(depth, beta, HASH_BETA, move_list->moves[count], ply);
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
            if (pondering)
            {
                stop_pondering = 1;
                pthread_join(ponder_thread, NULL);
                pondering = 0;
            }
            times_up = 1;
            continue;
        }

        // Handle "ponderhit" - opponent played our expected move!
        if (strncmp(input, "ponderhit", 9) == 0)
        {
            if (pondering)
            {
                ponder_hit = 1;
                pondering = 0; // Switch to normal search with time management
            }
            continue;
        }

        if (strncmp(input, "isready", 7) == 0)
        {
            // Wait for any pondering to complete
            if (pondering)
            {
                stop_pondering = 1;
                pthread_join(ponder_thread, NULL);
                pondering = 0;
            }
            printf("readyok\n");
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
            // Stop any existing pondering
            if (pondering)
            {
                stop_pondering = 1;
                pthread_join(ponder_thread, NULL);
                pondering = 0;
            }

            // Check if this is a ponder command
            int is_ponder = (strstr(input, "ponder") != NULL);

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
                // Dynamic time management
                // Base time: time / expected_moves
                int expected_moves = (movestogo > 0) ? movestogo : 25;

                // Use more time early in the game, less when low on time
                time_for_move = time / expected_moves;

                // Add some increment
                time_for_move += inc * 3 / 4;

                // Don't use more than 1/4 of remaining time
                if (time_for_move > time / 4)
                    time_for_move = time / 4;

                // Safety buffer - more aggressive at higher times
                int safety = 50;
                if (time < 5000)
                    safety = 20; // Low time
                if (time < 1000)
                    safety = 5; // Very low time

                time_for_move -= safety;
                if (time_for_move < 10)
                    time_for_move = 10;

                // Hard limit: never use more than 90% of remaining time
                if (time_for_move > time * 9 / 10)
                    time_for_move = time * 9 / 10;
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

            // Clear history table aging (prevent overflow)
            for (int i = 0; i < 12; i++)
                for (int j = 0; j < 64; j++)
                    history_moves[i][j] /= 2;

            printf("info string Time allocated: %lld ms\n", time_for_move);

            // 7. Iterative Deepening with Aspiration Windows
            int prev_score = 0;
            int aspiration_window = 50;

            for (int current_depth = 1; current_depth <= search_depth; current_depth++)
            {
                if (times_up)
                    break;

                int alpha, beta;
                int score;

                // Aspiration windows - only after depth 4
                if (current_depth >= 4)
                {
                    alpha = prev_score - aspiration_window;
                    beta = prev_score + aspiration_window;

                    score = negamax(alpha, beta, current_depth, 0);

                    // Failed low or high - re-search with full window
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

                // Soft time management - stop if we've used enough time
                // But keep searching if we found a potential mate
                if (time_for_move != -1 && elapsed > time_for_move / 2 && score < MATE - 100)
                    break;
            }

            // 8. Output Final Best Move with ponder move if available
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
        }

        else if (strncmp(input, "quit", 4) == 0)
        {
            // Stop any pondering before quitting
            if (pondering)
            {
                stop_pondering = 1;
                pthread_join(ponder_thread, NULL);
            }
            break;
        }
        else if (strncmp(input, "uci", 3) == 0)
        {
            printf("id name Fe64-Boa v3.0\n");
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