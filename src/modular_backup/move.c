/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file move.c
 * @brief Move encoding, generation, and execution
 * @author Syed Masood
 * @version 4.0.0
 */

#include "include/move.h"
#include "include/board.h"
#include "include/hash.h"

// ============================================
//          MVV-LVA TABLE
// ============================================

int mvv_lva[12][12] = {
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

// SEE piece values
const int see_piece_values[12] = {
    100, 320, 330, 500, 900, 20000, // White
    100, 320, 330, 500, 900, 20000  // Black
};

// ============================================
//          MOVE UTILITY FUNCTIONS
// ============================================

/**
 * Add a move to the move list
 */
void add_move(MoveList *move_list, Move move)
{
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

/**
 * Print a move in algebraic notation
 */
void print_move(Move move)
{
    printf("%c%d%c%d",
           (get_move_source(move) % 8) + 'a',
           8 - (get_move_source(move) / 8),
           (get_move_target(move) % 8) + 'a',
           8 - (get_move_target(move) / 8));

    int promoted = get_move_promoted(move);
    if (promoted)
    {
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

/**
 * Get move string in algebraic notation
 */
void move_to_string(Move move, char *str)
{
    str[0] = (get_move_source(move) % 8) + 'a';
    str[1] = '0' + (8 - (get_move_source(move) / 8));
    str[2] = (get_move_target(move) % 8) + 'a';
    str[3] = '0' + (8 - (get_move_target(move) / 8));
    str[4] = '\0';

    int promoted = get_move_promoted(move);
    if (promoted)
    {
        switch (promoted)
        {
        case Q:
        case q:
            str[4] = 'q';
            break;
        case R:
        case r:
            str[4] = 'r';
            break;
        case B:
        case b:
            str[4] = 'b';
            break;
        case N:
        case n:
            str[4] = 'n';
            break;
        default:
            str[4] = 'q';
            break;
        }
        str[5] = '\0';
    }
}

/**
 * Parse a move string (e.g., "e2e4")
 */
Move parse_move(char *move_string)
{
    if (!move_string || strlen(move_string) < 4)
        return 0;

    MoveList move_list;
    move_list.count = 0;
    generate_moves(&move_list);

    int source_file = move_string[0] - 'a';
    int source_rank = 8 - (move_string[1] - '0');
    int target_file = move_string[2] - 'a';
    int target_rank = 8 - (move_string[3] - '0');

    // Bounds check
    if (source_file < 0 || source_file > 7 ||
        source_rank < 0 || source_rank > 7 ||
        target_file < 0 || target_file > 7 ||
        target_rank < 0 || target_rank > 7)
        return 0;

    int source = source_rank * 8 + source_file;
    int target = target_rank * 8 + target_file;

    for (int i = 0; i < move_list.count; i++)
    {
        Move move = move_list.moves[i];

        if (get_move_source(move) == source && get_move_target(move) == target)
        {
            int promoted = get_move_promoted(move);

            if (promoted)
            {
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

// ============================================
//          STATIC EXCHANGE EVALUATION
// ============================================

/**
 * Get smallest attacker to a square
 */
static int get_smallest_attacker(int square, int attacker_side, int *attacker_piece)
{
    // Pawns first (smallest)
    if (attacker_side == WHITE)
    {
        if (pawn_attacks[BLACK][square] & bitboards[P])
        {
            *attacker_piece = P;
            return get_ls1b_index(pawn_attacks[BLACK][square] & bitboards[P]);
        }
    }
    else
    {
        if (pawn_attacks[WHITE][square] & bitboards[p])
        {
            *attacker_piece = p;
            return get_ls1b_index(pawn_attacks[WHITE][square] & bitboards[p]);
        }
    }

    // Knights
    int knight_piece = (attacker_side == WHITE) ? N : n;
    if (knight_attacks[square] & bitboards[knight_piece])
    {
        *attacker_piece = knight_piece;
        return get_ls1b_index(knight_attacks[square] & bitboards[knight_piece]);
    }

    // Bishops
    int bishop_piece = (attacker_side == WHITE) ? B : b;
    U64 bishop_atks = get_bishop_attacks_magic(square, occupancies[BOTH]);
    if (bishop_atks & bitboards[bishop_piece])
    {
        *attacker_piece = bishop_piece;
        return get_ls1b_index(bishop_atks & bitboards[bishop_piece]);
    }

    // Rooks
    int rook_piece = (attacker_side == WHITE) ? R : r;
    U64 rook_atks = get_rook_attacks_magic(square, occupancies[BOTH]);
    if (rook_atks & bitboards[rook_piece])
    {
        *attacker_piece = rook_piece;
        return get_ls1b_index(rook_atks & bitboards[rook_piece]);
    }

    // Queens
    int queen_piece = (attacker_side == WHITE) ? Q : q;
    U64 queen_atks = rook_atks | bishop_atks;
    if (queen_atks & bitboards[queen_piece])
    {
        *attacker_piece = queen_piece;
        return get_ls1b_index(queen_atks & bitboards[queen_piece]);
    }

    // King (last resort)
    int king_piece = (attacker_side == WHITE) ? K : k;
    if (king_attacks[square] & bitboards[king_piece])
    {
        *attacker_piece = king_piece;
        return get_ls1b_index(king_attacks[square] & bitboards[king_piece]);
    }

    return -1; // No attacker
}

/**
 * Static Exchange Evaluation
 */
int see(Move move)
{
    int from = get_move_source(move);
    int to = get_move_target(move);
    int attacker = get_move_piece(move);

    // Find captured piece
    int captured = -1;
    int start = (side == WHITE) ? p : P;
    int end = (side == WHITE) ? k : K;

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
        captured = (side == WHITE) ? p : P;

    if (captured == -1)
        return 0; // Not a capture

    int gain[32];
    int depth = 0;

    // Initial gain is captured piece value
    gain[depth] = see_piece_values[captured % 6];

    // Check if defended
    int defender_piece;
    int current_side = side ^ 1;
    int defender_sq = get_smallest_attacker(to, current_side, &defender_piece);

    if (defender_sq == -1)
        return gain[0]; // Not defended

    // Simple SEE: compare attacker vs victim
    if (see_piece_values[attacker % 6] > gain[0])
        return gain[0] - see_piece_values[attacker % 6];

    return gain[0];
}

/**
 * Check if SEE value >= threshold
 */
int see_ge(Move move, int threshold)
{
    return see(move) >= threshold;
}

// ============================================
//          MOVE GENERATION
// ============================================

/**
 * Generate all pseudo-legal moves
 */
void generate_moves(MoveList *move_list)
{
    move_list->count = 0;

    int source_square, target_square;
    U64 bitboard, attacks;

    // ========================
    // PAWN MOVES
    // ========================
    if (side == WHITE)
    {
        bitboard = bitboards[P];

        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);
            target_square = source_square - 8;

            // Quiet pushes
            if (target_square >= a8 && !get_bit(occupancies[BOTH], target_square))
            {
                // Promotion
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

                    // Double push
                    if (source_square >= a2 && source_square <= h2 &&
                        !get_bit(occupancies[BOTH], target_square - 8))
                    {
                        add_move(move_list, encode_move(source_square, target_square - 8, P, 0, 0, 1, 0, 0));
                    }
                }
            }

            // Captures
            attacks = pawn_attacks[WHITE][source_square] & occupancies[BLACK];
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

            // En passant
            if (en_passant != no_sq)
            {
                U64 ep_attacks = pawn_attacks[WHITE][source_square] & (1ULL << en_passant);
                if (ep_attacks)
                {
                    int ep_target = get_ls1b_index(ep_attacks);
                    add_move(move_list, encode_move(source_square, ep_target, P, 0, 1, 0, 1, 0));
                }
            }

            pop_bit(bitboard, source_square);
        }
    }
    else
    {
        // BLACK PAWNS
        bitboard = bitboards[p];

        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);
            target_square = source_square + 8;

            if (target_square <= h1 && !get_bit(occupancies[BOTH], target_square))
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

                    if (source_square >= a7 && source_square <= h7 &&
                        !get_bit(occupancies[BOTH], target_square + 8))
                    {
                        add_move(move_list, encode_move(source_square, target_square + 8, p, 0, 0, 1, 0, 0));
                    }
                }
            }

            attacks = pawn_attacks[BLACK][source_square] & occupancies[WHITE];
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
                U64 ep_attacks = pawn_attacks[BLACK][source_square] & (1ULL << en_passant);
                if (ep_attacks)
                {
                    int ep_target = get_ls1b_index(ep_attacks);
                    add_move(move_list, encode_move(source_square, ep_target, p, 0, 1, 0, 1, 0));
                }
            }

            pop_bit(bitboard, source_square);
        }
    }

    // ========================
    // CASTLING
    // ========================
    if (side == WHITE)
    {
        if (castle & wk)
        {
            if (!get_bit(occupancies[BOTH], f1) && !get_bit(occupancies[BOTH], g1))
            {
                if (!is_square_attacked(e1, BLACK) &&
                    !is_square_attacked(f1, BLACK) &&
                    !is_square_attacked(g1, BLACK))
                {
                    add_move(move_list, encode_move(e1, g1, K, 0, 0, 0, 0, 1));
                }
            }
        }
        if (castle & wq)
        {
            if (!get_bit(occupancies[BOTH], d1) &&
                !get_bit(occupancies[BOTH], c1) &&
                !get_bit(occupancies[BOTH], b1))
            {
                if (!is_square_attacked(e1, BLACK) &&
                    !is_square_attacked(d1, BLACK) &&
                    !is_square_attacked(c1, BLACK))
                {
                    add_move(move_list, encode_move(e1, c1, K, 0, 0, 0, 0, 1));
                }
            }
        }
    }
    else
    {
        if (castle & bk)
        {
            if (!get_bit(occupancies[BOTH], f8) && !get_bit(occupancies[BOTH], g8))
            {
                if (!is_square_attacked(e8, WHITE) &&
                    !is_square_attacked(f8, WHITE) &&
                    !is_square_attacked(g8, WHITE))
                {
                    add_move(move_list, encode_move(e8, g8, k, 0, 0, 0, 0, 1));
                }
            }
        }
        if (castle & bq)
        {
            if (!get_bit(occupancies[BOTH], d8) &&
                !get_bit(occupancies[BOTH], c8) &&
                !get_bit(occupancies[BOTH], b8))
            {
                if (!is_square_attacked(e8, WHITE) &&
                    !is_square_attacked(d8, WHITE) &&
                    !is_square_attacked(c8, WHITE))
                {
                    add_move(move_list, encode_move(e8, c8, k, 0, 0, 0, 0, 1));
                }
            }
        }
    }

    // ========================
    // PIECE MOVES (N, B, R, Q, K)
    // ========================
    int p_start = (side == WHITE) ? N : n;
    int p_end = (side == WHITE) ? K : k;

    for (int piece = p_start; piece <= p_end; piece++)
    {
        bitboard = bitboards[piece];

        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);

            // Get attacks based on piece type
            if (side == WHITE)
            {
                if (piece == N)
                    attacks = knight_attacks[source_square];
                else if (piece == B)
                    attacks = get_bishop_attacks_magic(source_square, occupancies[BOTH]);
                else if (piece == R)
                    attacks = get_rook_attacks_magic(source_square, occupancies[BOTH]);
                else if (piece == Q)
                    attacks = get_queen_attacks(source_square, occupancies[BOTH]);
                else if (piece == K)
                    attacks = king_attacks[source_square];

                attacks &= ~occupancies[WHITE];
            }
            else
            {
                if (piece == n)
                    attacks = knight_attacks[source_square];
                else if (piece == b)
                    attacks = get_bishop_attacks_magic(source_square, occupancies[BOTH]);
                else if (piece == r)
                    attacks = get_rook_attacks_magic(source_square, occupancies[BOTH]);
                else if (piece == q)
                    attacks = get_queen_attacks(source_square, occupancies[BOTH]);
                else if (piece == k)
                    attacks = king_attacks[source_square];

                attacks &= ~occupancies[BLACK];
            }

            while (attacks)
            {
                target_square = get_ls1b_index(attacks);
                int capture = get_bit(occupancies[(side == WHITE) ? BLACK : WHITE], target_square) ? 1 : 0;
                add_move(move_list, encode_move(source_square, target_square, piece, 0, capture, 0, 0, 0));
                pop_bit(attacks, target_square);
            }

            pop_bit(bitboard, source_square);
        }
    }
}

// ============================================
//          MAKE MOVE
// ============================================

/**
 * Make a move on the board
 */
int make_move(Move move, int move_flag)
{
    // Captures only filter
    if (move_flag == CAPTURES_ONLY && !get_move_capture(move))
        return 0;

    // Backup board
    copy_board();

    // Parse move
    int source = get_move_source(move);
    int target = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted = get_move_promoted(move);
    int capture = get_move_capture(move);
    int double_push = get_move_double(move);
    int enpass = get_move_enpassant(move);
    int castling = get_move_castling(move);

    // Update fifty-move counter
    if (piece == P || piece == p || capture)
        fifty_move = 0;
    else
        fifty_move++;

    // Move piece
    pop_bit(bitboards[piece], source);
    set_bit(bitboards[piece], target);
    hash_key ^= piece_keys[piece][source];
    hash_key ^= piece_keys[piece][target];

    // Handle captures
    if (capture)
    {
        int start_piece = (side == WHITE) ? p : P;
        int end_piece = (side == WHITE) ? k : K;

        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            if (get_bit(bitboards[bb_piece], target))
            {
                pop_bit(bitboards[bb_piece], target);
                hash_key ^= piece_keys[bb_piece][target];
                break;
            }
        }
    }

    // Handle promotions
    if (promoted)
    {
        pop_bit(bitboards[(side == WHITE) ? P : p], target);
        set_bit(bitboards[promoted], target);
        hash_key ^= piece_keys[(side == WHITE) ? P : p][target];
        hash_key ^= piece_keys[promoted][target];
    }

    // Handle en passant capture
    if (enpass)
    {
        if (side == WHITE)
        {
            pop_bit(bitboards[p], target + 8);
            hash_key ^= piece_keys[p][target + 8];
        }
        else
        {
            pop_bit(bitboards[P], target - 8);
            hash_key ^= piece_keys[P][target - 8];
        }
    }

    // Update en passant state
    if (en_passant != no_sq)
        hash_key ^= enpassant_keys[en_passant];
    en_passant = no_sq;

    if (double_push)
    {
        if (side == WHITE)
        {
            en_passant = target + 8;
            hash_key ^= enpassant_keys[target + 8];
        }
        else
        {
            en_passant = target - 8;
            hash_key ^= enpassant_keys[target - 8];
        }
    }

    // Handle castling rook movement
    if (castling)
    {
        switch (target)
        {
        case g1: // White kingside
            pop_bit(bitboards[R], h1);
            set_bit(bitboards[R], f1);
            hash_key ^= piece_keys[R][h1];
            hash_key ^= piece_keys[R][f1];
            break;
        case c1: // White queenside
            pop_bit(bitboards[R], a1);
            set_bit(bitboards[R], d1);
            hash_key ^= piece_keys[R][a1];
            hash_key ^= piece_keys[R][d1];
            break;
        case g8: // Black kingside
            pop_bit(bitboards[r], h8);
            set_bit(bitboards[r], f8);
            hash_key ^= piece_keys[r][h8];
            hash_key ^= piece_keys[r][f8];
            break;
        case c8: // Black queenside
            pop_bit(bitboards[r], a8);
            set_bit(bitboards[r], d8);
            hash_key ^= piece_keys[r][a8];
            hash_key ^= piece_keys[r][d8];
            break;
        }
    }

    // Update castling rights
    hash_key ^= castle_keys[castle];
    castle &= castling_rights[source];
    castle &= castling_rights[target];
    hash_key ^= castle_keys[castle];

    // Update occupancies
    occupancies[WHITE] = 0ULL;
    occupancies[BLACK] = 0ULL;
    for (int bb_piece = P; bb_piece <= K; bb_piece++)
        occupancies[WHITE] |= bitboards[bb_piece];
    for (int bb_piece = p; bb_piece <= k; bb_piece++)
        occupancies[BLACK] |= bitboards[bb_piece];
    occupancies[BOTH] = occupancies[WHITE] | occupancies[BLACK];

    // Change side
    side ^= 1;
    hash_key ^= side_key;

    // Legality check - king not in check
    int king_sq = get_ls1b_index((side == WHITE) ? bitboards[k] : bitboards[K]);
    if (is_square_attacked(king_sq, side))
    {
        take_back();
        return 0;
    }

    return 1;
}

/**
 * Make a null move (pass turn)
 */
void make_null_move(void)
{
    // Update en passant
    if (en_passant != no_sq)
        hash_key ^= enpassant_keys[en_passant];
    en_passant = no_sq;

    // Change side
    side ^= 1;
    hash_key ^= side_key;
}

/**
 * Unmake a null move
 */
void unmake_null_move(void)
{
    // Change side back
    side ^= 1;
    hash_key ^= side_key;
}
