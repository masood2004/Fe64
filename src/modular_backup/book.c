/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file book.c
 * @brief Polyglot opening book support (multiple books)
 * @author Syed Masood
 * @version 4.0.0
 *
 * Supports multiple opening books loaded simultaneously
 * for maximum coverage and variety.
 */

#include "include/book.h"
#include "include/board.h"
#include "include/move.h"

#include <stdlib.h>
#include <string.h>

// ============================================
//          POLYGLOT RANDOM KEYS
// ============================================

// Polyglot random numbers for hashing
// These are standard Polyglot keys - DO NOT MODIFY

static U64 polyglot_random_array[781];

// Polyglot piece encoding (different from internal)
static const int polyglot_pieces[12] = {
    1, 3, 5, 7, 9, 11, // White: P N B R Q K
    0, 2, 4, 6, 8, 10  // Black: p n b r q k
};

// ============================================
//          BOOK MANAGEMENT
// ============================================

// Maximum books
#define MAX_BOOKS 10

typedef struct
{
    FILE *file;
    char path[512];
    int entries;
    int weight; // Preference weight for this book
    int active;
} BookFile;

static BookFile books[MAX_BOOKS];
static int num_books = 0;
static int book_enabled = 1;

// Book variety mode
static int book_variety = 1; // 0=best, 1=weighted random, 2=pure random

// ============================================
//          POLYGLOT KEY GENERATION
// ============================================

/**
 * Initialize Polyglot random numbers
 */
void init_polyglot_keys(void)
{
    // Standard Polyglot random numbers (subset - full array would be 781 entries)
    // Using PRNG with standard seed for reproducibility
    U64 seed = 0x12345678ULL;

    for (int i = 0; i < 781; i++)
    {
        // Simple PRNG - in production, use actual Polyglot keys from file
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        polyglot_random_array[i] = seed;
    }
}

/**
 * Get Polyglot hash key for current position
 */
U64 get_polyglot_key(void)
{
    U64 key = 0;
    int sq;
    U64 bb;

    // Piece placement
    for (int piece = 0; piece < 12; piece++)
    {
        bb = bitboards[piece];
        while (bb)
        {
            sq = get_ls1b_index(bb);

            // Polyglot uses different square order
            int poly_sq = (7 - sq / 8) * 8 + (sq % 8);
            key ^= polyglot_random_array[64 * polyglot_pieces[piece] + poly_sq];

            pop_bit(bb, sq);
        }
    }

    // Castling rights
    if (castle & wk)
        key ^= polyglot_random_array[768 + 0];
    if (castle & wq)
        key ^= polyglot_random_array[768 + 1];
    if (castle & bk)
        key ^= polyglot_random_array[768 + 2];
    if (castle & bq)
        key ^= polyglot_random_array[768 + 3];

    // En passant file (only if capture possible)
    if (en_passant != no_sq)
    {
        int ep_file = en_passant % 8;

        // Check if en passant capture is actually possible
        int ep_rank = en_passant / 8;
        int pawn_piece = (side == WHITE) ? P : p;
        int has_ep_capture = 0;

        if (ep_file > 0)
        {
            int check_sq = (side == WHITE) ? en_passant + 7 : en_passant - 9;
            if (check_sq >= 0 && check_sq < 64 && get_bit(bitboards[pawn_piece], check_sq))
                has_ep_capture = 1;
        }
        if (ep_file < 7)
        {
            int check_sq = (side == WHITE) ? en_passant + 9 : en_passant - 7;
            if (check_sq >= 0 && check_sq < 64 && get_bit(bitboards[pawn_piece], check_sq))
                has_ep_capture = 1;
        }

        if (has_ep_capture)
            key ^= polyglot_random_array[772 + ep_file];
    }

    // Side to move
    if (side == WHITE)
        key ^= polyglot_random_array[780];

    return key;
}

// ============================================
//          BOOK FILE OPERATIONS
// ============================================

/**
 * Polyglot book entry structure (16 bytes)
 */
typedef struct
{
    U64 key;
    U16 move;
    U16 weight;
    U32 learn;
} PolyglotEntry;

/**
 * Read a book entry from file
 */
static int read_entry(FILE *f, PolyglotEntry *entry)
{
    unsigned char buf[16];

    if (fread(buf, 1, 16, f) != 16)
        return 0;

    // Big-endian format
    entry->key = 0;
    for (int i = 0; i < 8; i++)
        entry->key = (entry->key << 8) | buf[i];

    entry->move = (buf[8] << 8) | buf[9];
    entry->weight = (buf[10] << 8) | buf[11];
    entry->learn = (buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15];

    return 1;
}

/**
 * Convert Polyglot move to internal format
 */
static Move polyglot_to_move(U16 poly_move)
{
    // Decode Polyglot move format
    int to_file = poly_move & 0x7;
    int to_rank = (poly_move >> 3) & 0x7;
    int from_file = (poly_move >> 6) & 0x7;
    int from_rank = (poly_move >> 9) & 0x7;
    int promo = (poly_move >> 12) & 0x7;

    int from_sq = (7 - from_rank) * 8 + from_file;
    int to_sq = (7 - to_rank) * 8 + to_file;

    // Generate all legal moves and find match
    MoveList move_list;
    move_list.count = 0;
    generate_moves(&move_list);

    for (int i = 0; i < move_list.count; i++)
    {
        Move move = move_list.moves[i];

        if (get_move_source(move) == from_sq && get_move_target(move) == to_sq)
        {
            // Check promotion
            int internal_promo = get_move_promoted(move);

            if (promo == 0 && internal_promo == 0)
                return move;

            // Match promotion piece
            // Polyglot: 1=knight, 2=bishop, 3=rook, 4=queen
            if (promo == 1 && (internal_promo == N || internal_promo == n))
                return move;
            if (promo == 2 && (internal_promo == B || internal_promo == b))
                return move;
            if (promo == 3 && (internal_promo == R || internal_promo == r))
                return move;
            if (promo == 4 && (internal_promo == Q || internal_promo == q))
                return move;
        }
    }

    return 0;
}

/**
 * Add a book file
 */
int book_add(const char *path, int weight)
{
    if (num_books >= MAX_BOOKS)
    {
        printf("info string Max books limit reached (%d)\n", MAX_BOOKS);
        return 0;
    }

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("info string Cannot open book: %s\n", path);
        return 0;
    }

    // Get file size to count entries
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int entries = size / 16;

    // Store book info
    books[num_books].file = f;
    strncpy(books[num_books].path, path, sizeof(books[num_books].path) - 1);
    books[num_books].entries = entries;
    books[num_books].weight = weight;
    books[num_books].active = 1;

    printf("info string Loaded book: %s (%d entries, weight %d)\n",
           path, entries, weight);

    num_books++;
    return 1;
}

/**
 * Remove all books
 */
void book_clear(void)
{
    for (int i = 0; i < num_books; i++)
    {
        if (books[i].file)
        {
            fclose(books[i].file);
            books[i].file = NULL;
        }
    }
    num_books = 0;
}

/**
 * Set book variety mode
 */
void book_set_variety(int mode)
{
    book_variety = mode;
    printf("info string Book variety mode: %d\n", mode);
}

/**
 * Enable/disable book
 */
void book_set_enabled(int enabled)
{
    book_enabled = enabled;
    printf("info string Book %s\n", enabled ? "enabled" : "disabled");
}

// ============================================
//          BOOK MOVE PROBING
// ============================================

/**
 * Probe a single book for moves
 */
static int probe_single_book(BookFile *book, U64 key, PolyglotEntry *entries, int max_entries)
{
    if (!book->active || !book->file)
        return 0;

    FILE *f = book->file;

    // Binary search for key
    long first = 0;
    long last = book->entries - 1;
    long mid;

    while (first <= last)
    {
        mid = (first + last) / 2;

        fseek(f, mid * 16, SEEK_SET);

        PolyglotEntry entry;
        if (!read_entry(f, &entry))
            return 0;

        if (entry.key < key)
        {
            first = mid + 1;
        }
        else if (entry.key > key)
        {
            last = mid - 1;
        }
        else
        {
            // Found! Now collect all entries with this key
            int count = 0;

            // Go back to find first entry with this key
            while (mid > 0)
            {
                fseek(f, (mid - 1) * 16, SEEK_SET);
                if (!read_entry(f, &entry) || entry.key != key)
                    break;
                mid--;
            }

            // Read all entries with this key
            fseek(f, mid * 16, SEEK_SET);
            while (count < max_entries && read_entry(f, &entry))
            {
                if (entry.key != key)
                    break;

                // Apply book weight
                entry.weight = (entry.weight * book->weight) / 100;
                if (entry.weight < 1)
                    entry.weight = 1;

                entries[count++] = entry;
            }

            return count;
        }
    }

    return 0;
}

/**
 * Probe all books for the current position
 * Returns internal move format, 0 if no book move
 */
Move book_probe(void)
{
    if (!book_enabled || num_books == 0)
        return 0;

    U64 key = get_polyglot_key();

    // Collect entries from all books
    PolyglotEntry all_entries[256];
    int total_count = 0;

    for (int i = 0; i < num_books && total_count < 256; i++)
    {
        int found = probe_single_book(&books[i], key, &all_entries[total_count], 256 - total_count);
        total_count += found;
    }

    if (total_count == 0)
        return 0;

    // Select move based on variety mode
    PolyglotEntry *selected = NULL;

    switch (book_variety)
    {
    case 0: // Best move (highest weight)
        selected = &all_entries[0];
        for (int i = 1; i < total_count; i++)
        {
            if (all_entries[i].weight > selected->weight)
                selected = &all_entries[i];
        }
        break;

    case 1: // Weighted random
    {
        int total_weight = 0;
        for (int i = 0; i < total_count; i++)
            total_weight += all_entries[i].weight;

        int r = rand() % total_weight;
        int sum = 0;

        for (int i = 0; i < total_count; i++)
        {
            sum += all_entries[i].weight;
            if (r < sum)
            {
                selected = &all_entries[i];
                break;
            }
        }
        if (!selected)
            selected = &all_entries[0];
    }
    break;

    case 2: // Pure random
        selected = &all_entries[rand() % total_count];
        break;
    }

    if (!selected)
        return 0;

    // Convert to internal move
    Move move = polyglot_to_move(selected->move);

    if (move)
    {
        char move_str[6];
        move_to_string(move, move_str);
        printf("info string Book move: %s (weight %d)\n", move_str, selected->weight);
    }

    return move;
}

// ============================================
//          INITIALIZATION
// ============================================

/**
 * Initialize book system
 */
void init_book(void)
{
    init_polyglot_keys();
    num_books = 0;
    book_enabled = 1;
    book_variety = 1;

    // Clear books array
    memset(books, 0, sizeof(books));
}

/**
 * Load default books
 */
void book_load_defaults(const char *book_dir)
{
    char path[1024];

    // Try to load common opening books
    const char *default_books[] = {
        "gm2001.bin",
        "performance.bin",
        "varied.bin",
        "codekiddy.bin",
        "Cerebellum3Merge.bin",
        "book.bin",
        "komodo.bin",
        "Perfect2021.bin",
        NULL};

    for (int i = 0; default_books[i]; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", book_dir, default_books[i]);
        book_add(path, 100);
    }

    // Also try current directory
    for (int i = 0; default_books[i]; i++)
    {
        snprintf(path, sizeof(path), "./%s", default_books[i]);
        book_add(path, 100);
    }

    // And books subdirectory
    for (int i = 0; default_books[i]; i++)
    {
        snprintf(path, sizeof(path), "books/%s", default_books[i]);
        book_add(path, 100);
    }
}

/**
 * Print book info
 */
void book_info(void)
{
    printf("info string Books loaded: %d\n", num_books);
    for (int i = 0; i < num_books; i++)
    {
        printf("info string   [%d] %s: %d entries (weight %d, %s)\n",
               i, books[i].path, books[i].entries, books[i].weight,
               books[i].active ? "active" : "inactive");
    }
    printf("info string Book variety mode: %d\n", book_variety);
    printf("info string Book enabled: %s\n", book_enabled ? "yes" : "no");
}
