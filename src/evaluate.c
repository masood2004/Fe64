// ============================================ \\
//       FE64 CHESS ENGINE - EVALUATION         \\
//    "The Boa Constrictor" - Slow Death Style  \\
// ============================================ \\

#include "types.h"

// External function declarations
extern U64 get_bishop_attacks_magic(int square, U64 occupancy);
extern U64 get_rook_attacks_magic(int square, U64 occupancy);
extern U64 get_queen_attacks(int square, U64 block);

// NNUE evaluation (from nnue.c)
extern int evaluate_nnue();
extern int nnue_weights_loaded();

// ============================================ \\
//           EVALUATION CONSTANTS               \\
// ============================================ \\

// Mobility bonuses
const int mobility_bonus[5] = {0, 4, 3, 2, 1};

// King attack weights
const int king_attack_weights[5] = {0, 20, 20, 40, 80};

// Pawn structure
const int doubled_pawn_penalty = 10;
const int isolated_pawn_penalty = 20;
const int rook_open_file_bonus = 25;
const int rook_semi_open_bonus = 15;
const int bishop_pair_bonus = 50;

// Boa Constrictor style
const int space_bonus_mg = 2;
const int space_bonus_eg = 1;
const int restricted_piece_penalty = 8;
const int pawn_chain_bonus = 10;
const int knight_outpost_bonus = 25;
const int bishop_outpost_bonus = 15;
const int king_tropism_bonus = 3;
const int trade_bonus_per_100cp = 5;
const int blockade_bonus = 20;
const int seventh_rank_rook_bonus = 30;
const int connected_rooks_bonus = 15;
const int pawn_shelter_bonus = 10;
const int pawn_storm_bonus = 5;

// Mop-up evaluation
const int center_manhattan_distance[64] = {
    6, 5, 4, 3, 3, 4, 5, 6,
    5, 4, 3, 2, 2, 3, 4, 5,
    4, 3, 2, 1, 1, 2, 3, 4,
    3, 2, 1, 0, 0, 1, 2, 3,
    3, 2, 1, 0, 0, 1, 2, 3,
    4, 3, 2, 1, 1, 2, 3, 4,
    5, 4, 3, 2, 2, 3, 4, 5,
    6, 5, 4, 3, 3, 4, 5, 6};

// ============================================ \\
//           HELPER FUNCTIONS                   \\
// ============================================ \\

static inline int mop_up_square_distance(int sq1, int sq2)
{
    int r1 = sq1 / 8, f1 = sq1 % 8;
    int r2 = sq2 / 8, f2 = sq2 % 8;
    int dr = abs(r1 - r2);
    int df = abs(f1 - f2);
    return (dr > df) ? dr : df;
}

int mop_up_eval(int winning_side, int losing_king_sq, int winning_king_sq)
{
    int score = 0;
    score += center_manhattan_distance[losing_king_sq] * 10;
    int king_distance = mop_up_square_distance(winning_king_sq, losing_king_sq);
    score += (14 - king_distance) * 4;
    return score;
}

int square_distance(int sq1, int sq2)
{
    int r1 = sq1 / 8, f1 = sq1 % 8;
    int r2 = sq2 / 8, f2 = sq2 % 8;
    int dr = abs(r1 - r2);
    int df = abs(f1 - f2);
    return (dr > df) ? dr : df;
}

// ============================================ \\
//           SPACE CONTROL                      \\
// ============================================ \\

int calculate_space(int color)
{
    int space = 0;
    U64 our_attacks = 0ULL;
    U64 their_territory = (color == white) ? 0x00000000FFFFFFFFULL : 0xFFFFFFFF00000000ULL;

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

// ============================================ \\
//           PIECE RESTRICTION                  \\
// ============================================ \\

int calculate_restriction(int color)
{
    int restriction = 0;
    const int avg_knight_mobility = 5;
    const int avg_bishop_mobility = 7;

    if (color == white)
    {
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

// ============================================ \\
//           PAWN STRUCTURE                     \\
// ============================================ \\

int is_outpost(int square, int color)
{
    int file = square % 8;
    int rank = square / 8;

    if (color == white && rank > 3)
        return 0;
    if (color == black && rank < 4)
        return 0;

    U64 our_pawns = (color == white) ? bitboards[P] : bitboards[p];
    U64 pawn_defenders = (color == white) ? pawn_attacks[black][square] : pawn_attacks[white][square];
    if (!(pawn_defenders & our_pawns))
        return 0;

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

int calculate_pawn_chain(int color)
{
    int bonus = 0;
    U64 pawns = (color == white) ? bitboards[P] : bitboards[p];
    U64 original_pawns = pawns;

    while (pawns)
    {
        int sq = get_ls1b_index(pawns);
        U64 defenders = (color == white) ? pawn_attacks[black][sq] : pawn_attacks[white][sq];
        if (defenders & original_pawns)
            bonus += pawn_chain_bonus;
        pop_bit(pawns, sq);
    }
    return bonus;
}

int calculate_king_tropism(int color)
{
    int tropism = 0;
    int enemy_king = (color == white) ? get_ls1b_index(bitboards[k]) : get_ls1b_index(bitboards[K]);

    U64 knights = (color == white) ? bitboards[N] : bitboards[n];
    while (knights)
    {
        int sq = get_ls1b_index(knights);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus;
        pop_bit(knights, sq);
    }

    U64 bishops = (color == white) ? bitboards[B] : bitboards[b];
    while (bishops)
    {
        int sq = get_ls1b_index(bishops);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus;
        pop_bit(bishops, sq);
    }

    U64 rooks = (color == white) ? bitboards[R] : bitboards[r];
    while (rooks)
    {
        int sq = get_ls1b_index(rooks);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus / 2;
        pop_bit(rooks, sq);
    }

    U64 queens = (color == white) ? bitboards[Q] : bitboards[q];
    while (queens)
    {
        int sq = get_ls1b_index(queens);
        tropism += (7 - square_distance(sq, enemy_king)) * king_tropism_bonus * 2;
        pop_bit(queens, sq);
    }

    return tropism;
}

int count_king_attackers(int king_square, int attacking_side)
{
    int attackers = 0;

    if (attacking_side == white)
    {
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

int is_passed_pawn(int square, int color)
{
    int file = square % 8;
    int rank = square / 8;

    if (color == white)
    {
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

// ============================================ \\
//           MAIN EVALUATION FUNCTION           \\
// ============================================ \\

int evaluate()
{
    // Try NNUE first
    if (use_nnue_eval && nnue_weights_loaded())
    {
        return evaluate_nnue();
    }

    int score = 0;
    U64 bitboard;
    int square;

    // Phase calculation
    int phase = 0;
    phase += count_bits(bitboards[N] | bitboards[n]) * 1;
    phase += count_bits(bitboards[B] | bitboards[b]) * 1;
    phase += count_bits(bitboards[R] | bitboards[r]) * 2;
    phase += count_bits(bitboards[Q] | bitboards[q]) * 4;
    int total_phase = 24;
    int phase_score = (phase * 256 + total_phase / 2) / total_phase;

    // Bishop pair
    if (count_bits(bitboards[B]) >= 2)
        score += bishop_pair_bonus;
    if (count_bits(bitboards[b]) >= 2)
        score -= bishop_pair_bonus;

    // King positions
    int white_king_sq = get_ls1b_index(bitboards[K]);
    int black_king_sq = get_ls1b_index(bitboards[k]);

    // King attackers
    int white_king_attackers = count_king_attackers(black_king_sq, white);
    int black_king_attackers = count_king_attackers(white_king_sq, black);
    score += white_king_attackers * 15;
    score -= black_king_attackers * 15;

    // Boa Constrictor evaluation
    int white_space = calculate_space(white);
    int black_space = calculate_space(black);
    score += (white_space - black_space) * space_bonus_mg;

    int white_restriction = calculate_restriction(white);
    int black_restriction = calculate_restriction(black);
    score -= white_restriction;
    score += black_restriction;

    score += calculate_pawn_chain(white);
    score -= calculate_pawn_chain(black);

    score += calculate_king_tropism(white);
    score -= calculate_king_tropism(black);

    // Trade bonus when ahead
    int material_imbalance = 0;
    for (int piece = P; piece <= k; piece++)
    {
        material_imbalance += material_weights[piece] * count_bits(bitboards[piece]);
    }
    if (abs(material_imbalance) >= 100)
    {
        int num_pieces = count_bits(occupancies[both]);
        int trade_bonus = (32 - num_pieces) * trade_bonus_per_100cp * abs(material_imbalance) / 100;
        if (material_imbalance > 0)
            score += trade_bonus;
        else
            score -= trade_bonus;
    }

    // Piece evaluation
    for (int piece = P; piece <= k; piece++)
    {
        bitboard = bitboards[piece];
        while (bitboard)
        {
            square = get_ls1b_index(bitboard);
            score += material_weights[piece];

            switch (piece)
            {
            case P:
                score += pawn_score[square];
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
                score += count_bits(knight_attacks[square] & ~occupancies[white]) * 4;
                if (is_outpost(square, white))
                    score += knight_outpost_bonus;
                break;
            case B:
                score += bishop_score[square];
                score += count_bits(get_bishop_attacks_magic(square, occupancies[both]) & ~occupancies[white]) * 5;
                if (is_outpost(square, white))
                    score += bishop_outpost_bonus;
                break;
            case R:
                score += rook_score[square];
                {
                    int file = square % 8;
                    int rank = square / 8;
                    U64 file_mask = 0x0101010101010101ULL << file;
                    if (!(file_mask & (bitboards[P] | bitboards[p])))
                        score += rook_open_file_bonus;
                    else if (!(file_mask & bitboards[P]))
                        score += rook_semi_open_bonus;
                    if (rank == 1)
                        score += seventh_rank_rook_bonus;
                }
                score += count_bits(get_rook_attacks_magic(square, occupancies[both]) & ~occupancies[white]) * 2;
                break;
            case Q:
                score += count_bits(get_queen_attacks(square, occupancies[both]) & ~occupancies[white]) * 1;
                break;
            case K:
                if (phase_score <= 128)
                    score += king_endgame_score[square];
                else
                {
                    score += king_score[square];
                    U64 shelter_mask = king_attacks[square] & bitboards[P];
                    score += count_bits(shelter_mask) * pawn_shelter_bonus;
                }
                break;
            case p:
                score -= pawn_score[square ^ 56];
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
                    if (rank == 6)
                        score -= seventh_rank_rook_bonus;
                }
                score -= count_bits(get_rook_attacks_magic(square, occupancies[both]) & ~occupancies[black]) * 2;
                break;
            case q:
                score -= count_bits(get_queen_attacks(square, occupancies[both]) & ~occupancies[black]) * 1;
                break;
            case k:
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

    // Mop-up evaluation
    if (phase_score <= 128 && abs(material_imbalance) >= 400)
    {
        if (material_imbalance > 0)
        {
            score += mop_up_eval(white, black_king_sq, white_king_sq);
        }
        else
        {
            score -= mop_up_eval(black, white_king_sq, black_king_sq);
        }
    }

    // Tempo
    score += (side == white) ? 10 : -10;

    return (side == white) ? score : -score;
}
