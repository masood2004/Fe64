/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file uci.c
 * @brief Universal Chess Interface protocol handler
 * @author Syed Masood
 * @version 4.0.0
 *
 * Implements UCI protocol for communication with
 * chess GUIs and lichess-bot.
 */

#include "include/uci.h"
#include "include/board.h"
#include "include/move.h"
#include "include/search.h"
#include "include/hash.h"
#include "include/book.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

// ============================================
//          ENGINE INFO
// ============================================

#define ENGINE_NAME "Fe64"
#define ENGINE_VERSION "4.0.0"
#define ENGINE_AUTHOR "Syed Masood"
#define ENGINE_STYLE "The Boa Constrictor - Slow Death Style"

// ============================================
//          UCI OPTIONS
// ============================================

typedef struct
{
    int hash_size;       // MB
    int threads;         // Number of threads (future)
    int multi_pv;        // Multi-PV mode
    int skill_level;     // 0-20
    int ponder;          // Pondering enabled
    int book_enabled;    // Opening book enabled
    int book_variety;    // 0=best, 1=weighted, 2=random
    char book_path[512]; // Custom book path
    int contempt;        // Contempt factor
    int boa_aggression;  // Boa Constrictor aggression level
} UCIOptions;

static UCIOptions options = {
    .hash_size = 128,
    .threads = 1,
    .multi_pv = 1,
    .skill_level = 20,
    .ponder = 1,
    .book_enabled = 1,
    .book_variety = 1,
    .book_path = "",
    .contempt = 0,
    .boa_aggression = 50};

// Thread for search
static pthread_t search_thread;
static volatile int searching = 0;

// ============================================
//          INPUT PARSING HELPERS
// ============================================

/**
 * Skip whitespace
 */
static char *skip_ws(char *str)
{
    while (*str == ' ' || *str == '\t')
        str++;
    return str;
}

/**
 * Get next token
 */
static char *next_token(char *str)
{
    while (*str && *str != ' ' && *str != '\t')
        str++;
    return skip_ws(str);
}

/**
 * Parse integer value
 */
static int parse_int(char *str)
{
    return atoi(str);
}

// ============================================
//          SEARCH THREAD
// ============================================

typedef struct
{
    int depth;
    int wtime, btime;
    int winc, binc;
    int movestogo;
    int movetime;
    int infinite;
} SearchParams;

static SearchParams search_params;

/**
 * Search thread function
 */
static void *search_thread_func(void *arg)
{
    (void)arg;

    // Calculate time
    if (search_params.movetime > 0)
    {
        time_limit = search_params.movetime;
    }
    else if (!search_params.infinite)
    {
        calculate_time(search_params.wtime, search_params.btime,
                       search_params.winc, search_params.binc,
                       search_params.movestogo);
    }
    else
    {
        time_limit = 0; // Infinite
    }

    // Search
    int max_depth = search_params.depth > 0 ? search_params.depth : MAX_PLY;
    search_position(max_depth);

    searching = 0;
    return NULL;
}

// ============================================
//          UCI COMMAND HANDLERS
// ============================================

/**
 * Handle 'uci' command
 */
static void cmd_uci(void)
{
    printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
    printf("id author %s\n", ENGINE_AUTHOR);

    // Print style
    printf("info string Style: %s\n", ENGINE_STYLE);

    // Options
    printf("option name Hash type spin default 128 min 1 max 8192\n");
    printf("option name Threads type spin default 1 min 1 max 1\n");
    printf("option name Ponder type check default true\n");
    printf("option name MultiPV type spin default 1 min 1 max 500\n");
    printf("option name Skill Level type spin default 20 min 0 max 20\n");
    printf("option name Book type check default true\n");
    printf("option name BookVariety type spin default 1 min 0 max 2\n");
    printf("option name BookPath type string default \n");
    printf("option name Contempt type spin default 0 min -100 max 100\n");
    printf("option name BoaAggression type spin default 50 min 0 max 100\n");
    printf("option name UCI_AnalyseMode type check default false\n");
    printf("option name Clear Hash type button\n");

    printf("uciok\n");
    fflush(stdout);
}

/**
 * Handle 'isready' command
 */
static void cmd_isready(void)
{
    printf("readyok\n");
    fflush(stdout);
}

/**
 * Handle 'setoption' command
 */
static void cmd_setoption(char *input)
{
    char *name = strstr(input, "name ");
    if (!name)
        return;
    name += 5;

    char *value = strstr(input, "value ");
    if (value)
        value += 6;

    // Parse option name
    if (strncmp(name, "Hash", 4) == 0 && value)
    {
        options.hash_size = parse_int(value);
        if (options.hash_size < 1)
            options.hash_size = 1;
        if (options.hash_size > 8192)
            options.hash_size = 8192;
        init_tt(options.hash_size);
        printf("info string Hash set to %d MB\n", options.hash_size);
    }
    else if (strncmp(name, "Threads", 7) == 0 && value)
    {
        options.threads = parse_int(value);
        printf("info string Threads set to %d\n", options.threads);
    }
    else if (strncmp(name, "Ponder", 6) == 0 && value)
    {
        options.ponder = (strncmp(value, "true", 4) == 0);
        use_pondering = options.ponder;
        printf("info string Pondering %s\n", options.ponder ? "enabled" : "disabled");
    }
    else if (strncmp(name, "MultiPV", 7) == 0 && value)
    {
        options.multi_pv = parse_int(value);
        multi_pv = options.multi_pv;
        printf("info string MultiPV set to %d\n", options.multi_pv);
    }
    else if (strncmp(name, "Skill Level", 11) == 0 && value)
    {
        options.skill_level = parse_int(value);
        printf("info string Skill Level set to %d\n", options.skill_level);
    }
    else if (strncmp(name, "Book ", 5) == 0 && value)
    {
        options.book_enabled = (strncmp(value, "true", 4) == 0);
        book_set_enabled(options.book_enabled);
    }
    else if (strncmp(name, "BookVariety", 11) == 0 && value)
    {
        options.book_variety = parse_int(value);
        book_set_variety(options.book_variety);
    }
    else if (strncmp(name, "BookPath", 8) == 0 && value)
    {
        strncpy(options.book_path, value, sizeof(options.book_path) - 1);
        if (strlen(options.book_path) > 0)
        {
            book_add(options.book_path, 100);
        }
    }
    else if (strncmp(name, "Contempt", 8) == 0 && value)
    {
        options.contempt = parse_int(value);
        printf("info string Contempt set to %d\n", options.contempt);
    }
    else if (strncmp(name, "BoaAggression", 13) == 0 && value)
    {
        options.boa_aggression = parse_int(value);
        printf("info string Boa Aggression set to %d\n", options.boa_aggression);
    }
    else if (strncmp(name, "Clear Hash", 10) == 0)
    {
        tt_clear();
        printf("info string Hash cleared\n");
    }

    fflush(stdout);
}

/**
 * Handle 'ucinewgame' command
 */
static void cmd_ucinewgame(void)
{
    new_game();
    parse_fen(START_FEN);
    printf("info string New game started\n");
    fflush(stdout);
}

/**
 * Handle 'position' command
 */
static void cmd_position(char *input)
{
    input = skip_ws(input + 8); // Skip "position"

    // Parse position type
    if (strncmp(input, "startpos", 8) == 0)
    {
        parse_fen(START_FEN);
        input = skip_ws(input + 8);
    }
    else if (strncmp(input, "fen", 3) == 0)
    {
        input = skip_ws(input + 3);

        // Find end of FEN
        char *moves_ptr = strstr(input, "moves");
        if (moves_ptr)
        {
            // Temporarily terminate FEN string
            char saved = *(moves_ptr - 1);
            *(moves_ptr - 1) = '\0';
            parse_fen(input);
            *(moves_ptr - 1) = saved;
            input = moves_ptr;
        }
        else
        {
            parse_fen(input);
            return;
        }
    }

    // Parse moves
    char *moves = strstr(input, "moves");
    if (moves)
    {
        moves = skip_ws(moves + 5);

        while (*moves)
        {
            Move move = parse_move(moves);

            if (move == 0)
            {
                printf("info string Invalid move in position command\n");
                break;
            }

            // Store current position in repetition history
            repetition_table[repetition_index++] = hash_key;

            make_move(move, ALL_MOVES);

            // Move to next move
            while (*moves && *moves != ' ')
                moves++;
            moves = skip_ws(moves);
        }
    }
}

/**
 * Handle 'go' command
 */
static void cmd_go(char *input)
{
    // Don't start new search if already searching
    if (searching)
    {
        printf("info string Already searching\n");
        return;
    }

    // Reset search parameters
    memset(&search_params, 0, sizeof(search_params));
    search_params.depth = MAX_PLY;

    input = skip_ws(input + 2); // Skip "go"

    // Parse parameters
    while (*input)
    {
        if (strncmp(input, "depth", 5) == 0)
        {
            input = skip_ws(input + 5);
            search_params.depth = parse_int(input);
        }
        else if (strncmp(input, "wtime", 5) == 0)
        {
            input = skip_ws(input + 5);
            search_params.wtime = parse_int(input);
        }
        else if (strncmp(input, "btime", 5) == 0)
        {
            input = skip_ws(input + 5);
            search_params.btime = parse_int(input);
        }
        else if (strncmp(input, "winc", 4) == 0)
        {
            input = skip_ws(input + 4);
            search_params.winc = parse_int(input);
        }
        else if (strncmp(input, "binc", 4) == 0)
        {
            input = skip_ws(input + 4);
            search_params.binc = parse_int(input);
        }
        else if (strncmp(input, "movestogo", 9) == 0)
        {
            input = skip_ws(input + 9);
            search_params.movestogo = parse_int(input);
        }
        else if (strncmp(input, "movetime", 8) == 0)
        {
            input = skip_ws(input + 8);
            search_params.movetime = parse_int(input);
        }
        else if (strncmp(input, "infinite", 8) == 0)
        {
            search_params.infinite = 1;
            input = skip_ws(input + 8);
            continue;
        }
        else if (strncmp(input, "ponder", 6) == 0)
        {
            pondering = 1;
            input = skip_ws(input + 6);
            continue;
        }

        // Move to next parameter
        input = next_token(input);
    }

    // Try book move first (if not pondering)
    if (!pondering && options.book_enabled)
    {
        Move book_move = book_probe();
        if (book_move)
        {
            printf("bestmove ");
            print_move(book_move);
            printf("\n");
            fflush(stdout);
            return;
        }
    }

    // Start search thread
    searching = 1;
    pthread_create(&search_thread, NULL, search_thread_func, NULL);
}

/**
 * Handle 'stop' command
 */
static void cmd_stop(void)
{
    if (pondering)
    {
        stop_pondering_search();
    }
    stop_search = 1;

    // Wait for search thread to finish
    if (searching)
    {
        pthread_join(search_thread, NULL);
        searching = 0;
    }
}

/**
 * Handle 'ponderhit' command
 */
static void cmd_ponderhit(void)
{
    handle_ponderhit();
}

/**
 * Handle 'quit' command
 */
static void cmd_quit(void)
{
    // Stop any running search
    cmd_stop();

    // Clean up
    book_clear();

    exit(0);
}

/**
 * Handle 'd' command (debug - display board)
 */
static void cmd_debug(void)
{
    print_board();
    printf("\nHash key: %016llx\n", (unsigned long long)hash_key);
    printf("Side: %s\n", side == WHITE ? "white" : "black");
    printf("Castling: %c%c%c%c\n",
           (castle & wk) ? 'K' : '-',
           (castle & wq) ? 'Q' : '-',
           (castle & bk) ? 'k' : '-',
           (castle & bq) ? 'q' : '-');
    printf("En passant: %s\n", en_passant != no_sq ? square_to_coords[en_passant] : "-");
    printf("Fifty move: %d\n", fifty_move);
    fflush(stdout);
}

/**
 * Handle 'eval' command
 */
static void cmd_eval(void)
{
    int score = evaluate();
    printf("info string Static eval: %d cp (from %s's perspective)\n",
           score, side == WHITE ? "white" : "black");
    fflush(stdout);
}

/**
 * Handle 'book' command
 */
static void cmd_book(void)
{
    book_info();
    fflush(stdout);
}

/**
 * Handle 'perft' command
 */
static void cmd_perft(char *input)
{
    input = skip_ws(input + 5);
    int depth = parse_int(input);
    if (depth <= 0)
        depth = 1;

    printf("info string Perft(%d):\n", depth);

    long start = get_time_ms();
    long total_nodes = 0;

    MoveList move_list;
    move_list.count = 0;
    generate_moves(&move_list);

    for (int i = 0; i < move_list.count; i++)
    {
        copy_board();

        if (!make_move(move_list.moves[i], ALL_MOVES))
        {
            take_back();
            continue;
        }

        // Recursive perft would go here
        // For now, just count moves
        long count = 1; // Simplified

        take_back();

        print_move(move_list.moves[i]);
        printf(": %ld\n", count);
        total_nodes += count;
    }

    long elapsed = get_time_ms() - start;
    if (elapsed < 1)
        elapsed = 1;

    printf("\nTotal: %ld nodes in %ld ms (%ld nps)\n",
           total_nodes, elapsed, total_nodes * 1000 / elapsed);
    fflush(stdout);
}

// ============================================
//          MAIN UCI LOOP
// ============================================

/**
 * Process a single UCI command
 */
void uci_process_command(char *input)
{
    // Remove trailing newline
    int len = strlen(input);
    if (len > 0 && input[len - 1] == '\n')
        input[len - 1] = '\0';
    if (len > 1 && input[len - 2] == '\r')
        input[len - 2] = '\0';

    // Skip empty input
    if (strlen(input) == 0)
        return;

    // Parse command
    if (strcmp(input, "uci") == 0)
    {
        cmd_uci();
    }
    else if (strcmp(input, "isready") == 0)
    {
        cmd_isready();
    }
    else if (strncmp(input, "setoption", 9) == 0)
    {
        cmd_setoption(input);
    }
    else if (strcmp(input, "ucinewgame") == 0)
    {
        cmd_ucinewgame();
    }
    else if (strncmp(input, "position", 8) == 0)
    {
        cmd_position(input);
    }
    else if (strncmp(input, "go", 2) == 0)
    {
        cmd_go(input);
    }
    else if (strcmp(input, "stop") == 0)
    {
        cmd_stop();
    }
    else if (strcmp(input, "ponderhit") == 0)
    {
        cmd_ponderhit();
    }
    else if (strcmp(input, "quit") == 0)
    {
        cmd_quit();
    }
    else if (strcmp(input, "d") == 0)
    {
        cmd_debug();
    }
    else if (strcmp(input, "eval") == 0)
    {
        cmd_eval();
    }
    else if (strcmp(input, "book") == 0)
    {
        cmd_book();
    }
    else if (strncmp(input, "perft", 5) == 0)
    {
        cmd_perft(input);
    }
    else
    {
        // Unknown command - ignore silently (UCI spec)
    }
}

/**
 * Main UCI loop
 */
void uci_loop(void)
{
    char input[8192];

    // Disable buffering
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    while (1)
    {
        memset(input, 0, sizeof(input));

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        uci_process_command(input);
    }
}

/**
 * Initialize UCI
 */
void init_uci(void)
{
    // Initialize with default options
    init_tt(options.hash_size);
    use_pondering = options.ponder;
    multi_pv = options.multi_pv;
    book_set_enabled(options.book_enabled);
    book_set_variety(options.book_variety);
}
