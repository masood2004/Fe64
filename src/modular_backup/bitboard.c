/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file bitboard.c
 * @brief Bitboard operations and attack generation
 * @author Syed Masood
 * @version 4.0.0
 */

#include "include/bitboard.h"
#include "include/hash.h"

// ============================================
//          ATTACK TABLES (Global)
// ============================================

U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];

U64 rook_magic_numbers[64];
U64 bishop_magic_numbers[64];

U64 rook_masks[64];
U64 bishop_masks[64];

U64 rook_attacks_table[64][4096];
U64 bishop_attacks_table[64][512];

// ============================================
//          BIT MANIPULATION FUNCTIONS
// ============================================

/**
 * Count bits in a bitboard using Kernighan's Algorithm
 */
int count_bits(U64 bitboard)
{
    int count = 0;
    while (bitboard)
    {
        count++;
        bitboard &= bitboard - 1; // Reset LS1B
    }
    return count;
}

/**
 * Get Least Significant 1st Bit Index
 */
int get_ls1b_index(U64 bitboard)
{
    if (bitboard == 0)
        return -1;
    return count_bits((bitboard & -bitboard) - 1);
}

// ============================================
//          LEAPER ATTACK GENERATION
// ============================================

/**
 * Generate Pawn Attack Mask
 */
U64 mask_pawn_attacks(int side, int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    if (side == WHITE)
    {
        // White pawn attacks (move up - smaller index)
        if ((bitboard >> 7) & not_a_file)
            attacks |= (bitboard >> 7);
        if ((bitboard >> 9) & not_h_file)
            attacks |= (bitboard >> 9);
    }
    else
    {
        // Black pawn attacks (move down - larger index)
        if ((bitboard << 9) & not_a_file)
            attacks |= (bitboard << 9);
        if ((bitboard << 7) & not_h_file)
            attacks |= (bitboard << 7);
    }

    return attacks;
}

/**
 * Generate Knight Attack Mask
 */
U64 mask_knight_attacks(int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    // Knight move offsets and masks
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

/**
 * Generate King Attack Mask
 */
U64 mask_king_attacks(int square)
{
    U64 attacks = 0ULL;
    U64 bitboard = 0ULL;
    set_bit(bitboard, square);

    // King moves in all 8 directions
    if (bitboard >> 8)
        attacks |= (bitboard >> 8); // North
    if ((bitboard >> 9) & not_h_file)
        attacks |= (bitboard >> 9); // North-West
    if ((bitboard >> 7) & not_a_file)
        attacks |= (bitboard >> 7); // North-East
    if ((bitboard >> 1) & not_h_file)
        attacks |= (bitboard >> 1); // West

    if (bitboard << 8)
        attacks |= (bitboard << 8); // South
    if ((bitboard << 9) & not_a_file)
        attacks |= (bitboard << 9); // South-East
    if ((bitboard << 7) & not_h_file)
        attacks |= (bitboard << 7); // South-West
    if ((bitboard << 1) & not_a_file)
        attacks |= (bitboard << 1); // East

    return attacks;
}

// ============================================
//          SLIDING PIECE ATTACKS (On-the-fly)
// ============================================

/**
 * Calculate Bishop Attacks (Diagonal)
 */
U64 get_bishop_attacks(int square, U64 block)
{
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    // South-East
    for (r = tr + 1, f = tf + 1; r < 8 && f < 8; r++, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    // North-East
    for (r = tr - 1, f = tf + 1; r >= 0 && f < 8; r--, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    // South-West
    for (r = tr + 1, f = tf - 1; r < 8 && f >= 0; r++, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    // North-West
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block)
            break;
    }

    return attacks;
}

/**
 * Calculate Rook Attacks (Straight)
 */
U64 get_rook_attacks(int square, U64 block)
{
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    // South (Rank +)
    for (r = tr + 1; r < 8; r++)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }

    // North (Rank -)
    for (r = tr - 1; r >= 0; r--)
    {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block)
            break;
    }

    // East (File +)
    for (f = tf + 1; f < 8; f++)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }

    // West (File -)
    for (f = tf - 1; f >= 0; f--)
    {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block)
            break;
    }

    return attacks;
}

/**
 * Calculate Queen Attacks (Bishop + Rook)
 */
U64 get_queen_attacks(int square, U64 block)
{
    return get_bishop_attacks(square, block) | get_rook_attacks(square, block);
}

// ============================================
//          MAGIC BITBOARD HELPERS
// ============================================

/**
 * Mask relevant occupancy bits for Bishop (exclude edges)
 */
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

/**
 * Mask relevant occupancy bits for Rook (exclude edges)
 */
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

/**
 * Generate Occupancy Variation
 */
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask)
{
    U64 occupancy = 0ULL;

    for (int count = 0; count < bits_in_mask; count++)
    {
        int square = get_ls1b_index(attack_mask);
        pop_bit(attack_mask, square);

        if (index & (1 << count))
            occupancy |= (1ULL << square);
    }

    return occupancy;
}

/**
 * Find Magic Number for a square
 */
U64 find_magic_number(int square, int relevant_bits, int bishop)
{
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];

    U64 attack_mask = bishop ? mask_bishop_attacks_occupancy(square)
                             : mask_rook_attacks_occupancy(square);

    int occupancy_indices = 1 << relevant_bits;

    // Pre-calculate all occupancy patterns and attacks
    for (int index = 0; index < occupancy_indices; index++)
    {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        attacks[index] = bishop ? get_bishop_attacks(square, occupancies[index])
                                : get_rook_attacks(square, occupancies[index]);
    }

    // Brute force search for magic number
    for (int random_count = 0; random_count < 100000000; random_count++)
    {
        U64 magic_number = generate_magic_candidate();

        // Quick rejection test
        if (count_bits((attack_mask * magic_number) & 0xFF00000000000000ULL) < 6)
            continue;

        // Reset used attacks
        for (int i = 0; i < 4096; i++)
            used_attacks[i] = 0ULL;

        int fail = 0;

        // Test magic number against all variations
        for (int index = 0; !fail && index < occupancy_indices; index++)
        {
            int magic_index = (int)((occupancies[index] * magic_number) >> (64 - relevant_bits));

            if (used_attacks[magic_index] == 0ULL)
                used_attacks[magic_index] = attacks[index];
            else if (used_attacks[magic_index] != attacks[index])
                fail = 1;
        }

        if (!fail)
            return magic_number;
    }

    printf("Magic number search failed for square %d!\n", square);
    return 0ULL;
}

// ============================================
//          INITIALIZATION FUNCTIONS
// ============================================

/**
 * Initialize leaper piece attack tables
 */
void init_leapers_attacks(void)
{
    for (int square = 0; square < 64; square++)
    {
        pawn_attacks[WHITE][square] = mask_pawn_attacks(WHITE, square);
        pawn_attacks[BLACK][square] = mask_pawn_attacks(BLACK, square);
        knight_attacks[square] = mask_knight_attacks(square);
        king_attacks[square] = mask_king_attacks(square);
    }
}

/**
 * Initialize sliding piece attack tables (magic bitboards)
 */
void init_sliders_attacks(int bishop)
{
    for (int square = 0; square < 64; square++)
    {
        // Initialize masks
        bishop_masks[square] = mask_bishop_attacks_occupancy(square);
        rook_masks[square] = mask_rook_attacks_occupancy(square);

        U64 attack_mask = bishop ? bishop_masks[square] : rook_masks[square];
        int relevant_bits_count = count_bits(attack_mask);

        // Find magic number
        U64 magic_number = find_magic_number(square, relevant_bits_count, bishop);

        if (bishop)
            bishop_magic_numbers[square] = magic_number;
        else
            rook_magic_numbers[square] = magic_number;

        // Populate lookup table
        int occupancy_indices = 1 << relevant_bits_count;

        for (int index = 0; index < occupancy_indices; index++)
        {
            U64 occupancy = set_occupancy(index, relevant_bits_count, attack_mask);
            int magic_index = (int)((occupancy * magic_number) >> (64 - relevant_bits_count));

            if (bishop)
                bishop_attacks_table[square][magic_index] = get_bishop_attacks(square, occupancy);
            else
                rook_attacks_table[square][magic_index] = get_rook_attacks(square, occupancy);
        }
    }
}

/**
 * Initialize all attack tables
 */
void init_attack_tables(void)
{
    init_leapers_attacks();
    init_sliders_attacks(1); // Bishops
    init_sliders_attacks(0); // Rooks
}

// ============================================
//          UTILITY FUNCTIONS
// ============================================

/**
 * Print a bitboard to console (for debugging)
 */
void print_bitboard(U64 bitboard)
{
    printf("\n");

    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            if (file == 0)
                printf("  %d ", 8 - rank);

            int square = rank * 8 + file;
            printf(" %d", get_bit(bitboard, square) ? 1 : 0);
        }
        printf("\n");
    }

    printf("\n     a b c d e f g h\n\n");
    printf("     Bitboard: %lluULL\n\n", bitboard);
}
