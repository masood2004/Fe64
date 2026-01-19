/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file search.c
 * @brief Search algorithm with advanced pruning
 * @author Syed Masood
 * @version 4.0.0
 *
 * Features:
 * - Principal Variation Search (PVS)
 * - Iterative Deepening
 * - Aspiration Windows
 * - Null Move Pruning
 * - Late Move Reductions (LMR)
 * - Futility Pruning
 * - History Heuristic
 * - Killer Moves
 * - Counter Moves
 * - Pondering with proper state management
 */

#include "include/search.h"
#include "include/board.h"
#include "include/move.h"
#include "include/hash.h"
#include "include/evaluate.h"

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

// ============================================
//          SEARCH TABLES & GLOBALS
// ============================================

// Principal Variation
int pv_length[MAX_PLY];
Move pv_table[MAX_PLY][MAX_PLY];

// Killer moves (2 per ply)
Move killer_moves[2][MAX_PLY];

// History heuristic
int history_moves[12][64];

// Counter moves
Move counter_moves[12][64];

// Late Move Reduction table
int lmr_table[64][64];

// Search info
int ply;
long nodes;
long qnodes;
int follow_pv, score_pv;
int null_pruning_allowed;

// Time management - CRITICAL FOR PONDERING
int time_limit;
long start_time;
int stop_search;

// Pondering state - FIXED
volatile int pondering;
volatile int stop_pondering;
volatile int ponder_hit;
Move ponder_move;

// UCI options
int use_pondering;
int multi_pv;

// ============================================
//          TIME MANAGEMENT
// ============================================

/**
 * Get current time in milliseconds
 */
long get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * Check if time is up (called frequently during search)
 * CRITICAL: Proper handling during pondering
 */
static inline int time_over(void)
{
    // During pondering, we DON'T stop based on time
    // We only stop if stop_pondering is set
    if (pondering)
    {
        if (stop_pondering || ponder_hit)
        {
            // Ponderhit received - switch to normal time control
            if (ponder_hit)
            {
                pondering = 0;
                // Recalculate time from ponderhit moment
                start_time = get_time_ms();
            }
            return stop_pondering && !ponder_hit;
        }
        return 0; // Keep searching while pondering
    }

    // Normal time control
    if ((nodes & 2047) == 0)
    {
        if (time_limit > 0 && get_time_ms() - start_time >= time_limit)
        {
            stop_search = 1;
            return 1;
        }
    }
    return stop_search;
}

/**
 * Calculate time for this move
 */
void calculate_time(int wtime, int btime, int winc, int binc, int movestogo)
{
    int time_left = (side == WHITE) ? wtime : btime;
    int inc = (side == WHITE) ? winc : binc;

    if (movestogo == 0)
        movestogo = 40; // Assume 40 moves to go

    // Base time allocation
    time_limit = time_left / movestogo + inc - 50; // 50ms safety margin

    // Don't use more than 1/3 of remaining time
    if (time_limit > time_left / 3)
        time_limit = time_left / 3;

    // Minimum time
    if (time_limit < 100)
        time_limit = 100;

    // Maximum time (don't think forever)
    if (time_limit > 60000)
        time_limit = 60000;
}

// ============================================
//          LMR TABLE INITIALIZATION
// ============================================

void init_lmr_table(void)
{
    for (int depth = 0; depth < 64; depth++)
    {
        for (int moves = 0; moves < 64; moves++)
        {
            if (depth == 0 || moves == 0)
            {
                lmr_table[depth][moves] = 0;
            }
            else
            {
                lmr_table[depth][moves] = (int)(0.75 + log(depth) * log(moves) / 2.25);
            }
        }
    }
}

// ============================================
//          MOVE SCORING
// ============================================

/**
 * Score a move for ordering
 */
int score_move(Move move)
{
    // PV move
    if (score_pv && pv_table[0][ply] == move)
    {
        score_pv = 0;
        return 20000;
    }

    // Hash move (from TT)
    // (handled separately in search)

    // Captures - MVV-LVA
    if (get_move_capture(move))
    {
        int attacker = get_move_piece(move);
        int target_sq = get_move_target(move);

        // Find victim
        int victim = P; // Default to pawn
        int start = (side == WHITE) ? p : P;
        int end = (side == WHITE) ? k : K;
        for (int piece = start; piece <= end; piece++)
        {
            if (get_bit(bitboards[piece], target_sq))
            {
                victim = piece;
                break;
            }
        }

        // MVV-LVA: Most Valuable Victim - Least Valuable Attacker
        return mvv_lva[attacker][victim] + 10000;
    }

    // Killer moves
    if (killer_moves[0][ply] == move)
        return 9000;
    if (killer_moves[1][ply] == move)
        return 8000;

    // Counter move
    // (would need previous move info)

    // History heuristic
    return history_moves[get_move_piece(move)][get_move_target(move)];
}

/**
 * Sort moves by score
 */
void sort_moves(MoveList *move_list)
{
    int scores[256];

    // Score all moves
    for (int i = 0; i < move_list->count; i++)
    {
        scores[i] = score_move(move_list->moves[i]);
    }

    // Selection sort (good enough for small lists)
    for (int i = 0; i < move_list->count - 1; i++)
    {
        int best = i;
        for (int j = i + 1; j < move_list->count; j++)
        {
            if (scores[j] > scores[best])
                best = j;
        }
        if (best != i)
        {
            // Swap moves
            Move temp_move = move_list->moves[i];
            move_list->moves[i] = move_list->moves[best];
            move_list->moves[best] = temp_move;

            // Swap scores
            int temp_score = scores[i];
            scores[i] = scores[best];
            scores[best] = temp_score;
        }
    }
}

/**
 * Pick best move without full sort (lazy sorting)
 */
static void pick_move(MoveList *move_list, int start_idx)
{
    int best_idx = start_idx;
    int best_score = score_move(move_list->moves[start_idx]);

    for (int i = start_idx + 1; i < move_list->count; i++)
    {
        int score = score_move(move_list->moves[i]);
        if (score > best_score)
        {
            best_score = score;
            best_idx = i;
        }
    }

    if (best_idx != start_idx)
    {
        Move temp = move_list->moves[start_idx];
        move_list->moves[start_idx] = move_list->moves[best_idx];
        move_list->moves[best_idx] = temp;
    }
}

// ============================================
//          QUIESCENCE SEARCH
// ============================================

/**
 * Quiescence search - resolve tactical positions
 */
int quiescence(int alpha, int beta)
{
    qnodes++;

    // Check time periodically
    if ((qnodes & 2047) == 0 && time_over())
        return 0;

    // Stand pat
    int stand_pat = evaluate();

    // Beta cutoff
    if (stand_pat >= beta)
        return beta;

    // Delta pruning - don't search captures that can't improve
    const int DELTA_MARGIN = 975; // Queen + safety margin
    if (stand_pat + DELTA_MARGIN < alpha)
        return alpha;

    if (stand_pat > alpha)
        alpha = stand_pat;

    // Generate captures only
    MoveList move_list;
    move_list.count = 0;
    generate_moves(&move_list);
    sort_moves(&move_list);

    for (int i = 0; i < move_list.count; i++)
    {
        // Skip non-captures
        if (!get_move_capture(move_list.moves[i]))
            continue;

        // SEE pruning - skip bad captures
        if (see(move_list.moves[i]) < 0)
            continue;

        copy_board();
        ply++;

        if (!make_move(move_list.moves[i], ALL_MOVES))
        {
            ply--;
            take_back();
            continue;
        }

        int score = -quiescence(-beta, -alpha);

        ply--;
        take_back();

        if (stop_search)
            return 0;

        if (score > alpha)
        {
            alpha = score;

            if (score >= beta)
                return beta;
        }
    }

    return alpha;
}

// ============================================
//          NEGAMAX WITH ALPHA-BETA
// ============================================

/**
 * Main negamax search with alpha-beta pruning
 */
int negamax(int alpha, int beta, int depth, int do_null)
{
    // Init PV length
    pv_length[ply] = ply;

    // Check time
    if (time_over())
        return 0;

    // Repetition/draw detection
    if (ply > 0 && (is_repetition() || fifty_move >= 100 || is_insufficient_material()))
        return 0;

    // Mate distance pruning
    if (ply > 0)
    {
        alpha = (alpha > -MATE_VALUE + ply) ? alpha : -MATE_VALUE + ply;
        beta = (beta < MATE_VALUE - ply) ? beta : MATE_VALUE - ply;
        if (alpha >= beta)
            return alpha;
    }

    // Depth limit - drop into quiescence
    if (depth <= 0)
        return quiescence(alpha, beta);

    nodes++;

    // Check extension
    int in_check_flag = in_check();
    if (in_check_flag)
        depth++;

    // Probe transposition table
    Move hash_move = 0;
    int hash_score;
    int hash_flag = tt_probe(hash_key, depth, alpha, beta, &hash_score, &hash_move);

    if (hash_flag != TT_FLAG_NONE && ply > 0)
    {
        return hash_score;
    }

    // Static evaluation for pruning decisions
    int static_eval = evaluate();

    // Reverse Futility Pruning (Static Null Move Pruning)
    if (!in_check_flag && ply > 0 && depth <= 6 && do_null)
    {
        int margin = 100 * depth;
        if (static_eval - margin >= beta)
            return static_eval - margin;
    }

    // Null Move Pruning
    if (do_null && !in_check_flag && ply > 0 && depth >= 3 &&
        static_eval >= beta && null_pruning_allowed)
    {

        // Verify we have non-pawn material
        int non_pawn = 0;
        if (side == WHITE)
            non_pawn = count_bits(bitboards[N] | bitboards[B] | bitboards[R] | bitboards[Q]);
        else
            non_pawn = count_bits(bitboards[n] | bitboards[b] | bitboards[r] | bitboards[q]);

        if (non_pawn > 0)
        {
            copy_board();
            ply++;

            // Make null move
            make_null_move();

            // Reduced depth search
            int R = 3 + depth / 6;
            if (R > 4)
                R = 4;

            int score = -negamax(-beta, -beta + 1, depth - R - 1, 0);

            ply--;
            take_back();

            if (stop_search)
                return 0;

            if (score >= beta)
            {
                // Verification search at reduced depth
                if (depth >= 12)
                {
                    score = negamax(beta - 1, beta, depth - R - 1, 0);
                    if (score >= beta)
                        return beta;
                }
                else
                {
                    return beta;
                }
            }
        }
    }

    // Razoring
    if (!in_check_flag && ply > 0 && depth <= 3)
    {
        int margin = 200 + 100 * depth;
        if (static_eval + margin < alpha)
        {
            int score = quiescence(alpha, beta);
            if (score < alpha)
                return alpha;
        }
    }

    // Generate and sort moves
    MoveList move_list;
    move_list.count = 0;
    generate_moves(&move_list);

    // Check for PV follow
    if (follow_pv)
        score_pv = 1;

    int moves_searched = 0;
    int best_score = -INF;
    Move best_move = 0;
    int flag = TT_FLAG_ALPHA;

    for (int i = 0; i < move_list.count; i++)
    {
        pick_move(&move_list, i);
        Move move = move_list.moves[i];

        copy_board();
        ply++;

        if (!make_move(move, ALL_MOVES))
        {
            ply--;
            take_back();
            continue;
        }

        int score;

        // Principal Variation Search
        if (moves_searched == 0)
        {
            // First move - full window
            score = -negamax(-beta, -alpha, depth - 1, 1);
        }
        else
        {
            // Late Move Reductions
            int reduction = 0;

            if (moves_searched >= 4 && depth >= 3 && !in_check_flag &&
                !get_move_capture(move) && !get_move_promoted(move))
            {

                reduction = lmr_table[depth][moves_searched];

                // Reduce less for killers and good history
                if (move == killer_moves[0][ply] || move == killer_moves[1][ply])
                    reduction--;
                if (history_moves[get_move_piece(move)][get_move_target(move)] > 1000)
                    reduction--;

                // Don't reduce into negative
                if (reduction < 0)
                    reduction = 0;
                if (reduction > depth - 2)
                    reduction = depth - 2;
            }

            // Zero window search with reduction
            score = -negamax(-alpha - 1, -alpha, depth - 1 - reduction, 1);

            // Re-search if needed
            if (score > alpha && reduction > 0)
            {
                score = -negamax(-alpha - 1, -alpha, depth - 1, 1);
            }

            // Full re-search if score is within window
            if (score > alpha && score < beta)
            {
                score = -negamax(-beta, -alpha, depth - 1, 1);
            }
        }

        ply--;
        take_back();

        if (stop_search)
            return 0;

        moves_searched++;

        if (score > best_score)
        {
            best_score = score;
            best_move = move;

            if (score > alpha)
            {
                alpha = score;
                flag = TT_FLAG_EXACT;

                // Update PV
                pv_table[ply][ply] = move;
                for (int j = ply + 1; j < pv_length[ply + 1]; j++)
                    pv_table[ply][j] = pv_table[ply + 1][j];
                pv_length[ply] = pv_length[ply + 1];

                // Update history for quiet moves
                if (!get_move_capture(move))
                {
                    history_moves[get_move_piece(move)][get_move_target(move)] += depth * depth;
                }

                if (score >= beta)
                {
                    // Store in TT
                    tt_store(hash_key, depth, beta, TT_FLAG_BETA, best_move);

                    // Update killers for quiet moves
                    if (!get_move_capture(move))
                    {
                        killer_moves[1][ply] = killer_moves[0][ply];
                        killer_moves[0][ply] = move;
                    }

                    return beta;
                }
            }
        }
    }

    // Checkmate or stalemate
    if (moves_searched == 0)
    {
        if (in_check_flag)
            return -MATE_VALUE + ply; // Checkmate
        else
            return 0; // Stalemate
    }

    // Store in TT
    tt_store(hash_key, depth, best_score, flag, best_move);

    return alpha;
}

// ============================================
//          ITERATIVE DEEPENING
// ============================================

/**
 * Search position with iterative deepening
 */
void search_position(int max_depth)
{
    // Reset search
    nodes = 0;
    qnodes = 0;
    stop_search = 0;
    ply = 0;
    follow_pv = 0;
    score_pv = 0;
    null_pruning_allowed = 1;

    // Clear search tables
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
    memset(pv_table, 0, sizeof(pv_table));
    memset(pv_length, 0, sizeof(pv_length));

    // Aspiration windows
    int alpha = -INF;
    int beta = INF;
    int score = 0;
    int prev_score = 0;

    start_time = get_time_ms();

    // Iterative deepening
    for (int depth = 1; depth <= max_depth; depth++)
    {
        follow_pv = 1;

        // Aspiration windows (after depth 4)
        if (depth >= 4)
        {
            int window = 25;
            alpha = prev_score - window;
            beta = prev_score + window;

            while (1)
            {
                score = negamax(alpha, beta, depth, 1);

                if (stop_search)
                    break;

                // Window failed - widen and re-search
                if (score <= alpha)
                {
                    alpha -= window * 4;
                    if (alpha < -INF)
                        alpha = -INF;
                }
                else if (score >= beta)
                {
                    beta += window * 4;
                    if (beta > INF)
                        beta = INF;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            score = negamax(alpha, beta, depth, 1);
        }

        if (stop_search)
            break;

        prev_score = score;

        // Print UCI info
        long elapsed = get_time_ms() - start_time;
        if (elapsed < 1)
            elapsed = 1;

        printf("info depth %d score ", depth);

        // Print score (mate or cp)
        if (score > MATE_VALUE - MAX_PLY)
        {
            printf("mate %d ", (MATE_VALUE - score + 1) / 2);
        }
        else if (score < -MATE_VALUE + MAX_PLY)
        {
            printf("mate %d ", -(MATE_VALUE + score) / 2);
        }
        else
        {
            printf("cp %d ", score);
        }

        printf("nodes %ld time %ld nps %ld pv ",
               nodes + qnodes, elapsed, (nodes + qnodes) * 1000 / elapsed);

        // Print PV
        for (int i = 0; i < pv_length[0]; i++)
        {
            print_move(pv_table[0][i]);
            printf(" ");
        }
        printf("\n");
        fflush(stdout);

        // Check time for next iteration
        if (!pondering && time_limit > 0)
        {
            // Don't start new iteration if we've used significant time
            if (get_time_ms() - start_time > time_limit * 0.6)
                break;
        }
    }

    // Output best move
    printf("bestmove ");
    if (pv_length[0] > 0)
    {
        print_move(pv_table[0][0]);

        // Output ponder move if available and pondering enabled
        if (use_pondering && pv_length[0] > 1)
        {
            printf(" ponder ");
            print_move(pv_table[0][1]);
            ponder_move = pv_table[0][1];
        }
    }
    else
    {
        printf("0000"); // No legal moves
    }
    printf("\n");
    fflush(stdout);
}

// ============================================
//          PONDERING
// ============================================

/**
 * Start pondering on predicted opponent move
 */
void start_pondering(void)
{
    if (!use_pondering || ponder_move == 0)
        return;

    // Make ponder move
    copy_board();
    if (!make_move(ponder_move, ALL_MOVES))
    {
        ponder_move = 0;
        return;
    }

    // Set pondering state
    pondering = 1;
    stop_pondering = 0;
    ponder_hit = 0;

    // Search with infinite time (will be stopped by ponderhit or stop)
    time_limit = 0;
    search_position(MAX_PLY);

    // Restore board
    take_back();
    pondering = 0;
}

/**
 * Handle ponderhit (opponent played predicted move)
 */
void handle_ponderhit(void)
{
    if (pondering)
    {
        ponder_hit = 1;
        // Search will switch to normal time control
    }
}

/**
 * Stop pondering
 */
void stop_pondering_search(void)
{
    if (pondering)
    {
        stop_pondering = 1;
        stop_search = 1;
    }
}

// ============================================
//          SEARCH INITIALIZATION
// ============================================

/**
 * Initialize search tables
 */
void init_search(void)
{
    init_lmr_table();

    // Clear tables
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
    memset(counter_moves, 0, sizeof(counter_moves));

    // Reset pondering state
    pondering = 0;
    stop_pondering = 0;
    ponder_hit = 0;
    ponder_move = 0;
    use_pondering = 1; // Enable by default
}

/**
 * Reset for new game
 */
void new_game(void)
{
    // Clear TT
    tt_clear();

    // Clear search tables
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_moves, 0, sizeof(history_moves));
    memset(counter_moves, 0, sizeof(counter_moves));

    // Reset pondering
    ponder_move = 0;
}
