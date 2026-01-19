/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file hash.c
 * @brief Zobrist hashing and transposition table
 * @author Syed Masood
 * @version 4.0.0
 */

#include "include/hash.h"
#include "include/board.h"

// ============================================
//          ZOBRIST KEYS
// ============================================

U64 piece_keys[12][64];
U64 side_key;
U64 castle_keys[16];
U64 enpassant_keys[64];

// ============================================
//          TRANSPOSITION TABLE
// ============================================

TTEntry *transposition_table = NULL;
int tt_size = TT_DEFAULT_SIZE;
int tt_generation = 0;

// ============================================
//          RANDOM NUMBER GENERATION
// ============================================

unsigned int random_state = 1804289383;

/**
 * Generate 32-bit pseudo-random number using XOR-shift
 */
unsigned int get_random_U32(void)
{
    unsigned int number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

/**
 * Generate 64-bit pseudo-random number
 */
U64 get_random_U64(void)
{
    U64 n1 = (U64)(get_random_U32()) & 0xFFFFULL;
    U64 n2 = (U64)(get_random_U32()) & 0xFFFFULL;
    U64 n3 = (U64)(get_random_U32()) & 0xFFFFULL;
    U64 n4 = (U64)(get_random_U32()) & 0xFFFFULL;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

/**
 * Generate sparse magic candidate (few bits set)
 */
U64 generate_magic_candidate(void)
{
    return get_random_U64() & get_random_U64() & get_random_U64();
}

// ============================================
//          HASH INITIALIZATION
// ============================================

/**
 * Initialize Zobrist hash keys
 */
void init_hash_keys(void)
{
    random_state = 1804289383; // Reset for consistency

    // Piece keys
    for (int piece = P; piece <= k; piece++)
        for (int square = 0; square < 64; square++)
            piece_keys[piece][square] = get_random_U64();

    // Side key
    side_key = get_random_U64();

    // Castle keys
    for (int i = 0; i < 16; i++)
        castle_keys[i] = get_random_U64();

    // En passant keys
    for (int i = 0; i < 64; i++)
        enpassant_keys[i] = get_random_U64();
}

/**
 * Generate full hash key from scratch
 */
U64 generate_hash_key(void)
{
    U64 final_key = 0ULL;

    // Hash all pieces
    for (int piece = P; piece <= k; piece++)
    {
        U64 bb = bitboards[piece];
        while (bb)
        {
            int square = get_ls1b_index(bb);
            final_key ^= piece_keys[piece][square];
            pop_bit(bb, square);
        }
    }

    // Hash side to move
    if (side == BLACK)
        final_key ^= side_key;

    // Hash en passant
    if (en_passant != no_sq)
        final_key ^= enpassant_keys[en_passant];

    // Hash castling rights
    final_key ^= castle_keys[castle];

    return final_key;
}

// ============================================
//          TRANSPOSITION TABLE FUNCTIONS
// ============================================

/**
 * Initialize transposition table
 */
void init_tt(int size_mb)
{
    // Free existing table
    if (transposition_table != NULL)
    {
        free(transposition_table);
        transposition_table = NULL;
    }

    // Calculate number of entries
    int entry_size = sizeof(TTEntry);
    tt_size = (size_mb * 1024 * 1024) / entry_size;

    // Allocate table
    transposition_table = (TTEntry *)calloc(tt_size, entry_size);

    if (transposition_table == NULL)
    {
        printf("info string Failed to allocate %d MB hash table\n", size_mb);
        // Try smaller size
        size_mb = 16;
        tt_size = (size_mb * 1024 * 1024) / entry_size;
        transposition_table = (TTEntry *)calloc(tt_size, entry_size);
    }

    if (transposition_table != NULL)
    {
        printf("info string Hash table: %d MB, %d entries\n", size_mb, tt_size);
    }

    tt_generation = 0;
}

/**
 * Clear transposition table
 */
void clear_tt(void)
{
    if (transposition_table != NULL)
    {
        memset(transposition_table, 0, tt_size * sizeof(TTEntry));
    }
    tt_generation = 0;
}

/**
 * Free transposition table memory
 */
void free_tt(void)
{
    if (transposition_table != NULL)
    {
        free(transposition_table);
        transposition_table = NULL;
    }
}

/**
 * Age the transposition table (increment generation)
 */
void age_tt(void)
{
    tt_generation++;
    if (tt_generation > 255)
        tt_generation = 0;
}

/**
 * Probe transposition table
 */
int probe_tt(int alpha, int beta, int depth, int ply, Move *tt_move)
{
    if (transposition_table == NULL)
        return -INF - 1;

    TTEntry *entry = &transposition_table[hash_key % tt_size];

    *tt_move = 0;

    if (entry->key == hash_key)
    {
        *tt_move = entry->best_move;

        if (entry->depth >= depth)
        {
            int score = entry->value;

            // Adjust mate scores for distance from root
            if (score > MATE_IN_MAX)
                score -= ply;
            else if (score < -MATE_IN_MAX)
                score += ply;

            if (entry->flags == TT_EXACT)
                return score;
            if (entry->flags == TT_ALPHA && score <= alpha)
                return alpha;
            if (entry->flags == TT_BETA && score >= beta)
                return beta;
        }
    }

    return -INF - 1; // Not found
}

/**
 * Store position in transposition table
 */
void store_tt(int depth, int value, int flags, Move move, int ply)
{
    if (transposition_table == NULL)
        return;

    TTEntry *entry = &transposition_table[hash_key % tt_size];

    // Replacement strategy: replace if deeper or newer generation
    int replace = 0;

    if (entry->key == 0)
        replace = 1;
    else if (entry->key == hash_key)
        replace = 1; // Same position
    else if (entry->age != tt_generation)
        replace = 1; // Old entry
    else if (entry->depth <= depth)
        replace = 1; // Not deeper

    if (replace)
    {
        // Adjust mate scores for storage
        int score_to_store = value;
        if (value > MATE_IN_MAX)
            score_to_store += ply;
        else if (value < -MATE_IN_MAX)
            score_to_store -= ply;

        entry->key = hash_key;
        entry->depth = depth;
        entry->flags = flags;
        entry->value = score_to_store;
        entry->best_move = move;
        entry->age = tt_generation;
    }
}

/**
 * Get best move from hash table
 */
Move get_tt_move(void)
{
    if (transposition_table == NULL)
        return 0;

    TTEntry *entry = &transposition_table[hash_key % tt_size];

    if (entry->key == hash_key)
        return entry->best_move;

    return 0;
}

/**
 * Prefetch hash entry for upcoming position
 */
void prefetch_tt(U64 key)
{
// Compiler-specific prefetch
#if defined(__GNUC__)
    __builtin_prefetch(&transposition_table[key % tt_size]);
#endif
}

/**
 * Get hash table usage (permille)
 */
int get_tt_usage(void)
{
    if (transposition_table == NULL)
        return 0;

    int used = 0;
    int sample = tt_size < 1000 ? tt_size : 1000;

    for (int i = 0; i < sample; i++)
    {
        if (transposition_table[i].key != 0)
            used++;
    }

    return (used * 1000) / sample;
}
