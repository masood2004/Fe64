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

            int infinite = (strstr(input, "infinite") != NULL) || is_ponder;

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
                    expected_moves = 20 + phase;
                    if (expected_moves > 50)
                        expected_moves = 50;
                    if (expected_moves < 15)
                        expected_moves = 15;
                }

                time_for_move = time / expected_moves;
                if (inc > 0)
                    time_for_move += inc * 4 / 5;

                if (phase > 16)
                    time_for_move = time_for_move * 11 / 10;

                long long max_time;
                if (time > 60000)
                    max_time = time / 5;
                else if (time > 10000)
                    max_time = time / 6;
                else if (time > 3000)
                    max_time = time / 8;
                else
                    max_time = time / 10;

                if (time_for_move > max_time)
                    time_for_move = max_time;

                int safety = 30;
                if (time < 3000)
                    safety = 10;
                if (time < 1000)
                    safety = 5;
                time_for_move -= safety;

                if (time_for_move < 10)
                    time_for_move = 10;
            }
            else
            {
                time_for_move = -1; // Infinite
            }

            if (depth == -1)
                search_depth = MAX_PLY - 1;
            else
                search_depth = depth;

            // Setup search globals
            start_time = get_time_ms();
            times_up = 0;
            nodes = 0;

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
            int aspiration_window = 25;

            for (int current_depth = 1; current_depth <= search_depth; current_depth++)
            {
                if (times_up)
                    break;

                int alpha, beta;
                int score;

                if (current_depth >= 5)
                {
                    alpha = prev_score - aspiration_window;
                    beta = prev_score + aspiration_window;

                    score = negamax(alpha, beta, current_depth, 0);

                    if (!times_up && score <= alpha)
                    {
                        alpha = prev_score - aspiration_window * 4;
                        if (alpha < -INF)
                            alpha = -INF;
                        score = negamax(alpha, beta, current_depth, 0);
                    }
                    if (!times_up && score >= beta)
                    {
                        beta = prev_score + aspiration_window * 4;
                        if (beta > INF)
                            beta = INF;
                        score = negamax(alpha, beta, current_depth, 0);
                    }
                    if (!times_up && (score <= alpha || score >= beta))
                    {
                        score = negamax(-INF, INF, current_depth, 0);
                    }
                }
                else
                {
                    score = negamax(-INF, INF, current_depth, 0);
                }

                if (times_up)
                    break;

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
                    if (elapsed > time_for_move * 6 / 10 && current_depth >= 8)
                        break;
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
            printf("id name Fe64 v4.0 - The Boa Constrictor\n");
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
