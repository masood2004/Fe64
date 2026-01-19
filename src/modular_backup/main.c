/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file main.c
 * @brief Main entry point and initialization
 * @author Syed Masood
 * @version 4.0.0
 *
 * Fe64 is a powerful chess engine that employs
 * "The Boa Constrictor" playing style - slowly
 * suffocating opponents through:
 *   - Superior space control
 *   - Piece restriction
 *   - Strong pawn structures
 *   - Patient positional play
 *
 * Features:
 *   - Magic bitboard move generation
 *   - Principal Variation Search with aspiration windows
 *   - Late Move Reductions
 *   - Null Move Pruning
 *   - Transposition tables
 *   - Pondering support
 *   - Multiple opening books
 *   - UCI protocol compliant
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "include/fe64.h"

// ============================================
//          VERSION INFO
// ============================================

#define FE64_VERSION "4.0.0"
#define FE64_NAME "Fe64"
#define FE64_AUTHOR "Syed Masood"
#define FE64_STYLE "The Boa Constrictor - Slow Death Style"

// ============================================
//          STARTUP BANNER
// ============================================

void print_banner(void)
{
    printf("\n");
    printf("  ███████╗███████╗ ██████╗ ██╗  ██╗\n");
    printf("  ██╔════╝██╔════╝██╔════╝ ██║  ██║\n");
    printf("  █████╗  █████╗  ██║  ███╗███████║\n");
    printf("  ██╔══╝  ██╔══╝  ██║   ██║╚════██║\n");
    printf("  ██║     ███████╗╚██████╔╝     ██║\n");
    printf("  ╚═╝     ╚══════╝ ╚═════╝      ╚═╝\n");
    printf("\n");
    printf("  %s v%s\n", FE64_NAME, FE64_VERSION);
    printf("  \"%s\"\n", FE64_STYLE);
    printf("  by %s\n", FE64_AUTHOR);
    printf("\n");
    printf("  Type 'uci' to start UCI mode\n");
    printf("  Type 'help' for commands\n");
    printf("\n");
}

// ============================================
//          HELP
// ============================================

static void print_help(void)
{
    printf("\nFe64 Chess Engine Commands:\n");
    printf("---------------------------\n");
    printf("UCI Mode:\n");
    printf("  uci        - Enter UCI mode (for GUI communication)\n");
    printf("  isready    - Check if engine is ready\n");
    printf("  ucinewgame - Start a new game\n");
    printf("  position   - Set position (startpos/fen + moves)\n");
    printf("  go         - Start searching (depth/wtime/btime/infinite)\n");
    printf("  stop       - Stop searching\n");
    printf("  ponderhit  - Opponent played predicted move\n");
    printf("  quit       - Exit engine\n");
    printf("\nDebug/Info:\n");
    printf("  d          - Display current board\n");
    printf("  eval       - Show position evaluation\n");
    printf("  book       - Show loaded opening books\n");
    printf("  perft N    - Run perft test to depth N\n");
    printf("  bench      - Run benchmark\n");
    printf("  help       - Show this help\n");
    printf("\nOptions (setoption name X value Y):\n");
    printf("  Hash       - Transposition table size (MB)\n");
    printf("  Ponder     - Enable pondering (true/false)\n");
    printf("  Book       - Use opening book (true/false)\n");
    printf("  BookPath   - Path to opening book\n");
    printf("  Contempt   - Draw contempt value\n");
    printf("\n");
}

// ============================================
//          BENCHMARK
// ============================================

static const char *bench_positions[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    NULL};

static void run_bench(void)
{
    printf("\nRunning benchmark...\n");

    long total_nodes = 0;
    long total_time = 0;
    int depth = 12;

    SearchInfo info = {0};
    info.depth = depth;
    info.stop_time = 0; // No time limit

    for (int i = 0; bench_positions[i]; i++)
    {
        printf("\nPosition %d: %s\n", i + 1, bench_positions[i]);

        parse_fen((char *)bench_positions[i]);

        nodes = 0;

        long start = get_time_ms();
        search_position(&info);
        long elapsed = get_time_ms() - start;

        long pos_nodes = nodes;
        total_nodes += pos_nodes;
        total_time += elapsed;

        printf("Nodes: %ld, Time: %ld ms, NPS: %ld\n",
               pos_nodes, elapsed, elapsed > 0 ? pos_nodes * 1000 / elapsed : 0);
    }

    printf("\n=== Benchmark Results ===\n");
    printf("Total nodes: %ld\n", total_nodes);
    printf("Total time: %ld ms\n", total_time);
    printf("Average NPS: %ld\n", total_time > 0 ? total_nodes * 1000 / total_time : 0);
    printf("=========================\n\n");
}

// ============================================
//          INITIALIZATION
// ============================================

/**
 * Initialize all engine components
 */
static void init_all(void)
{
    // Seed random number generator
    srand(time(NULL));

    // Initialize attack tables
    init_leapers_attacks();
    init_sliders_attacks(1); // Bishop
    init_sliders_attacks(0); // Rook

    // Initialize Zobrist keys
    init_hash_keys();

    // Initialize transposition table
    init_tt(128); // 128 MB default

    // Initialize search
    init_search();

    // Set starting position
    parse_fen((char *)START_FEN);
}

// ============================================
//          MAIN
// ============================================

int main(int argc, char *argv[])
{
    // Initialize engine
    init_all();

    // Check for command line arguments
    if (argc > 1)
    {
        if (strcmp(argv[1], "bench") == 0)
        {
            run_bench();
            return 0;
        }
        else if (strcmp(argv[1], "uci") == 0)
        {
            // Direct UCI mode
            printf("id name %s %s\n", FE64_NAME, FE64_VERSION);
            printf("id author %s\n", FE64_AUTHOR);
            printf("uciok\n");
            fflush(stdout);
            uci_loop();
            return 0;
        }
        else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)
        {
            printf("%s %s\n", FE64_NAME, FE64_VERSION);
            return 0;
        }
        else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
        {
            print_help();
            return 0;
        }
    }

    // Interactive mode
    print_banner();

    char input[8192];

    while (1)
    {
        printf("fe64> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        // Remove trailing newline
        int len = strlen(input);
        if (len > 0 && input[len - 1] == '\n')
            input[len - 1] = '\0';

        // Process command
        if (strcmp(input, "help") == 0)
        {
            print_help();
        }
        else if (strcmp(input, "bench") == 0)
        {
            run_bench();
        }
        else if (strcmp(input, "uci") == 0)
        {
            // Switch to UCI mode
            printf("id name %s %s\n", FE64_NAME, FE64_VERSION);
            printf("id author %s\n", FE64_AUTHOR);
            printf("uciok\n");
            fflush(stdout);
            uci_loop();
            break;
        }
        else if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
        {
            break;
        }
        else
        {
            // Unknown command - ignore
            if (strlen(input) > 0)
            {
                printf("Unknown command: %s\n", input);
            }
        }
    }

    // Cleanup
    free_tt();
    free_opening_book();
    ;

    printf("Goodbye!\n");
    return 0;
}
