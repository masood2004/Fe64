// ============================================ \\
//       FE64 CHESS ENGINE - BITBOARD OPS       \\
// ============================================ \\

#include "types.h"

// ============================================ \\
//           GLOBAL VARIABLE DEFINITIONS        \\
// ============================================ \\

// Attack Tables
U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];
U64 bishop_masks[64];
U64 rook_masks[64];
U64 bishop_attacks_table[64][512];
U64 rook_attacks_table[64][4096];
U64 bishop_magic_numbers[64];
U64 rook_magic_numbers[64];

// Board State
U64 bitboards[12];
U64 occupancies[3];
int side;
int en_passant = no_sq;
int castle;

// Zobrist Hashing
U64 piece_keys[12][64];
U64 side_key;
U64 castle_keys[16];
U64 enpassant_keys[64];
U64 hash_key;

// Repetition Detection
U64 repetition_table[MAX_GAME_MOVES];
int repetition_index = 0;

// Transposition Table
tt_entry *transposition_table = NULL;
U64 tt_num_entries = 0;
int tt_generation = 0;

// Search State
int best_move;
long long nodes;
int pv_length[MAX_PLY];
int pv_table[MAX_PLY][MAX_PLY];
int killer_moves[2][64];
int history_moves[12][64];
int counter_moves[12][64];
int butterfly_history[2][64][64];
int capture_history[12][64][6];
int last_move_made[MAX_PLY];
int lmr_table[MAX_PLY][64];
int static_eval_stack[MAX_PLY];
int excluded_move[MAX_PLY];

// Timing
long long start_time;
long long stop_time;
long long time_for_move;
int times_up = 0;

// Pondering
volatile int pondering = 0;
volatile int stop_pondering = 0;
int ponder_move = 0;
int ponder_hit = 0;
long long ponder_time_for_move = -1;
pthread_mutex_t search_mutex = PTHREAD_MUTEX_INITIALIZER;

// UCI Options
int hash_size_mb = 64;
int multi_pv = 1;
int use_nnue_eval = 0;
int contempt = 10;
int use_book = 1;
int probcut_margin = 200;

// Constants
char *start_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
char ascii_pieces[12] = "PNBRQKpnbrqk";

const int castling_rights[64] = {
    7, 15, 15, 15, 3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14};

const int see_piece_values[12] = {
    100, 337, 365, 477, 1025, 20000,
    100, 337, 365, 477, 1025, 20000};

const int lmp_margins[8] = {0, 6, 10, 15, 22, 30, 40, 52};
const int futility_margins[7] = {0, 120, 180, 240, 300, 360, 420};
const int razor_margins[4] = {0, 150, 300, 450};
const int rfp_margins[7] = {0, 80, 160, 240, 320, 400, 480};
const int history_max = 32768;

// Piece-Square Tables (tuned for strong play - Stockfish-inspired values)
const int pawn_score[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    98, 134, 61, 95, 68, 126, 34, -11,
    -6, 7, 26, 31, 65, 56, 25, -20,
    -14, 13, 6, 21, 23, 12, 17, -23,
    -27, -2, -5, 12, 17, 6, 10, -25,
    -26, -4, -4, -10, 3, 3, 33, -12,
    -35, -1, -20, -23, -15, 24, 38, -22,
    0, 0, 0, 0, 0, 0, 0, 0};

const int knight_score[64] = {
    -167, -89, -34, -49, 61, -97, -15, -107,
    -73, -41, 72, 36, 23, 62, 7, -17,
    -47, 60, 37, 65, 84, 129, 73, 44,
    -9, 17, 19, 53, 37, 69, 18, 22,
    -13, 4, 16, 13, 28, 19, 21, -8,
    -23, -9, 12, 10, 19, 17, 25, -16,
    -29, -53, -12, -3, -1, 18, -14, -19,
    -105, -21, -58, -33, -17, -28, -19, -23};

const int bishop_score[64] = {
    -29, 4, -82, -37, -25, -42, 7, -8,
    -26, 16, -18, -13, 30, 59, 18, -47,
    -16, 37, 43, 40, 35, 50, 37, -2,
    -4, 5, 19, 50, 37, 37, 7, -2,
    -6, 13, 13, 26, 34, 12, 10, 4,
    0, 15, 15, 15, 14, 27, 18, 10,
    4, 15, 16, 0, 7, 21, 33, 1,
    -33, -3, -14, -21, -13, -12, -39, -21};

const int rook_score[64] = {
    32, 42, 32, 51, 63, 9, 31, 43,
    27, 32, 58, 62, 80, 67, 26, 44,
    -5, 19, 26, 36, 17, 45, 61, 16,
    -24, -11, 7, 26, 24, 35, -8, -20,
    -36, -26, -12, -1, 9, -7, 6, -23,
    -45, -25, -16, -17, 3, 0, -5, -33,
    -44, -16, -20, -9, -1, 11, -6, -71,
    -19, -13, 1, 17, 16, 7, -37, -26};

const int king_score[64] = {
    -65, 23, 16, -15, -56, -34, 2, 13,
    29, -1, -20, -7, -8, -4, -38, -29,
    -9, 24, 2, -16, -20, 6, 22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49, -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
    1, 7, -8, -64, -43, -16, 9, 8,
    -15, 36, 12, -54, 8, -28, 24, 14};

const int king_endgame_score[64] = {
    -74, -35, -18, -18, -11, 15, 4, -17,
    -12, 17, 14, 17, 17, 38, 23, 11,
    10, 17, 23, 15, 20, 45, 44, 13,
    -8, 22, 24, 27, 26, 33, 26, 3,
    -18, -4, 21, 24, 27, 23, 9, -11,
    -19, -3, 11, 21, 23, 16, 7, -9,
    -27, -11, 4, 13, 14, 4, -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43};

const int passed_pawn_bonus[8] = {0, 140, 100, 65, 40, 20, 10, 0};
const int passed_pawn_bonus_eg[8] = {0, 250, 180, 130, 80, 40, 20, 0};

int material_weights[12] = {
    100, 337, 365, 477, 1025, 20000,
    -100, -337, -365, -477, -1025, -20000};

// File masks
const U64 not_a_file = 18374403900871474942ULL;
const U64 not_h_file = 9187201950435737471ULL;
const U64 not_ab_file = 18229723555195321596ULL;
const U64 not_gh_file = 4557430888798830399ULL;

// ============================================ \\
//           RANDOM NUMBER GENERATION           \\
// ============================================ \\

unsigned int random_state = 1804289383;

unsigned int get_random_U32_number()
{
    unsigned int number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

U64 get_random_U64_number()
{
    U64 n1 = (U64)(get_random_U32_number()) & 0xFFFF;
    U64 n2 = (U64)(get_random_U32_number()) & 0xFFFF;
    U64 n3 = (U64)(get_random_U32_number()) & 0xFFFF;
    U64 n4 = (U64)(get_random_U32_number()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

U64 generate_magic_candidate()
{
    return get_random_U64_number() & get_random_U64_number() & get_random_U64_number();
}

// ============================================ \\
//           ZOBRIST HASHING                    \\
// ============================================ \\

void init_hash_keys()
{
    random_state = 1804289383;
    for (int p = P; p <= k; p++)
        for (int s = 0; s < 64; s++)
            piece_keys[p][s] = get_random_U64_number();
    side_key = get_random_U64_number();
    for (int i = 0; i < 16; i++)
        castle_keys[i] = get_random_U64_number();
    for (int i = 0; i < 64; i++)
        enpassant_keys[i] = get_random_U64_number();
}

U64 generate_hash_key()
{
    U64 final_key = 0ULL;
    for (int p = P; p <= k; p++)
    {
        U64 bitboard = bitboards[p];
        while (bitboard)
        {
            int sq = get_ls1b_index(bitboard);
            final_key ^= piece_keys[p][sq];
            pop_bit(bitboard, sq);
        }
    }
    if (side == black)
        final_key ^= side_key;
    if (en_passant != no_sq)
        final_key ^= enpassant_keys[en_passant];
    final_key ^= castle_keys[castle];
    return final_key;
}

int is_repetition()
{
    for (int i = repetition_index - 2; i >= 0; i -= 2)
    {
        if (repetition_table[i] == hash_key)
            return 1;
    }
    return 0;
}

// ============================================ \\
//           TRANSPOSITION TABLE                \\
// ============================================ \\

void init_tt(int mb)
{
    if (transposition_table)
        free(transposition_table);

    U64 size_bytes = (U64)mb * 1024ULL * 1024ULL;
    tt_num_entries = size_bytes / sizeof(tt_entry);
    if (tt_num_entries < 1024)
        tt_num_entries = 1024;

    transposition_table = (tt_entry *)calloc(tt_num_entries, sizeof(tt_entry));
    if (!transposition_table)
    {
        // Fallback to smaller size
        tt_num_entries = TT_DEFAULT_SIZE;
        transposition_table = (tt_entry *)calloc(tt_num_entries, sizeof(tt_entry));
    }
    tt_generation = 0;
    printf("info string TT: %llu entries (%d MB)\n",
           (unsigned long long)tt_num_entries, mb);
}

void resize_tt(int mb)
{
    init_tt(mb);
}

void clear_tt()
{
    if (transposition_table && tt_num_entries > 0)
        memset(transposition_table, 0, tt_num_entries * sizeof(tt_entry));
    tt_generation = 0;
}

int read_tt(int alpha, int beta, int depth, int ply)
{
    if (!transposition_table || tt_num_entries == 0)
        return -INF - 1;
    tt_entry *entry = &transposition_table[hash_key % tt_num_entries];
    if (entry->key == hash_key)
    {
        if (entry->depth >= depth)
        {
            int score = entry->value;
            if (score > MATE - 100)
                score -= ply;
            if (score < -MATE + 100)
                score += ply;
            if (entry->flags == HASH_EXACT)
                return score;
            if (entry->flags == HASH_ALPHA && score <= alpha)
                return alpha;
            if (entry->flags == HASH_BETA && score >= beta)
                return beta;
        }
    }
    return -INF - 1;
}

int get_tt_move()
{
    if (!transposition_table || tt_num_entries == 0)
        return 0;
    tt_entry *entry = &transposition_table[hash_key % tt_num_entries];
    if (entry->key == hash_key)
        return entry->best_move;
    return 0;
}

// Get raw TT score and depth for singular extension checks
int get_tt_score_raw(int ply, int *tt_depth_out, int *tt_flags_out)
{
    if (!transposition_table || tt_num_entries == 0)
    {
        *tt_depth_out = 0;
        *tt_flags_out = 0;
        return -INF - 1;
    }
    tt_entry *entry = &transposition_table[hash_key % tt_num_entries];
    if (entry->key == hash_key)
    {
        int score = entry->value;
        if (score > MATE - 100)
            score -= ply;
        if (score < -MATE + 100)
            score += ply;
        *tt_depth_out = entry->depth;
        *tt_flags_out = entry->flags;
        return score;
    }
    *tt_depth_out = 0;
    *tt_flags_out = 0;
    return -INF - 1;
}

void write_tt(int depth, int value, int flags, int move, int ply)
{
    if (!transposition_table || tt_num_entries == 0)
        return;
    tt_entry *entry = &transposition_table[hash_key % tt_num_entries];

    // Replace if: empty, same position, deeper search, or older generation
    int should_replace = (entry->key == 0) ||
                         (entry->key == hash_key) ||
                         (depth >= entry->depth) ||
                         (flags == HASH_EXACT && entry->flags != HASH_EXACT);

    if (should_replace)
    {
        int score_to_store = value;
        if (value > MATE - 100)
            score_to_store += ply;
        if (value < -MATE + 100)
            score_to_store -= ply;
        entry->key = hash_key;
        entry->depth = depth;
        entry->flags = flags;
        entry->value = score_to_store;
        entry->best_move = move;
    }
}

// ============================================ \\
//           HELPER FUNCTIONS                   \\
// ============================================ \\

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
    printf("\n     a b c d e f g h \n\n");
    printf("     Bitboard: %llu\n\n", bitboard);
}

void print_board()
{
    printf("\n");
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            if (!file)
                printf("  %d ", 8 - rank);
            int piece = -1;
            for (int bb_piece = P; bb_piece <= k; bb_piece++)
            {
                if (get_bit(bitboards[bb_piece], square))
                {
                    piece = bb_piece;
                }
            }
            printf(" %c", (piece == -1) ? '.' : ascii_pieces[piece]);
        }
        printf("\n");
    }
    printf("\n     a b c d e f g h \n\n");
    printf("     Side:     %s\n", !side ? "white" : "black");
    printf("     EnPassant:   %s\n", (en_passant != no_sq) ? "Yes" : "no");
    printf("     Castling:  %c%c%c%c\n\n",
           (castle & wk) ? 'K' : '-',
           (castle & wq) ? 'Q' : '-',
           (castle & bk) ? 'k' : '-',
           (castle & bq) ? 'q' : '-');
}

// ============================================ \\
//           TIME MANAGEMENT                    \\
// ============================================ \\

long long get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Static buffer for non-blocking input reading
static char input_buffer[256];
static int input_buffer_pos = 0;
static int stdin_nonblocking_set = 0;

// Set stdin to non-blocking mode
void set_stdin_nonblocking()
{
#ifndef _WIN32
    if (!stdin_nonblocking_set)
    {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        stdin_nonblocking_set = 1;
    }
#endif
}

int input_waiting()
{
#ifdef _WIN32
    static int init = 0;
    static HANDLE h;
    DWORD mode;
    if (!init)
    {
        init = 1;
        h = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(h, &mode);
        SetConsoleMode(h, mode & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
        FlushConsoleInputBuffer(h);
    }
    return _kbhit();
#else
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0;
#endif
}

// Non-blocking read of a complete line
// Returns 1 if a complete line was read, 0 otherwise
// Returns -1 on EOF (stdin closed)
int read_line_nonblocking(char *output, int max_len)
{
    // Ensure stdin is non-blocking
    set_stdin_nonblocking();

    // Try to read available characters
    char c;
    while (1)
    {
        ssize_t bytes_read = read(STDIN_FILENO, &c, 1);

        if (bytes_read == 0)
        {
            // EOF - stdin closed, engine should stop
            return -1;
        }

        if (bytes_read < 0)
        {
            // No more data available (EAGAIN/EWOULDBLOCK)
            break;
        }

        if (c == '\n' || c == '\r')
        {
            if (input_buffer_pos > 0)
            {
                input_buffer[input_buffer_pos] = '\0';
                strncpy(output, input_buffer, max_len - 1);
                output[max_len - 1] = '\0';
                input_buffer_pos = 0;
                return 1;
            }
        }
        else if (input_buffer_pos < (int)sizeof(input_buffer) - 1)
        {
            input_buffer[input_buffer_pos++] = c;
        }
    }
    return 0;
}

void communicate()
{
    if (times_up)
        return;

    // ALWAYS check time first before anything else - hard time limit
    if (time_for_move != -1 && !pondering)
    {
        long long elapsed = get_time_ms() - start_time;
        // Hard cutoff: if we've exceeded our time, stop immediately
        if (elapsed > time_for_move)
        {
            times_up = 1;
            return;
        }
        // Extra hard cutoff: never exceed 3x allocated time
        if (elapsed > time_for_move * 3)
        {
            times_up = 1;
            return;
        }
    }

    char input[256];
    int result = read_line_nonblocking(input, sizeof(input));
    if (result == -1)
    {
        // EOF - stdin closed, stop engine
        times_up = 1;
        stop_pondering = 1;
        return;
    }
    if (result == 1)
    {
        if (strncmp(input, "stop", 4) == 0)
        {
            times_up = 1;
            stop_pondering = 1;
            return;
        }
        else if (strncmp(input, "quit", 4) == 0)
        {
            times_up = 1;
            stop_pondering = 1;
            return;
        }
        else if (strncmp(input, "ponderhit", 9) == 0)
        {
            ponder_hit = 1;
        }
    }

    if (stop_pondering)
    {
        times_up = 1;
        return;
    }

    if (pondering && ponder_hit && ponder_time_for_move != -1)
    {
        pondering = 0;
        time_for_move = ponder_time_for_move;
        start_time = get_time_ms();
    }
    else if (pondering && ponder_hit && ponder_time_for_move == -1)
    {
        // Ponderhit but no time saved - give ourselves a reasonable default
        pondering = 0;
        time_for_move = 10000; // 10 seconds fallback
        start_time = get_time_ms();
    }

    if (pondering)
        return;
}

// Initialize LMR table (improved reduction formula)
void init_lmr_table()
{
    for (int depth = 1; depth < MAX_PLY; depth++)
    {
        for (int moves = 1; moves < 64; moves++)
        {
            lmr_table[depth][moves] = 0.5 + log(depth) * log(moves) / 2.5;
        }
    }
}
