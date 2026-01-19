/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file board.c
 * @brief Board representation and manipulation
 * @author Syed Masood
 * @version 4.0.0
 */

#include "include/board.h"
#include "include/hash.h"

// ============================================
//          BOARD STATE GLOBALS
// ============================================

U64 bitboards[12];
U64 occupancies[3];

int side = WHITE;
int en_passant = no_sq;
int castle = 0;
int fifty_move = 0;
int full_moves = 1;

U64 hash_key = 0ULL;

U64 repetition_table[MAX_GAME_MOVES];
int repetition_index = 0;

// ============================================
//          CASTLING RIGHTS UPDATE TABLE
// ============================================

/**
 * Update castling rights based on square moved from/to
 * AND with current rights to update them
 */
const int castling_rights[64] = {
    7, 15, 15, 15, 3, 15, 15, 11,   // a8-h8
    15, 15, 15, 15, 15, 15, 15, 15, // a7-h7
    15, 15, 15, 15, 15, 15, 15, 15, // a6-h6
    15, 15, 15, 15, 15, 15, 15, 15, // a5-h5
    15, 15, 15, 15, 15, 15, 15, 15, // a4-h4
    15, 15, 15, 15, 15, 15, 15, 15, // a3-h3
    15, 15, 15, 15, 15, 15, 15, 15, // a2-h2
    13, 15, 15, 15, 12, 15, 15, 14  // a1-h1
};

// ============================================
//          ATTACK DETECTION
// ============================================

/**
 * Check if a square is attacked by a given side
 */
int is_square_attacked(int square, int attacking_side)
{
    // Pawn attacks
    if (attacking_side == WHITE)
    {
        if (pawn_attacks[BLACK][square] & bitboards[P])
            return 1;
    }
    else
    {
        if (pawn_attacks[WHITE][square] & bitboards[p])
            return 1;
    }

    // Knight attacks
    if (knight_attacks[square] & ((attacking_side == WHITE) ? bitboards[N] : bitboards[n]))
        return 1;

    // King attacks
    if (king_attacks[square] & ((attacking_side == WHITE) ? bitboards[K] : bitboards[k]))
        return 1;

    // Bishop/Queen attacks (diagonal)
    U64 diagonal_attackers = (attacking_side == WHITE)
                                 ? (bitboards[B] | bitboards[Q])
                                 : (bitboards[b] | bitboards[q]);
    if (get_bishop_attacks_magic(square, occupancies[BOTH]) & diagonal_attackers)
        return 1;

    // Rook/Queen attacks (straight)
    U64 straight_attackers = (attacking_side == WHITE)
                                 ? (bitboards[R] | bitboards[Q])
                                 : (bitboards[r] | bitboards[q]);
    if (get_rook_attacks_magic(square, occupancies[BOTH]) & straight_attackers)
        return 1;

    return 0;
}

/**
 * Check if current side's king is in check
 */
int in_check(void)
{
    int king_square = get_ls1b_index((side == WHITE) ? bitboards[K] : bitboards[k]);
    return is_square_attacked(king_square, side ^ 1);
}

/**
 * Get the piece at a given square
 */
int get_piece_at_square(int square)
{
    for (int piece = P; piece <= k; piece++)
    {
        if (get_bit(bitboards[piece], square))
            return piece;
    }
    return -1;
}

// ============================================
//          REPETITION DETECTION
// ============================================

/**
 * Check for repetition (draw detection)
 */
int is_repetition(void)
{
    for (int i = repetition_index - 2; i >= 0; i -= 2)
    {
        if (repetition_table[i] == hash_key)
            return 1;
    }
    return 0;
}

// ============================================
//          DRAW DETECTION
// ============================================

/**
 * Check for insufficient material
 */
int is_insufficient_material(void)
{
    // King vs King
    if (count_bits(occupancies[BOTH]) == 2)
        return 1;

    // King + Bishop vs King
    if (count_bits(occupancies[BOTH]) == 3)
    {
        if (count_bits(bitboards[B]) == 1 || count_bits(bitboards[b]) == 1)
            return 1;
        if (count_bits(bitboards[N]) == 1 || count_bits(bitboards[n]) == 1)
            return 1;
    }

    // King + Bishop vs King + Bishop (same color bishops)
    if (count_bits(occupancies[BOTH]) == 4)
    {
        if (count_bits(bitboards[B]) == 1 && count_bits(bitboards[b]) == 1)
        {
            int wb_sq = get_ls1b_index(bitboards[B]);
            int bb_sq = get_ls1b_index(bitboards[b]);
            // Same color squares
            if (((wb_sq / 8) + (wb_sq % 8)) % 2 == ((bb_sq / 8) + (bb_sq % 8)) % 2)
                return 1;
        }
    }

    return 0;
}

/**
 * Check if position is a draw
 */
int is_draw(void)
{
    // Fifty-move rule
    if (fifty_move >= 100)
        return 1;

    // Repetition
    if (is_repetition())
        return 1;

    // Insufficient material
    if (is_insufficient_material())
        return 1;

    return 0;
}

// ============================================
//          BOARD DISPLAY
// ============================================

/**
 * Print the current board state
 */
void print_board(void)
{
    printf("\n");

    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;

            if (!file)
                printf("  %d ", 8 - rank);

            int piece = -1;
            for (int bb_piece = P; bb_piece <= k; bb_piece++)
            {
                if (get_bit(bitboards[bb_piece], square))
                {
                    piece = bb_piece;
                    break;
                }
            }

            printf(" %c", (piece == -1) ? '.' : ascii_pieces[piece]);
        }
        printf("\n");
    }

    printf("\n     a b c d e f g h\n\n");
    printf("     Side:      %s\n", (side == WHITE) ? "white" : "black");
    printf("     En passant: %s\n", (en_passant != no_sq) ? "yes" : "no");
    printf("     Castling:  %c%c%c%c\n",
           (castle & wk) ? 'K' : '-',
           (castle & wq) ? 'Q' : '-',
           (castle & bk) ? 'k' : '-',
           (castle & bq) ? 'q' : '-');
    printf("     Hash key:  %llx\n\n", hash_key);
}

// ============================================
//          FEN PARSING
// ============================================

/**
 * Parse a FEN string and setup the board
 */
void parse_fen(const char *fen)
{
    // Clear board state
    for (int i = 0; i < 12; i++)
        bitboards[i] = 0ULL;
    for (int i = 0; i < 3; i++)
        occupancies[i] = 0ULL;

    side = WHITE;
    en_passant = no_sq;
    castle = 0;
    fifty_move = 0;
    full_moves = 1;

    // Parse piece placement
    int rank = 0, file = 0;

    while (rank < 8 && *fen && *fen != ' ')
    {
        int square = rank * 8 + file;

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

            if (piece != -1 && square < 64)
                set_bit(bitboards[piece], square);
            file++;
            fen++;
        }
        else if (*fen >= '1' && *fen <= '8')
        {
            file += (*fen - '0');
            fen++;
        }
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

    // Skip space
    if (*fen)
        fen++;

    // Parse side to move
    side = (*fen == 'w') ? WHITE : BLACK;
    fen++;

    // Skip space
    if (*fen)
        fen++;

    // Parse castling rights
    while (*fen && *fen != ' ')
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
        }
        fen++;
    }

    // Skip space
    if (*fen)
        fen++;

    // Parse en passant
    if (*fen && *fen != '-')
    {
        int ep_file = fen[0] - 'a';
        int ep_rank = 8 - (fen[1] - '0');
        en_passant = ep_rank * 8 + ep_file;
        fen += 2;
    }
    else
    {
        en_passant = no_sq;
        if (*fen)
            fen++;
    }

    // Skip space
    if (*fen)
        fen++;

    // Parse halfmove clock (fifty-move rule)
    if (*fen && *fen >= '0' && *fen <= '9')
    {
        fifty_move = atoi(fen);
        while (*fen && *fen != ' ')
            fen++;
    }

    // Skip space
    if (*fen)
        fen++;

    // Parse fullmove number
    if (*fen && *fen >= '0' && *fen <= '9')
    {
        full_moves = atoi(fen);
    }

    // Build occupancy bitboards
    for (int piece = P; piece <= K; piece++)
        occupancies[WHITE] |= bitboards[piece];
    for (int piece = p; piece <= k; piece++)
        occupancies[BLACK] |= bitboards[piece];
    occupancies[BOTH] = occupancies[WHITE] | occupancies[BLACK];

    // Generate hash key
    hash_key = generate_hash_key();
}
