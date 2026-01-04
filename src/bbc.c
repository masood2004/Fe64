// ============================================ \\
//       FE64 CHESS ENGINE - SOURCE CODE        \\
// ============================================ \\

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

// The 64-bit integer is the heart of the engine.
// We use 'unsigned' because we don't need negative numbers.
// We use 'long long' to guarantee 64 bits on all systems.
#define U64 unsigned long long
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

int best_move;

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
    {105, 205, 305, 405, 505, 605, 105, 205, 305, 405, 505, 605},
    {104, 204, 304, 404, 504, 604, 104, 204, 304, 404, 504, 604},
    {103, 203, 303, 403, 503, 603, 103, 203, 303, 403, 503, 603},
    {102, 202, 302, 402, 502, 602, 102, 202, 302, 402, 502, 602},
    {101, 201, 301, 401, 501, 601, 101, 201, 301, 401, 501, 601},
    {100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600},
    // Mirror for black attackers...
};

// Score moves to decide which to search first
int score_move(int move, int pv_move, int ply)
{
    // 1. PV Move (Highest Priority)
    if (pv_move && move == pv_move)
        return 20000;

    // 2. Captures (MVV-LVA)
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
        return mvv_lva[get_move_piece(move)][victim] + 10000;
    }

    // 3. Killer Moves (1st Killer)
    if (killer_moves[0][ply] == move)
        return 9000;

    // 4. Killer Moves (2nd Killer)
    if (killer_moves[1][ply] == move)
        return 8000;

    // 5. History Moves
    return history_moves[get_move_piece(move)][get_move_target(move)];
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

int read_tt(int alpha, int beta, int depth)
{
    tt_entry *entry = &transposition_table[hash_key % TT_SIZE];
    if (entry->key == hash_key)
    {
        if (entry->depth >= depth)
        {
            if (entry->flags == HASH_EXACT)
                return entry->value;
            if (entry->flags == HASH_ALPHA && entry->value <= alpha)
                return alpha;
            if (entry->flags == HASH_BETA && entry->value >= beta)
                return beta;
        }
    }
    return -INF;
}

void write_tt(int depth, int value, int flags, int move)
{
    tt_entry *entry = &transposition_table[hash_key % TT_SIZE];
    entry->key = hash_key;
    entry->depth = depth;
    entry->flags = flags;
    entry->value = value;
    entry->best_move = move;
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

            // Move pointer to the end of the current move string
            while (*current_char && *current_char != ' ')
                current_char++;

            // Skip space to get to the next move
            current_char++;
        }
    }

    // 5. Update Board State
    print_board();
}

// -------------------------------------------- \\
//       Evaluation & Negamax Algorithm         \\
// -------------------------------------------- \\

// Piece values
int material_weights[12] = {
    100, 300, 350, 500, 1000, 10000,      // White P, N, B, R, Q, K
    -100, -300, -350, -500, -1000, -10000 // Black p, n, b, r, q, k
};

int evaluate()
{
    int score = 0;
    U64 bitboard;
    int square;

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
                break;
            case N:
                score += knight_score[square];
                break;
            case B:
                score += bishop_score[square];
                break;
            case R:
                score += rook_score[square];
                break;
            case K:
                score += king_score[square];
                break;

            // Mirror indices for black pieces (subtracting because black is negative)
            case p:
                score -= pawn_score[square ^ 56];
                break;
            case n:
                score -= knight_score[square ^ 56];
                break;
            case b:
                score -= bishop_score[square ^ 56];
                break;
            case r:
                score -= rook_score[square ^ 56];
                break;
            case k:
                score -= king_score[square ^ 56];
                break;
            }
            pop_bit(bitboard, square);
        }
    }
    return (side == white) ? score : -score;
}

int quiescence(int alpha, int beta)
{
    int stand_pat = evaluate();
    if (stand_pat >= beta)
        return beta;
    if (alpha < stand_pat)
        alpha = stand_pat;

    moves move_list[1];
    generate_moves(move_list); // You should modify generate_moves to only produce captures here

    for (int count = 0; count < move_list->count; count++)
    {
        if (!get_move_capture(move_list->moves[count]))
            continue;
        // Move sorting logic (Selection Sort)
        for (int next = count + 1; next < move_list->count; next++)
        {
            if (score_move(move_list->moves[count], 0, 0) < score_move(move_list->moves[next], 0, 0))
            {
                int temp = move_list->moves[count];
                move_list->moves[count] = move_list->moves[next];
                move_list->moves[next] = temp;
            }
        }
        copy_board();
        if (!make_move(move_list->moves[count], only_captures))
            continue;
        int score = -quiescence(-beta, -alpha);
        take_back();

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }
    return alpha;
}

// Negamax with Alpha-Beta, Killer Moves, and History Heuristic
int negamax(int alpha, int beta, int depth, int ply)
{
    // 1. Check Transposition Table
    int score = read_tt(alpha, beta, depth);
    // If we have a stored value and we are not at the root (ply 0), use it
    if (ply && score != -INF && (ply < 64)) // Safety check
        return score;

    // PV Check from TT
    int pv_move = 0;
    tt_entry *entry = &transposition_table[hash_key % TT_SIZE];
    if (entry->key == hash_key)
        pv_move = entry->best_move;

    // 2. Base Case: Quiescence Search
    if (depth == 0)
        return quiescence(alpha, beta);

    // Safety check for array bounds
    if (ply > 63)
        return evaluate();

    // --- NULL MOVE PRUNING ---
    // Rules:
    // 1. Depth must be >= 3 (Don't prune in shallow search)
    // 2. Not root node (ply > 0)
    // 3. Not in Check (If in check, you MUST move)

    // Check if King is in check
    int in_check = is_square_attacked((side == white) ? get_ls1b_index(bitboards[K]) : get_ls1b_index(bitboards[k]), side ^ 1);

    if (depth >= 3 && !in_check && ply > 0)
    {
        // Backup board
        copy_board();

        // Make Null Move (Switch side, clear En Passant)
        side ^= 1;
        hash_key ^= side_key;

        if (en_passant != no_sq)
        {
            hash_key ^= enpassant_keys[en_passant];
            en_passant = no_sq;
        }

        // Search with reduced depth (R=2)
        // We pass -beta, -beta+1 (Zero Window) because we only care if >= beta
        int score = -negamax(-beta, -beta + 1, depth - 1 - 2, ply + 1);

        // Restore board
        take_back();

        // If the null move result is still a cutoff, return beta
        if (score >= beta)
            return beta;
    }
    // -------------------------

    moves move_list[1];
    generate_moves(move_list);

    int moves_searched = 0;
    int best_so_far = -INF;
    int move_to_store = 0;
    int old_alpha = alpha;

    // [Inside negamax, replacing the existing loop]
    for (int count = 0; count < move_list->count; count++)
    {
        // Sort Moves
        for (int next = count + 1; next < move_list->count; next++)
        {
            if (score_move(move_list->moves[count], pv_move, ply) < score_move(move_list->moves[next], pv_move, ply))
            {
                int temp = move_list->moves[count];
                move_list->moves[count] = move_list->moves[next];
                move_list->moves[next] = temp;
            }
        }

        copy_board();
        if (!make_move(move_list->moves[count], all_moves))
            continue;

        moves_searched++;

        // --- PRINCIPAL VARIATION SEARCH (PVS) START ---

        if (count == 0)
        {
            // 1. PV Move: Full Search
            score = -negamax(-beta, -alpha, depth - 1, ply + 1);
        }
        else
        {
            // 2. Late Move Reduction (LMR) will go here later...
            // [Insert this inside the 'else' block of PVS]

            // LMR Conditions:
            // - Depth is substantial (> 2)
            // - We have searched at least 4 moves already
            // - Not a capture (Tactical moves are dangerous to reduce)
            // - Not in check (Safety first)
            // --- LATE MOVE REDUCTION (LMR) ---

            // Condition to check if we should search this move
            // Default: We assume we need to search it (Zero Window)
            int needs_full_search = 1;

            if (depth >= 3 && moves_searched > 4 && !get_move_capture(move_list->moves[count]))
            {
                // Search with reduced depth
                score = -negamax(-alpha - 1, -alpha, depth - 2, ply + 1);

                // If the reduced search confirms the move is bad (score <= alpha),
                // we don't need to search further.
                if (score <= alpha)
                {
                    needs_full_search = 0;
                }
            }

            // --- ZERO WINDOW SEARCH ---

            // Only search if LMR didn't prove the move was bad
            if (needs_full_search)
            {
                score = -negamax(-alpha - 1, -alpha, depth - 1, ply + 1);
            }

            // --- RE-SEARCH (Full Window) ---

            // If the move turns out to be better than expected, we must search it fully
            if (score > alpha && score < beta)
            {
                score = -negamax(-beta, -alpha, depth - 1, ply + 1);
            }
        }

        // --- PRINCIPAL VARIATION SEARCH (PVS) END ---

        take_back();

        if (score > best_so_far)
        {
            best_so_far = score;
            move_to_store = move_list->moves[count];
        }

        if (score >= beta)
        {
            if (!get_move_capture(move_list->moves[count]))
            {
                killer_moves[1][ply] = killer_moves[0][ply];
                killer_moves[0][ply] = move_list->moves[count];
                history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])] += depth * depth;
            }
            write_tt(depth, beta, HASH_BETA, move_list->moves[count]);
            return beta;
        }

        if (score > alpha)
        {
            alpha = score;
            if (ply == 0)
                best_move = move_list->moves[count];
        }
    }

    if (moves_searched == 0)
    {
        if (is_square_attacked((side == white) ? get_ls1b_index(bitboards[K]) : get_ls1b_index(bitboards[k]), side ^ 1))
            return -MATE + ply; // MATE score corrected for distance (shorter mate is better)
        else
            return 0;
    }

    int flag = (alpha > old_alpha) ? HASH_EXACT : HASH_ALPHA;
    write_tt(depth, alpha, flag, move_to_store);

    return alpha;
}

// -------------------------------------------- \\
//                PERFORMANCE TEST              \\
// -------------------------------------------- \\

// Leaf nodes count
long long nodes;

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
    printf("id name Fe64\n");
    printf("id author Syed Masood\n");
    printf("uciok\n");

    while (1)
    {
        memset(input, 0, sizeof(input));
        fflush(stdout);

        if (!fgets(input, 2000, stdin))
            continue;
        if (input[0] == '\n')
            continue;

        if (strncmp(input, "isready", 7) == 0)
        {
            printf("readyok\n");
            continue;
        }
        else if (strncmp(input, "position", 8) == 0)
        {
            parse_position(input);
            clear_tt(); // Optional: Clear hash on new position to avoid collisions
        }
        else if (strncmp(input, "ucinewgame", 10) == 0)
        {
            parse_position("position startpos");
            clear_tt();
        }
        else if (strncmp(input, "go", 2) == 0)
        {
            int depth = -1;
            char *ptr = strstr(input, "depth");
            if (ptr)
                depth = atoi(ptr + 6);
            else
                depth = 6;

            // Search
            for (int current_depth = 1; current_depth <= depth; current_depth++)
            {
                search_depth = current_depth;
                int score = negamax(-INF, INF, current_depth, 0);

                printf("info depth %d score cp %d pv ", current_depth, score);
                // (Optional: Print full PV line here if you stored it)
                print_move(best_move);
                printf("\n");
            }
            printf("bestmove ");
            print_move(best_move);
            printf("\n");
        }
        else if (strncmp(input, "quit", 4) == 0)
        {
            break;
        }
        else if (strncmp(input, "uci", 3) == 0)
        {
            printf("id name Fe64\n");
            printf("id author Syed Masood\n");
            printf("uciok\n");
        }
    }
}

int main()
{
    // CRITICAL: Initialize attack tables BEFORE using them!
    init_leapers_attacks();  // Initialize pawn, knight, king attacks
    init_sliders_attacks(1); // Initialize bishop attacks (1 = bishop)
    init_sliders_attacks(0); // Initialize rook attacks (0 = rook)

    init_hash_keys();
    hash_key = generate_hash_key();
    clear_tt();

    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));

    uci_loop();

    return 0;
}