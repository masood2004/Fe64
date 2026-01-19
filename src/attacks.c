// ============================================ \\
//       FE64 CHESS ENGINE - ATTACK TABLES      \\
// ============================================ \\

#include "types.h"

// External declarations from bitboard.c
extern const U64 not_a_file;
extern const U64 not_h_file;
extern const U64 not_ab_file;
extern const U64 not_gh_file;
extern unsigned int random_state;
extern U64 generate_magic_candidate();

// ============================================ \\
//           LEAPER ATTACK GENERATION           \\
// ============================================ \\

U64 mask_pawn_attacks(int side, int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    if (!side)
    { // WHITE
        if ((bitboard >> 7) & not_a_file)
            attacks |= (bitboard >> 7);
        if ((bitboard >> 9) & not_h_file)
            attacks |= (bitboard >> 9);
    }
    else
    { // BLACK
        if ((bitboard << 9) & not_a_file)
            attacks |= (bitboard << 9);
        if ((bitboard << 7) & not_h_file)
            attacks |= (bitboard << 7);
    }
    return attacks;
}

U64 mask_knight_attacks(int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

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

U64 mask_king_attacks(int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    if (bitboard >> 8)
        attacks |= (bitboard >> 8);
    if ((bitboard >> 9) & not_h_file)
        attacks |= (bitboard >> 9);
    if ((bitboard >> 7) & not_a_file)
        attacks |= (bitboard >> 7);
    if ((bitboard >> 1) & not_h_file)
        attacks |= (bitboard >> 1);
    if (bitboard << 8)
        attacks |= (bitboard << 8);
    if ((bitboard << 9) & not_a_file)
        attacks |= (bitboard << 9);
    if ((bitboard << 7) & not_h_file)
        attacks |= (bitboard << 7);
    if ((bitboard << 1) & not_a_file)
        attacks |= (bitboard << 1);

    return attacks;
}

void init_leapers_attacks()
{
    for (int square = 0; square < 64; square++)
    {
        pawn_attacks[white][square] = mask_pawn_attacks(white, square);
        pawn_attacks[black][square] = mask_pawn_attacks(black, square);
        knight_attacks[square] = mask_knight_attacks(square);
        king_attacks[square] = mask_king_attacks(square);
    }
}

// ============================================ \\
//           SLIDING PIECE ATTACKS              \\
// ============================================ \\

U64 get_bishop_attacks(int square, U64 block)
{
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1, f = tf + 1; r < 8 && f < 8; r++, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f < 8; r--, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }
    for (r = tr + 1, f = tf - 1; r < 8 && f >= 0; r++, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }
    return attacks;
}

U64 get_rook_attacks(int square, U64 block)
{
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r < 8; r++)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }
    for (r = tr - 1; r >= 0; r--)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }
    for (f = tf + 1; f < 8; f++)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }
    for (f = tf - 1; f >= 0; f--)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }
    return attacks;
}

U64 get_queen_attacks(int square, U64 block)
{
    return get_bishop_attacks(square, block) | get_rook_attacks(square, block);
}

// ============================================ \\
//           MAGIC BITBOARD HELPERS             \\
// ============================================ \\

U64 mask_bishop_attacks_occupancy(int square)
{
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

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

U64 mask_rook_attacks_occupancy(int square)
{
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r < 7; r++)
        attacks |= (1ULL << (r * 8 + tf));
    for (r = tr - 1; r > 0; r--)
        attacks |= (1ULL << (r * 8 + tf));
    for (f = tf + 1; f < 7; f++)
        attacks |= (1ULL << (tr * 8 + f));
    for (f = tf - 1; f > 0; f--)
        attacks |= (1ULL << (tr * 8 + f));

    return attacks;
}

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask)
{
    U64 occupancy = 0ULL;
    for (int count = 0; count < bits_in_mask; count++)
    {
        int square = get_ls1b_index(attack_mask);
        pop_bit(attack_mask, square);
        if (index & (1 << count))
        {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

U64 find_magic_number(int square, int relevant_bits, int bishop)
{
    U64 occupancies_arr[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];

    U64 attack_mask = bishop ? mask_bishop_attacks_occupancy(square) : mask_rook_attacks_occupancy(square);
    int occupancy_indices = 1 << relevant_bits;

    for (int index = 0; index < occupancy_indices; index++)
    {
        occupancies_arr[index] = set_occupancy(index, relevant_bits, attack_mask);
        attacks[index] = bishop ? get_bishop_attacks(square, occupancies_arr[index])
                                : get_rook_attacks(square, occupancies_arr[index]);
    }

    for (int random_count = 0; random_count < 100000000; random_count++)
    {
        U64 magic_number = generate_magic_candidate();

        for (int i = 0; i < 4096; i++)
            used_attacks[i] = 0ULL;

        int index, fail = 0;
        for (index = 0; index < occupancy_indices; index++)
        {
            int magic_index = (int)((occupancies_arr[index] * magic_number) >> (64 - relevant_bits));
            if (used_attacks[magic_index] == 0ULL)
            {
                used_attacks[magic_index] = attacks[index];
            }
            else if (used_attacks[magic_index] != attacks[index])
            {
                fail = 1;
                break;
            }
        }
        if (!fail)
            return magic_number;
    }
    printf("Magic number search failed!\n");
    return 0ULL;
}

void init_sliders_attacks(int bishop)
{
    for (int square = 0; square < 64; square++)
    {
        bishop_masks[square] = mask_bishop_attacks_occupancy(square);
        rook_masks[square] = mask_rook_attacks_occupancy(square);

        U64 attack_mask = bishop ? bishop_masks[square] : rook_masks[square];
        int relevant_bits_count = count_bits(attack_mask);

        U64 magic_number = find_magic_number(square, relevant_bits_count, bishop);

        if (bishop)
            bishop_magic_numbers[square] = magic_number;
        else
            rook_magic_numbers[square] = magic_number;

        int occupancy_indices = 1 << relevant_bits_count;
        for (int index = 0; index < occupancy_indices; index++)
        {
            U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
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

// ============================================ \\
//           ATTACK LOOKUP MACROS               \\
// ============================================ \\

U64 get_bishop_attacks_magic(int square, U64 occupancy)
{
    return bishop_attacks_table[square][((occupancy & bishop_masks[square]) * bishop_magic_numbers[square]) >> (64 - count_bits(bishop_masks[square]))];
}

U64 get_rook_attacks_magic(int square, U64 occupancy)
{
    return rook_attacks_table[square][((occupancy & rook_masks[square]) * rook_magic_numbers[square]) >> (64 - count_bits(rook_masks[square]))];
}

// ============================================ \\
//           SQUARE ATTACK CHECK                \\
// ============================================ \\

int is_square_attacked(int square, int side_attacking)
{
    // Pawn attacks
    if ((side_attacking == white) && (pawn_attacks[black][square] & bitboards[P]))
        return 1;
    if ((side_attacking == black) && (pawn_attacks[white][square] & bitboards[p]))
        return 1;

    // Knight attacks
    if (knight_attacks[square] & ((side_attacking == white) ? bitboards[N] : bitboards[n]))
        return 1;

    // King attacks
    if (king_attacks[square] & ((side_attacking == white) ? bitboards[K] : bitboards[k]))
        return 1;

    // Diagonal attacks (Bishop + Queen)
    U64 diagonal_attackers = (side_attacking == white) ? (bitboards[B] | bitboards[Q]) : (bitboards[b] | bitboards[q]);
    if (get_bishop_attacks_magic(square, occupancies[both]) & diagonal_attackers)
        return 1;

    // Straight attacks (Rook + Queen)
    U64 straight_attackers = (side_attacking == white) ? (bitboards[R] | bitboards[Q]) : (bitboards[r] | bitboards[q]);
    if (get_rook_attacks_magic(square, occupancies[both]) & straight_attackers)
        return 1;

    return 0;
}
