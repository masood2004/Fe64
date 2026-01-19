/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file evaluate.c
 * @brief Position evaluation with Boa Constrictor style
 * @author Syed Masood
 * @version 4.0.0
 *
 * The Boa Constrictor strategy focuses on:
 * 1. Space control - gradually suffocating opponent
 * 2. Piece restriction - limiting enemy mobility
 * 3. Pawn chains - creating strong structures
 * 4. Patient play - slow strategic pressure
 */

#include "include/evaluate.h"
#include "include/board.h"

// ============================================
//          MIRROR TABLE FOR BLACK
// ============================================

// Mirror table to flip squares for black piece-square table lookup
const int mirror_sq[64] = {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1};

// ============================================
//          PIECE VALUES (CENTIPAWNS)
// ============================================

const int piece_values[12] = {
    100, 320, 330, 500, 900, 20000, // White: P N B R Q K
    100, 320, 330, 500, 900, 20000  // Black: p n b r q k
};

const int piece_values_mg[12] = {
    82, 337, 365, 477, 1025, 0,
    82, 337, 365, 477, 1025, 0};

const int piece_values_eg[12] = {
    94, 281, 297, 512, 936, 0,
    94, 281, 297, 512, 936, 0};

// ============================================
//       PIECE-SQUARE TABLES (MIDDLEGAME)
// ============================================

// Pawn PST - encourages central pawns and advancement
const int pawn_mg[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    98, 134, 61, 95, 68, 126, 34, -11,
    -6, 7, 26, 31, 65, 56, 25, -20,
    -14, 13, 6, 21, 23, 12, 17, -23,
    -27, -2, -5, 12, 17, 6, 10, -25,
    -26, -4, -4, -10, 3, 3, 33, -12,
    -35, -1, -20, -23, -15, 24, 38, -22,
    0, 0, 0, 0, 0, 0, 0, 0};

// Knight PST - prefers central squares
const int knight_mg[64] = {
    -167, -89, -34, -49, 61, -97, -15, -107,
    -73, -41, 72, 36, 23, 62, 7, -17,
    -47, 60, 37, 65, 84, 129, 73, 44,
    -9, 17, 19, 53, 37, 69, 18, 22,
    -13, 4, 16, 13, 28, 19, 21, -8,
    -23, -9, 12, 10, 19, 17, 25, -16,
    -29, -53, -12, -3, -1, 18, -14, -19,
    -105, -21, -58, -33, -17, -28, -19, -23};

// Bishop PST - prefers long diagonals
const int bishop_mg[64] = {
    -29, 4, -82, -37, -25, -42, 7, -8,
    -26, 16, -18, -13, 30, 59, 18, -47,
    -16, 37, 43, 40, 35, 50, 37, -2,
    -4, 5, 19, 50, 37, 37, 7, -2,
    -6, 13, 13, 26, 34, 12, 10, 4,
    0, 15, 15, 15, 14, 27, 18, 10,
    4, 15, 16, 0, 7, 21, 33, 1,
    -33, -3, -14, -21, -13, -12, -39, -21};

// Rook PST - prefers 7th rank and open files
const int rook_mg[64] = {
    32, 42, 32, 51, 63, 9, 31, 43,
    27, 32, 58, 62, 80, 67, 26, 44,
    -5, 19, 26, 36, 17, 45, 61, 16,
    -24, -11, 7, 26, 24, 35, -8, -20,
    -36, -26, -12, -1, 9, -7, 6, -23,
    -45, -25, -16, -17, 3, 0, -5, -33,
    -44, -16, -20, -9, -1, 11, -6, -71,
    -19, -13, 1, 17, 16, 7, -37, -26};

// Queen PST
const int queen_mg[64] = {
    -28, 0, 29, 12, 59, 44, 43, 45,
    -24, -39, -5, 1, -16, 57, 28, 54,
    -13, -17, 7, 8, 29, 56, 47, 57,
    -27, -27, -16, -16, -1, 17, -2, 1,
    -9, -26, -9, -10, -2, -4, 3, -3,
    -14, 2, -11, -2, -5, 2, 14, 5,
    -35, -8, 11, 2, 8, 15, -3, 1,
    -1, -18, -9, 10, -15, -25, -31, -50};

// King PST (middlegame) - prefers castled position
const int king_mg[64] = {
    -65, 23, 16, -15, -56, -34, 2, 13,
    29, -1, -20, -7, -8, -4, -38, -29,
    -9, 24, 2, -16, -20, 6, 22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49, -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
    1, 7, -8, -64, -43, -16, 9, 8,
    -15, 36, 12, -54, 8, -28, 24, 14};

// ============================================
//       PIECE-SQUARE TABLES (ENDGAME)
// ============================================

const int pawn_eg[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    178, 173, 158, 134, 147, 132, 165, 187,
    94, 100, 85, 67, 56, 53, 82, 84,
    32, 24, 13, 5, -2, 4, 17, 17,
    13, 9, -3, -7, -7, -8, 3, -1,
    4, 7, -6, 1, 0, -5, -1, -8,
    13, 8, 8, 10, 13, 0, 2, -7,
    0, 0, 0, 0, 0, 0, 0, 0};

const int knight_eg[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25, -8, -25, -2, -9, -25, -24, -52,
    -24, -20, 10, 9, -1, -9, -19, -41,
    -17, 3, 22, 22, 22, 11, 8, -18,
    -18, -6, 16, 25, 16, 17, 4, -18,
    -23, -3, -1, 15, 10, -3, -20, -22,
    -42, -20, -10, -5, -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64};

const int bishop_eg[64] = {
    -14, -21, -11, -8, -7, -9, -17, -24,
    -8, -4, 7, -12, -3, -13, -4, -14,
    2, -8, 0, -1, -2, 6, 0, 4,
    -3, 9, 12, 9, 14, 10, 3, 2,
    -6, 3, 13, 19, 7, 10, -3, -9,
    -12, -3, 8, 10, 13, 3, -7, -15,
    -14, -18, -7, -1, 4, -9, -15, -27,
    -23, -9, -23, -5, -9, -16, -5, -17};

const int rook_eg[64] = {
    13, 10, 18, 15, 12, 12, 8, 5,
    11, 13, 13, 11, -3, 3, 8, 3,
    7, 7, 7, 5, 4, -3, -5, -3,
    4, 3, 13, 1, 2, 1, -1, 2,
    3, 5, 8, 4, -5, -6, -8, -11,
    -4, 0, -5, -1, -7, -12, -8, -16,
    -6, -6, 0, 2, -9, -9, -11, -3,
    -9, 2, 3, -1, -5, -13, 4, -20};

const int queen_eg[64] = {
    -9, 22, 22, 27, 27, 19, 10, 20,
    -17, 20, 32, 41, 58, 25, 30, 0,
    -20, 6, 9, 49, 47, 35, 19, 9,
    3, 22, 24, 45, 57, 40, 57, 36,
    -18, 28, 19, 47, 31, 34, 39, 23,
    -16, -27, 15, 6, 9, 17, 10, 5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43, -5, -32, -20, -41};

const int king_eg[64] = {
    -74, -35, -18, -18, -11, 15, 4, -17,
    -12, 17, 14, 17, 17, 38, 23, 11,
    10, 17, 23, 15, 20, 45, 44, 13,
    -8, 22, 24, 27, 26, 33, 26, 3,
    -18, -4, 21, 24, 27, 23, 9, -11,
    -19, -3, 11, 21, 23, 16, 7, -9,
    -27, -11, 4, 13, 14, 4, -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43};

// ============================================
//      BOA CONSTRICTOR EVALUATION BONUSES
// ============================================

// Space control bonus (by squares controlled)
const int space_bonus_mg = 5;
const int space_bonus_eg = 3;

// Mobility bonus per move available
const int mobility_bonus[6] = {0, 4, 5, 2, 1, 0}; // P N B R Q K

// Piece restriction bonus (penalty for enemy low mobility)
const int restriction_bonus = 3;

// Pawn chain bonus
const int pawn_chain_bonus = 12;
const int pawn_duo_bonus = 8;

// Outpost bonus
const int knight_outpost_bonus = 25;
const int bishop_outpost_bonus = 15;

// King safety
const int king_attack_weight[7] = {0, 0, 50, 75, 88, 94, 97};
const int king_shelter_bonus = 10;
const int pawn_shield_bonus[4] = {35, 20, 8, 0};

// Bishop pair
const int bishop_pair_mg = 50;
const int bishop_pair_eg = 65;

// Rook bonuses
const int rook_open_file = 25;
const int rook_semi_open = 15;
const int rook_7th_rank = 20;

// Passed pawns
const int passed_pawn_bonus[8] = {0, 10, 17, 35, 55, 90, 140, 0};
const int passed_pawn_king_distance = 5;

// Connected rooks
const int connected_rooks_bonus = 15;

// ============================================
//          GAME PHASE CALCULATION
// ============================================

/**
 * Calculate game phase (0 = opening, 256 = endgame)
 */
int get_game_phase(void)
{
    int phase = 0;

    // Total material excluding pawns and kings
    phase += count_bits(bitboards[N] | bitboards[n]) * 1;
    phase += count_bits(bitboards[B] | bitboards[b]) * 1;
    phase += count_bits(bitboards[R] | bitboards[r]) * 2;
    phase += count_bits(bitboards[Q] | bitboards[q]) * 4;

    // Max phase = 24 (4 minor + 4 rooks + 2 queens)
    // Scale to 256
    int max_phase = 24;
    phase = (256 * (max_phase - phase)) / max_phase;
    if (phase < 0)
        phase = 0;
    if (phase > 256)
        phase = 256;

    return phase;
}

// ============================================
//          PIECE MOBILITY
// ============================================

/**
 * Calculate mobility for a piece type
 */
static int calculate_mobility(int piece, int sq, U64 occ, U64 own_pieces)
{
    U64 attacks = 0;

    switch (piece % 6)
    {
    case 1: // Knight
        attacks = knight_attacks[sq] & ~own_pieces;
        break;
    case 2: // Bishop
        attacks = get_bishop_attacks_magic(sq, occ) & ~own_pieces;
        break;
    case 3: // Rook
        attacks = get_rook_attacks_magic(sq, occ) & ~own_pieces;
        break;
    case 4: // Queen
        attacks = get_queen_attacks(sq, occ) & ~own_pieces;
        break;
    }

    return count_bits(attacks);
}

// ============================================
//          BOA CONSTRICTOR SPECIFIC
// ============================================

/**
 * Calculate space control
 * The Boa Constrictor loves to control space!
 */
static int evaluate_space_control(int color)
{
    int score = 0;
    U64 controlled = 0ULL;

    // Define center and extended center
    U64 center = 0x00003C3C3C3C0000ULL;          // d3-e6 area
    U64 extended_center = 0x007E7E7E7E7E7E00ULL; // b2-g7 area

    // Count squares attacked by each piece type
    U64 bb, attacks;
    int sq;
    U64 occ = occupancies[BOTH];

    int pawn_piece = (color == WHITE) ? P : p;
    int knight_piece = (color == WHITE) ? N : n;
    int bishop_piece = (color == WHITE) ? B : b;
    int rook_piece = (color == WHITE) ? R : r;
    int queen_piece = (color == WHITE) ? Q : q;
    int king_piece = (color == WHITE) ? K : k;

    // Pawn control
    bb = bitboards[pawn_piece];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        controlled |= pawn_attacks[color][sq];
        pop_bit(bb, sq);
    }

    // Knight control
    bb = bitboards[knight_piece];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        controlled |= knight_attacks[sq];
        pop_bit(bb, sq);
    }

    // Bishop control
    bb = bitboards[bishop_piece];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        controlled |= get_bishop_attacks_magic(sq, occ);
        pop_bit(bb, sq);
    }

    // Rook control
    bb = bitboards[rook_piece];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        controlled |= get_rook_attacks_magic(sq, occ);
        pop_bit(bb, sq);
    }

    // Queen control
    bb = bitboards[queen_piece];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        controlled |= get_queen_attacks(sq, occ);
        pop_bit(bb, sq);
    }

    // King control
    bb = bitboards[king_piece];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        controlled |= king_attacks[sq];
        pop_bit(bb, sq);
    }

    // Score based on squares controlled
    score += count_bits(controlled & center) * 4;
    score += count_bits(controlled & extended_center) * 2;
    score += count_bits(controlled) * 1;

    return score;
}

/**
 * Evaluate pawn structure (chains, duos, weaknesses)
 */
static int evaluate_pawn_structure(int color)
{
    int score = 0;
    int pawn = (color == WHITE) ? P : p;
    U64 pawns = bitboards[pawn];

    while (pawns)
    {
        int sq = get_ls1b_index(pawns);
        int file = sq % 8;
        int rank = sq / 8;

        // Pawn chain (defended by another pawn)
        if (color == WHITE)
        {
            if (file > 0 && get_bit(bitboards[P], sq + 7))
                score += pawn_chain_bonus;
            if (file < 7 && get_bit(bitboards[P], sq + 9))
                score += pawn_chain_bonus;
        }
        else
        {
            if (file > 0 && get_bit(bitboards[p], sq - 9))
                score += pawn_chain_bonus;
            if (file < 7 && get_bit(bitboards[p], sq - 7))
                score += pawn_chain_bonus;
        }

        // Pawn duo (adjacent pawns on same rank)
        if (file < 7)
        {
            if (get_bit(bitboards[pawn], sq + 1))
                score += pawn_duo_bonus;
        }

        // Isolated pawn penalty
        U64 adjacent_files = 0ULL;
        if (file > 0)
            adjacent_files |= file_masks[file - 1];
        if (file < 7)
            adjacent_files |= file_masks[file + 1];
        if (!(bitboards[pawn] & adjacent_files))
            score -= 15;

        // Doubled pawn penalty
        int pawns_on_file = count_bits(bitboards[pawn] & file_masks[file]);
        if (pawns_on_file > 1)
            score -= 10 * (pawns_on_file - 1);

        // Backward pawn penalty
        // (Complex - simplified version)

        pop_bit(pawns, sq);
    }

    return score;
}

/**
 * Evaluate passed pawns
 */
static int evaluate_passed_pawns(int color, int phase)
{
    int score = 0;
    int pawn = (color == WHITE) ? P : p;
    int enemy_pawn = (color == WHITE) ? p : P;
    int enemy_king = (color == WHITE) ? k : K;

    U64 pawns = bitboards[pawn];

    while (pawns)
    {
        int sq = get_ls1b_index(pawns);
        int file = sq % 8;
        int rank = (color == WHITE) ? (7 - sq / 8) : (sq / 8);

        // Check if passed
        U64 front_span = 0ULL;
        if (color == WHITE)
        {
            for (int r = sq / 8 - 1; r >= 0; r--)
                front_span |= (1ULL << (r * 8 + file));
            if (file > 0)
                for (int r = sq / 8 - 1; r >= 0; r--)
                    front_span |= (1ULL << (r * 8 + file - 1));
            if (file < 7)
                for (int r = sq / 8 - 1; r >= 0; r--)
                    front_span |= (1ULL << (r * 8 + file + 1));
        }
        else
        {
            for (int r = sq / 8 + 1; r < 8; r++)
                front_span |= (1ULL << (r * 8 + file));
            if (file > 0)
                for (int r = sq / 8 + 1; r < 8; r++)
                    front_span |= (1ULL << (r * 8 + file - 1));
            if (file < 7)
                for (int r = sq / 8 + 1; r < 8; r++)
                    front_span |= (1ULL << (r * 8 + file + 1));
        }

        if (!(bitboards[enemy_pawn] & front_span))
        {
            // Passed pawn!
            int bonus = passed_pawn_bonus[rank];

            // King tropism - bonus if our king close, penalty if enemy king close
            int promo_sq = (color == WHITE) ? file : (56 + file);
            int enemy_king_sq = get_ls1b_index(bitboards[enemy_king]);
            int own_king_sq = get_ls1b_index(bitboards[(color == WHITE) ? K : k]);

            int enemy_dist = abs(enemy_king_sq % 8 - promo_sq % 8) +
                             abs(enemy_king_sq / 8 - promo_sq / 8);
            int own_dist = abs(own_king_sq % 8 - promo_sq % 8) +
                           abs(own_king_sq / 8 - promo_sq / 8);

            // In endgame, king distance matters more
            if (phase > 128)
            {
                bonus += (enemy_dist - own_dist) * passed_pawn_king_distance;
            }

            score += bonus;
        }

        pop_bit(pawns, sq);
    }

    return score;
}

/**
 * Evaluate king safety
 */
static int evaluate_king_safety(int color, int phase)
{
    // Less important in endgame
    if (phase > 200)
        return 0;

    int score = 0;
    int king = (color == WHITE) ? K : k;
    int king_sq = get_ls1b_index(bitboards[king]);
    int king_file = king_sq % 8;
    int king_rank = king_sq / 8;

    // Pawn shield
    int pawn = (color == WHITE) ? P : p;
    int shield_rank = (color == WHITE) ? king_rank - 1 : king_rank + 1;

    if (shield_rank >= 0 && shield_rank < 8)
    {
        for (int f = king_file - 1; f <= king_file + 1; f++)
        {
            if (f >= 0 && f < 8)
            {
                if (get_bit(bitboards[pawn], shield_rank * 8 + f))
                {
                    score += pawn_shield_bonus[0];
                }
                else
                {
                    // Check further
                    int far_rank = (color == WHITE) ? shield_rank - 1 : shield_rank + 1;
                    if (far_rank >= 0 && far_rank < 8)
                    {
                        if (get_bit(bitboards[pawn], far_rank * 8 + f))
                        {
                            score += pawn_shield_bonus[1];
                        }
                    }
                }
            }
        }
    }

    // Penalty for open files near king
    for (int f = king_file - 1; f <= king_file + 1; f++)
    {
        if (f >= 0 && f < 8)
        {
            if (!(bitboards[pawn] & file_masks[f]))
            {
                score -= 20; // Semi-open
                if (!(bitboards[(color == WHITE) ? p : P] & file_masks[f]))
                {
                    score -= 15; // Fully open
                }
            }
        }
    }

    return score;
}

/**
 * Evaluate piece activity and coordination
 */
static int evaluate_pieces(int color, int phase)
{
    int score_mg = 0, score_eg = 0;
    U64 occ = occupancies[BOTH];
    U64 own_pieces = (color == WHITE) ? occupancies[WHITE] : occupancies[BLACK];

    int knight = (color == WHITE) ? N : n;
    int bishop = (color == WHITE) ? B : b;
    int rook = (color == WHITE) ? R : r;
    int queen = (color == WHITE) ? Q : q;
    int pawn = (color == WHITE) ? P : p;
    int enemy_pawn = (color == WHITE) ? p : P;

    // Knights
    U64 bb = bitboards[knight];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        int mobility = calculate_mobility(knight, sq, occ, own_pieces);
        score_mg += mobility * mobility_bonus[1];
        score_eg += mobility * (mobility_bonus[1] - 1);

        // Outpost bonus
        int file = sq % 8;
        int rank = sq / 8;
        U64 outpost_mask = 0ULL;
        if (color == WHITE && rank <= 4)
        {
            if (file > 0)
                for (int r = 0; r < rank; r++)
                    outpost_mask |= (1ULL << (r * 8 + file - 1));
            if (file < 7)
                for (int r = 0; r < rank; r++)
                    outpost_mask |= (1ULL << (r * 8 + file + 1));

            if (!(bitboards[enemy_pawn] & outpost_mask))
            {
                score_mg += knight_outpost_bonus;
            }
        }

        pop_bit(bb, sq);
    }

    // Bishops
    bb = bitboards[bishop];
    int bishop_count = 0;
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        int mobility = calculate_mobility(bishop, sq, occ, own_pieces);
        score_mg += mobility * mobility_bonus[2];
        score_eg += mobility * (mobility_bonus[2] - 1);
        bishop_count++;
        pop_bit(bb, sq);
    }

    // Bishop pair bonus
    if (bishop_count >= 2)
    {
        score_mg += bishop_pair_mg;
        score_eg += bishop_pair_eg;
    }

    // Rooks
    bb = bitboards[rook];
    int prev_rook_file = -1;
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        int file = sq % 8;
        int rank = sq / 8;
        int mobility = calculate_mobility(rook, sq, occ, own_pieces);
        score_mg += mobility * mobility_bonus[3];
        score_eg += mobility * (mobility_bonus[3] + 1);

        // Open/semi-open file
        if (!(bitboards[pawn] & file_masks[file]))
        {
            if (!(bitboards[enemy_pawn] & file_masks[file]))
            {
                score_mg += rook_open_file;
                score_eg += rook_open_file;
            }
            else
            {
                score_mg += rook_semi_open;
                score_eg += rook_semi_open;
            }
        }

        // 7th rank
        if ((color == WHITE && rank == 1) || (color == BLACK && rank == 6))
        {
            score_mg += rook_7th_rank;
            score_eg += rook_7th_rank;
        }

        // Connected rooks
        if (prev_rook_file >= 0)
        {
            U64 between = get_rook_attacks_magic(sq, occ);
            if (get_bit(between, prev_rook_file))
            {
                score_mg += connected_rooks_bonus;
                score_eg += connected_rooks_bonus;
            }
        }
        prev_rook_file = sq;

        pop_bit(bb, sq);
    }

    // Queens
    bb = bitboards[queen];
    while (bb)
    {
        int sq = get_ls1b_index(bb);
        int mobility = calculate_mobility(queen, sq, occ, own_pieces);
        score_mg += mobility * mobility_bonus[4];
        score_eg += mobility * (mobility_bonus[4] + 1);
        pop_bit(bb, sq);
    }

    // Interpolate by phase
    return ((score_mg * (256 - phase)) + (score_eg * phase)) / 256;
}

// ============================================
//          MAIN EVALUATION FUNCTION
// ============================================

/**
 * Evaluate position from side to move perspective
 * Returns score in centipawns
 */
int evaluate(void)
{
    int score_mg = 0, score_eg = 0;
    int phase = get_game_phase();

    // ========================
    // Material and PST
    // ========================
    U64 bb;
    int sq;

    // White pieces
    bb = bitboards[P];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg += piece_values_mg[P] + pawn_mg[sq];
        score_eg += piece_values_eg[P] + pawn_eg[sq];
        pop_bit(bb, sq);
    }
    bb = bitboards[N];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg += piece_values_mg[N] + knight_mg[sq];
        score_eg += piece_values_eg[N] + knight_eg[sq];
        pop_bit(bb, sq);
    }
    bb = bitboards[B];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg += piece_values_mg[B] + bishop_mg[sq];
        score_eg += piece_values_eg[B] + bishop_eg[sq];
        pop_bit(bb, sq);
    }
    bb = bitboards[R];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg += piece_values_mg[R] + rook_mg[sq];
        score_eg += piece_values_eg[R] + rook_eg[sq];
        pop_bit(bb, sq);
    }
    bb = bitboards[Q];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg += piece_values_mg[Q] + queen_mg[sq];
        score_eg += piece_values_eg[Q] + queen_eg[sq];
        pop_bit(bb, sq);
    }
    bb = bitboards[K];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg += king_mg[sq];
        score_eg += king_eg[sq];
        pop_bit(bb, sq);
    }

    // Black pieces
    bb = bitboards[p];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg -= piece_values_mg[p] + pawn_mg[mirror_sq[sq]];
        score_eg -= piece_values_eg[p] + pawn_eg[mirror_sq[sq]];
        pop_bit(bb, sq);
    }
    bb = bitboards[n];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg -= piece_values_mg[n] + knight_mg[mirror_sq[sq]];
        score_eg -= piece_values_eg[n] + knight_eg[mirror_sq[sq]];
        pop_bit(bb, sq);
    }
    bb = bitboards[b];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg -= piece_values_mg[b] + bishop_mg[mirror_sq[sq]];
        score_eg -= piece_values_eg[b] + bishop_eg[mirror_sq[sq]];
        pop_bit(bb, sq);
    }
    bb = bitboards[r];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg -= piece_values_mg[r] + rook_mg[mirror_sq[sq]];
        score_eg -= piece_values_eg[r] + rook_eg[mirror_sq[sq]];
        pop_bit(bb, sq);
    }
    bb = bitboards[q];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg -= piece_values_mg[q] + queen_mg[mirror_sq[sq]];
        score_eg -= piece_values_eg[q] + queen_eg[mirror_sq[sq]];
        pop_bit(bb, sq);
    }
    bb = bitboards[k];
    while (bb)
    {
        sq = get_ls1b_index(bb);
        score_mg -= king_mg[mirror_sq[sq]];
        score_eg -= king_eg[mirror_sq[sq]];
        pop_bit(bb, sq);
    }

    // Interpolate PST/material score
    int score = ((score_mg * (256 - phase)) + (score_eg * phase)) / 256;

    // ========================
    // Boa Constrictor Bonuses
    // ========================

    // Space control
    int white_space = evaluate_space_control(WHITE);
    int black_space = evaluate_space_control(BLACK);
    score += (white_space - black_space) * space_bonus_mg * (256 - phase) / 256;
    score += (white_space - black_space) * space_bonus_eg * phase / 256;

    // Pawn structure
    score += evaluate_pawn_structure(WHITE) - evaluate_pawn_structure(BLACK);

    // Passed pawns
    score += evaluate_passed_pawns(WHITE, phase) - evaluate_passed_pawns(BLACK, phase);

    // King safety
    score += evaluate_king_safety(WHITE, phase) - evaluate_king_safety(BLACK, phase);

    // Piece activity
    score += evaluate_pieces(WHITE, phase) - evaluate_pieces(BLACK, phase);

    // ========================
    // Tempo bonus
    // ========================
    score += 15; // Bonus for side to move

    // Return from perspective of side to move
    return (side == WHITE) ? score : -score;
}
