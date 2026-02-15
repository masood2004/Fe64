// ============================================ \\
//       FE64 CHESS ENGINE - UCI PROTOCOL       \\
//    Universal Chess Interface Handler         \\
// ============================================ \\

#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// External function declarations
extern void parse_position(char *command);
extern void print_move(int move);
extern int evaluate();
extern int negamax(int alpha, int beta, int depth, int ply);
extern int get_book_move();
extern int load_opening_book(const char *filename);
extern void free_opening_book();
extern int load_nnue(const char *filename);
extern int save_nnue(const char *filename);
extern void init_nnue_random();
extern int nnue_weights_loaded();
extern void clear_tt();
extern void resize_tt(int mb);
extern long long get_time_ms();

// ============================================ \\
//              UCI LOOP                        \\
// ============================================ \\

void uci_loop()
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    char input[2000];

    while (1)
    {
        memset(input, 0, sizeof(input));
        fflush(stdout);

        if (!fgets(input, 2000, stdin))
            continue;
        if (input[0] == '\n')
            continue;

        // Handle "stop" command
        if (strncmp(input, "stop", 4) == 0)
        {
            stop_pondering = 1;
            times_up = 1;
            continue;
        }

        // Handle "ponderhit"
        if (strncmp(input, "ponderhit", 9) == 0)
        {
            ponder_hit = 1;
            continue;
        }

        if (strncmp(input, "isready", 7) == 0)
        {
            stop_pondering = 1;
            times_up = 1;
            usleep(10000);
            pondering = 0;
            stop_pondering = 0;
            printf("readyok\n");
            fflush(stdout);
            continue;
        }
        else if (strncmp(input, "setoption", 9) == 0)
        {
            if (strstr(input, "OwnBook"))
            {
                use_book = (strstr(input, "true") != NULL);
                printf("info string Book %s\n", use_book ? "enabled" : "disabled");
            }
            else if (strstr(input, "BookFile"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    value += 6;
                    char filename[256];
                    sscanf(value, "%255s", filename);
                    load_opening_book(filename);
                }
            }
            else if (strstr(input, "UseNNUE"))
            {
                int use_nnue = (strstr(input, "true") != NULL);
                use_nnue_eval = use_nnue;
                if (use_nnue && !nnue_weights_loaded())
                {
                    printf("info string NNUE not loaded, trying nnue.bin\n");
                    if (load_nnue("nnue.bin"))
                    {
                        printf("info string NNUE enabled\n");
                    }
                    else
                    {
                        printf("info string NNUE file not found, using HCE\n");
                        use_nnue_eval = 0;
                    }
                }
                else if (use_nnue)
                {
                    printf("info string NNUE enabled\n");
                }
                else
                {
                    printf("info string NNUE disabled, using HCE\n");
                }
            }
            else if (strstr(input, "NNUEFile"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    value += 6;
                    char filename[256];
                    sscanf(value, "%255s", filename);
                    if (load_nnue(filename))
                    {
                        use_nnue_eval = 1;
                        printf("info string NNUE file loaded and enabled\n");
                    }
                }
            }
            else if (strstr(input, "Hash"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    hash_size_mb = atoi(value + 6);
                    if (hash_size_mb < 1)
                        hash_size_mb = 1;
                    if (hash_size_mb > 4096)
                        hash_size_mb = 4096;
                    resize_tt(hash_size_mb);
                    printf("info string Hash set to %d MB\n", hash_size_mb);
                }
            }
            else if (strstr(input, "Contempt"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    contempt = atoi(value + 6);
                    printf("info string Contempt set to %d cp\n", contempt);
                }
            }
            else if (strstr(input, "MultiPV"))
            {
                char *value = strstr(input, "value");
                if (value)
                {
                    multi_pv = atoi(value + 6);
                    if (multi_pv < 1)
                        multi_pv = 1;
                    if (multi_pv > 10)
                        multi_pv = 10;
                    printf("info string MultiPV set to %d\n", multi_pv);
                }
            }
            continue;
        }
        else if (strncmp(input, "position", 8) == 0)
        {
            parse_position(input);
        }
        else if (strncmp(input, "ucinewgame", 10) == 0)
        {
            parse_position("position startpos");
            clear_tt();
            tt_generation = 0;
            memset(killer_moves, 0, sizeof(killer_moves));
            memset(history_moves, 0, sizeof(history_moves));
            memset(counter_moves, 0, sizeof(counter_moves));
            memset(butterfly_history, 0, sizeof(butterfly_history));
            repetition_index = 0;
        }
        else if (strncmp(input, "go", 2) == 0)
        {
            // Reset pondering state
            stop_pondering = 0;
            ponder_hit = 0;
            times_up = 0;

            int is_ponder = (strstr(input, "ponder") != NULL);
            pondering = is_ponder;

            // Check opening book first (not when pondering)
            if (use_book && !is_ponder)
            {
                int book_move = get_book_move();
                if (book_move)
                {
                    printf("info string Book move\n");
                    printf("bestmove ");
                    print_move(book_move);
                    printf("\n");
                    fflush(stdout);
                    continue;
                }
            }

            // Parse time control parameters
            int depth = -1;
            int search_depth;
            int movestogo = 30;
            int movetime = -1;
            int time = -1;
            int inc = 0;
            char *ptr = NULL;

            if ((ptr = strstr(input, "depth")))
                depth = atoi(ptr + 6);

            int infinite = (strstr(input, "infinite") != NULL);
            // Don't set infinite for ponder - we need to calculate time for ponderhit

            if ((ptr = strstr(input, "movestogo")))
                movestogo = atoi(ptr + 10);
            if ((ptr = strstr(input, "movetime")))
                movetime = atoi(ptr + 9);

            if (side == white)
            {
                if ((ptr = strstr(input, "wtime")))
                    time = atoi(ptr + 6);
                if ((ptr = strstr(input, "winc")))
                    inc = atoi(ptr + 5);
            }
            else
            {
                if ((ptr = strstr(input, "btime")))
                    time = atoi(ptr + 6);
                if ((ptr = strstr(input, "binc")))
                    inc = atoi(ptr + 5);
            }

            // Calculate time to spend
            if (movetime != -1)
            {
                time_for_move = movetime - 50;
                if (time_for_move < 10)
                    time_for_move = 10;
            }
            else if (time != -1 && !infinite)
            {
                // Game phase estimation
                int phase = count_bits(bitboards[N] | bitboards[n]) +
                            count_bits(bitboards[B] | bitboards[b]) +
                            count_bits(bitboards[R] | bitboards[r]) * 2 +
                            count_bits(bitboards[Q] | bitboards[q]) * 4;

                int expected_moves;
                if (movestogo > 0)
                {
                    expected_moves = movestogo;
                }
                else
                {
                    // Better move estimation
                    expected_moves = 25 + phase / 2;
                    if (expected_moves > 50)
                        expected_moves = 50;
                    if (expected_moves < 15)
                        expected_moves = 15;
                }

                time_for_move = time / expected_moves;
                if (inc > 0)
                    time_for_move += inc * 3 / 4;

                // Opening bonus: spend a bit more time in complex positions
                if (phase > 18)
                    time_for_move = time_for_move * 12 / 10;

                long long max_time;
                if (time > 120000)
                    max_time = time / 4;
                else if (time > 60000)
                    max_time = time / 5;
                else if (time > 10000)
                    max_time = time / 6;
                else if (time > 3000)
                    max_time = time / 8;
                else
                    max_time = time / 10;

                if (time_for_move > max_time)
                    time_for_move = max_time;

                // Safety margin
                int safety = 30;
                if (time < 5000)
                    safety = 15;
                if (time < 2000)
                    safety = 8;
                if (time < 500)
                    safety = 3;
                time_for_move -= safety;

                if (time_for_move < 10)
                    time_for_move = 10;
            }
            else
            {
                time_for_move = -1; // Infinite
            }

            // For pondering: save calculated time for ponderhit, then set infinite
            if (is_ponder)
            {
                ponder_time_for_move = time_for_move;
                time_for_move = -1; // Infinite while pondering
            }
            else
            {
                ponder_time_for_move = -1;
            }

            if (depth == -1)
                search_depth = MAX_PLY - 1;
            else
                search_depth = depth;

            // Setup search globals
            start_time = get_time_ms();
            times_up = 0;
            nodes = 0;
            best_move = 0; // Reset best move before search
            memset(excluded_move, 0, sizeof(excluded_move));

            // Age history tables
            for (int i = 0; i < 12; i++)
            {
                for (int j = 0; j < 64; j++)
                {
                    history_moves[i][j] /= 2;
                    for (int k = 0; k < 6; k++)
                        capture_history[i][j][k] /= 2;
                }
            }
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 64; j++)
                    for (int k = 0; k < 64; k++)
                        butterfly_history[i][j][k] /= 2;

            printf("info string Time allocated: %lld ms\n", time_for_move);

            // Iterative deepening with aspiration windows
            int prev_score = 0;
            int score_stability = 0; // Tracks how stable the score is across iterations

            for (int current_depth = 1; current_depth <= search_depth; current_depth++)
            {
                // Only allow time-based exit after completing at least depth 1
                if (times_up && current_depth > 1)
                    break;

                int score;

                if (current_depth >= 5)
                {
                    int delta = 25;
                    int alpha = prev_score - delta;
                    int beta = prev_score + delta;

                    // Aspiration window loop with exponentially growing windows
                    while (1)
                    {
                        if (alpha < -INF)
                            alpha = -INF;
                        if (beta > INF)
                            beta = INF;

                        score = negamax(alpha, beta, current_depth, 0);

                        if (times_up)
                            break;

                        if (score <= alpha)
                        {
                            // Fail low - widen alpha
                            beta = (alpha + beta) / 2;
                            alpha = score - delta;
                            delta += delta / 2 + 10;
                        }
                        else if (score >= beta)
                        {
                            // Fail high - widen beta
                            beta = score + delta;
                            delta += delta / 2 + 10;
                        }
                        else
                        {
                            // Score is within window
                            break;
                        }

                        if (delta > 1000)
                        {
                            // Window too large, do full search
                            score = negamax(-INF, INF, current_depth, 0);
                            break;
                        }
                    }
                }
                else
                {
                    score = negamax(-INF, INF, current_depth, 0);
                }

                if (times_up)
                    break;

                // Track score stability for time management
                int score_diff = score - prev_score;
                if (score_diff < 0)
                    score_diff = -score_diff;
                if (score_diff > 30)
                    score_stability = 0; // Score changed significantly
                else
                    score_stability++;

                prev_score = score;

                long long elapsed = get_time_ms() - start_time;
                if (elapsed < 1)
                    elapsed = 1;
                long long nps = nodes * 1000 / elapsed;

                printf("info depth %d score ", current_depth);

                if (score > MATE - 100)
                    printf("mate %d", (MATE - score + 1) / 2);
                else if (score < -MATE + 100)
                    printf("mate %d", -(MATE + score + 1) / 2);
                else
                    printf("cp %d", score);

                printf(" nodes %lld nps %lld time %lld pv ", nodes, nps, elapsed);

                for (int i = 0; i < pv_length[0]; i++)
                {
                    print_move(pv_table[0][i]);
                    printf(" ");
                }
                printf("\n");
                fflush(stdout);

                // Soft time management
                if (time_for_move != -1)
                {
                    if (score > MATE - 100 || score < -MATE + 100)
                        continue;

                    // If score is stable, can stop earlier
                    if (score_stability >= 3 && elapsed > time_for_move * 4 / 10 && current_depth >= 8)
                        break;

                    // Normal early exit
                    if (elapsed > time_for_move * 6 / 10 && current_depth >= 8)
                        break;

                    // If score dropped significantly, allow more time
                    if (score_diff > 50 && elapsed < time_for_move * 2)
                        continue; // Keep searching

                    if (elapsed > time_for_move * 8 / 10)
                        break;
                }
            }

            // Output best move
            pondering = 0;
            printf("bestmove ");
            if (best_move)
                print_move(best_move);
            else
                printf("0000");

            if (pv_length[0] >= 2 && pv_table[0][1])
            {
                printf(" ponder ");
                print_move(pv_table[0][1]);
                ponder_move = pv_table[0][1];
            }
            printf("\n");
            fflush(stdout);
        }
        else if (strncmp(input, "quit", 4) == 0)
        {
            stop_pondering = 1;
            times_up = 1;
            break;
        }
        else if (strncmp(input, "uci", 3) == 0)
        {
            printf("id name Fe64 v4.3 - The Boa Constrictor\n");
            printf("id author Syed Masood\n");
            printf("option name Hash type spin default 64 min 1 max 4096\n");
            printf("option name Contempt type spin default 10 min -100 max 100\n");
            printf("option name MultiPV type spin default 1 min 1 max 10\n");
            printf("option name OwnBook type check default true\n");
            printf("option name BookFile type string default book.bin\n");
            printf("option name UseNNUE type check default false\n");
            printf("option name NNUEFile type string default nnue.bin\n");
            printf("option name Ponder type check default true\n");
            printf("option name SyzygyPath type string default <empty>\n");
            printf("uciok\n");
        }
        // Custom commands
        else if (strncmp(input, "loadbook", 8) == 0)
        {
            char filename[256] = "book.bin";
            sscanf(input + 9, "%255s", filename);
            load_opening_book(filename);
        }
        else if (strncmp(input, "loadnnue", 8) == 0)
        {
            char filename[256] = "nnue.bin";
            sscanf(input + 9, "%255s", filename);
            load_nnue(filename);
        }
        else if (strncmp(input, "savennue", 8) == 0)
        {
            char filename[256] = "nnue.bin";
            sscanf(input + 9, "%255s", filename);
            save_nnue(filename);
            printf("info string NNUE saved to %s\n", filename);
        }
        else if (strncmp(input, "initnnue", 8) == 0)
        {
            init_nnue_random();
            printf("info string NNUE initialized with random weights\n");
        }
        else if (strncmp(input, "eval", 4) == 0)
        {
            printf("info string Static eval: %d cp\n", evaluate());
        }
    }

    // Cleanup
    free_opening_book();
}
