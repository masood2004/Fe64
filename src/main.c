// ============================================ \\
//       FE64 CHESS ENGINE - MAIN ENTRY         \\
//    "The Boa Constrictor" - Slow Death Style  \\
// ============================================ \\

#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External function declarations - Initialization
extern void init_leapers_attacks();
extern void init_sliders_attacks(int bishop);
extern void init_hash_keys();
extern void init_lmr_table();
extern U64 generate_hash_key();
extern void clear_tt();

// External function declarations - Opening Book
extern int load_opening_book(const char *filename);

// External function declarations - NNUE
extern int load_nnue(const char *filename);
extern int nnue_weights_loaded();

// External function declarations - UCI
extern void uci_loop();

// NNUE compile-time flag
#ifdef USE_NNUE
#define NNUE_ENABLED 1
#else
#define NNUE_ENABLED 0
#endif

// ============================================ \\
//              MAIN FUNCTION                   \\
// ============================================ \\

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

    // Clear search tables
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

    // Start UCI loop
    uci_loop();

    return 0;
}
