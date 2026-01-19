// ============================================ \\
//       FE64 CHESS ENGINE - SEARCH             \\
//    Negamax with Alpha-Beta, LMR, PVS         \\
// ============================================ \\

#include "types.h"
#include <stdio.h>
#include <stdlib.h>

// External function declarations
extern int evaluate();
extern int is_square_attacked(int square, int attacking_side);
extern void generate_moves(moves *move_list);
extern int make_move(int move, int move_flag);
extern void print_move(int move);
extern U64 get_bishop_attacks_magic(int square, U64 occupancy);
extern U64 get_rook_attacks_magic(int square, U64 occupancy);

// Time management externals
extern void communicate();
extern int read_tt(int alpha, int beta, int depth, int ply);
extern void write_tt(int depth, int score, int flag, int best_move, int ply);
extern int get_tt_move();
extern int is_repetition();

// MVV-LVA (Most Valuable Victim - Least Valuable Attacker) scores
// [attacker][victim] - higher score = better capture
static int mvv_lva_scores[12][12] = {
    {105, 205, 305, 405, 505, 605, 105, 205, 305, 405, 505, 605}, // P
    {104, 204, 304, 404, 504, 604, 104, 204, 304, 404, 504, 604}, // N
    {103, 203, 303, 403, 503, 603, 103, 203, 303, 403, 503, 603}, // B
    {102, 202, 302, 402, 502, 602, 102, 202, 302, 402, 502, 602}, // R
    {101, 201, 301, 401, 501, 601, 101, 201, 301, 401, 501, 601}, // Q
    {100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600}, // K
    {105, 205, 305, 405, 505, 605, 105, 205, 305, 405, 505, 605}, // p
    {104, 204, 304, 404, 504, 604, 104, 204, 304, 404, 504, 604}, // n
    {103, 203, 303, 403, 503, 603, 103, 203, 303, 403, 503, 603}, // b
    {102, 202, 302, 402, 502, 602, 102, 202, 302, 402, 502, 602}, // r
    {101, 201, 301, 401, 501, 601, 101, 201, 301, 401, 501, 601}, // q
    {100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600}, // k
};

// ============================================ \\
//              SEE (Static Exchange Eval)      \\
// ============================================ \\

// Get smallest attacker to a square
int get_smallest_attacker(int square, int side, int *from_square)
{
    *from_square = -1;

    // Pawns
    U64 pawn_attackers = pawn_attacks[side ^ 1][square] & bitboards[side == white ? P : p];
    if (pawn_attackers)
    {
        *from_square = get_ls1b_index(pawn_attackers);
        return see_piece_values[0]; // Pawn
    }

    // Knights
    U64 knight_attackers = knight_attacks[square] & bitboards[side == white ? N : n];
    if (knight_attackers)
    {
        *from_square = get_ls1b_index(knight_attackers);
        return see_piece_values[1]; // Knight
    }

    // Bishops
    U64 bishop_attackers = get_bishop_attacks_magic(square, occupancies[both]) & bitboards[side == white ? B : b];
    if (bishop_attackers)
    {
        *from_square = get_ls1b_index(bishop_attackers);
        return see_piece_values[2]; // Bishop
    }

    // Rooks
    U64 rook_attackers = get_rook_attacks_magic(square, occupancies[both]) & bitboards[side == white ? R : r];
    if (rook_attackers)
    {
        *from_square = get_ls1b_index(rook_attackers);
        return see_piece_values[3]; // Rook
    }

    // Queens
    U64 queen_attackers = (get_bishop_attacks_magic(square, occupancies[both]) |
                           get_rook_attacks_magic(square, occupancies[both])) &
                          bitboards[side == white ? Q : q];
    if (queen_attackers)
    {
        *from_square = get_ls1b_index(queen_attackers);
        return see_piece_values[4]; // Queen
    }

    // Kings
    U64 king_attackers = king_attacks[square] & bitboards[side == white ? K : k];
    if (king_attackers)
    {
        *from_square = get_ls1b_index(king_attackers);
        return see_piece_values[5]; // King
    }

    return 0;
}

// Static Exchange Evaluation
int see(int move)
{
    int from = get_move_source(move);
    int to = get_move_target(move);
    int piece = get_move_piece(move);

    // Determine attacker value
    int attacker_value;
    switch (piece % 6)
    {
    case 0:
        attacker_value = see_piece_values[0];
        break; // Pawn
    case 1:
        attacker_value = see_piece_values[1];
        break; // Knight
    case 2:
        attacker_value = see_piece_values[2];
        break; // Bishop
    case 3:
        attacker_value = see_piece_values[3];
        break; // Rook
    case 4:
        attacker_value = see_piece_values[4];
        break; // Queen
    case 5:
        attacker_value = see_piece_values[5];
        break; // King
    default:
        attacker_value = 100;
    }

    // Determine victim value
    int victim_value = 0;
    if (get_move_capture(move))
    {
        for (int p = (side == white) ? p : P; p <= ((side == white) ? k : K); p++)
        {
            if (get_bit(bitboards[p], to))
            {
                switch (p % 6)
                {
                case 0:
                    victim_value = see_piece_values[0];
                    break;
                case 1:
                    victim_value = see_piece_values[1];
                    break;
                case 2:
                    victim_value = see_piece_values[2];
                    break;
                case 3:
                    victim_value = see_piece_values[3];
                    break;
                case 4:
                    victim_value = see_piece_values[4];
                    break;
                case 5:
                    victim_value = see_piece_values[5];
                    break;
                }
                break;
            }
        }
    }

    // Simple approximation: gain is victim - attacker if we lose our piece
    int gain[32];
    int d = 0;

    // We're copying board state conceptually
    U64 occ = occupancies[both];
    pop_bit(occ, from);

    gain[d] = victim_value;
    int current_side = side ^ 1;
    int current_attacker = attacker_value;

    while (1)
    {
        d++;
        int from_sq;
        int next_attacker = get_smallest_attacker(to, current_side, &from_sq);

        if (next_attacker == 0)
            break;

        // Stand pat
        gain[d] = current_attacker - gain[d - 1];

        // Remove attacker from occupancy
        if (from_sq >= 0)
            pop_bit(occ, from_sq);

        current_attacker = next_attacker;
        current_side ^= 1;

        if (d >= 30)
            break;
    }

    // Negamax the gain array
    while (--d > 0)
    {
        gain[d - 1] = -(gain[d] > -gain[d - 1] ? -gain[d - 1] : gain[d]);
    }

    return gain[0];
}

// SEE greater-than-or-equal threshold test
int see_ge(int move, int threshold)
{
    int from = get_move_source(move);
    int to = get_move_target(move);
    int piece = get_move_piece(move);

    // Quick wins for obvious cases
    if (!get_move_capture(move))
        return 0 >= threshold; // Non-captures: SEE = 0

    // Get victim value
    int victim_value = 0;
    for (int p = (side == white) ? p : P; p <= ((side == white) ? k : K); p++)
    {
        if (get_bit(bitboards[p], to))
        {
            switch (p % 6)
            {
            case 0:
                victim_value = see_piece_values[0];
                break;
            case 1:
                victim_value = see_piece_values[1];
                break;
            case 2:
                victim_value = see_piece_values[2];
                break;
            case 3:
                victim_value = see_piece_values[3];
                break;
            case 4:
                victim_value = see_piece_values[4];
                break;
            case 5:
                victim_value = see_piece_values[5];
                break;
            }
            break;
        }
    }

    // Get attacker value
    int attacker_value;
    switch (piece % 6)
    {
    case 0:
        attacker_value = see_piece_values[0];
        break;
    case 1:
        attacker_value = see_piece_values[1];
        break;
    case 2:
        attacker_value = see_piece_values[2];
        break;
    case 3:
        attacker_value = see_piece_values[3];
        break;
    case 4:
        attacker_value = see_piece_values[4];
        break;
    case 5:
        attacker_value = see_piece_values[5];
        break;
    default:
        attacker_value = 100;
    }

    // If capturing with lower value piece, almost certainly good
    if (victim_value >= attacker_value)
        return (victim_value - attacker_value) >= threshold;

    // Otherwise need full SEE
    return see(move) >= threshold;
}

// ============================================ \\
//              MOVE SCORING                    \\
// ============================================ \\

int score_move(int move, int pv_move, int ply)
{
    // PV move from TT
    if (move == pv_move)
        return 2000000;

    // Captures - MVV-LVA + SEE + capture history
    if (get_move_capture(move))
    {
        int piece = get_move_piece(move);
        int target = get_move_target(move);

        // Find victim piece
        int victim = P;
        int start = (side == white) ? p : P;
        int end = (side == white) ? k : K;
        for (int p = start; p <= end; p++)
        {
            if (get_bit(bitboards[p], target))
            {
                victim = p;
                break;
            }
        }

        // MVV-LVA base score
        int mvv_lva = mvv_lva_scores[piece][victim];

        // Add capture history
        int cap_hist = capture_history[piece][target][victim % 6];

        // SEE bonus for good captures, penalty for bad
        int see_score = 0;
        if (see_ge(move, 0))
            see_score = 50000;
        else
            see_score = -50000;

        return 1000000 + mvv_lva + cap_hist / 10 + see_score;
    }

    // Killer moves
    if (killer_moves[0][ply] == move)
        return 900000;
    if (killer_moves[1][ply] == move)
        return 800000;

    // Counter-move bonus
    if (ply > 0 && last_move_made[ply - 1])
    {
        int lm = last_move_made[ply - 1];
        if (counter_moves[get_move_piece(lm)][get_move_target(lm)] == move)
            return 700000;
    }

    // History heuristic
    int hist = history_moves[get_move_piece(move)][get_move_target(move)];

    // Butterfly history
    int from = get_move_source(move);
    int to = get_move_target(move);
    int bfly = butterfly_history[side][from][to];

    return hist + bfly / 2;
}

// ============================================ \\
//              QUIESCENCE SEARCH               \\
// ============================================ \\

int quiescence(int alpha, int beta)
{
    // Time check
    if ((nodes & 2047) == 0)
        communicate();
    if (times_up)
        return 0;

    nodes++;

    int stand_pat = evaluate();

    // Standing pat cutoff
    if (stand_pat >= beta)
        return beta;

    // Delta pruning
    const int BIG_DELTA = 975;
    if (stand_pat + BIG_DELTA < alpha)
        return alpha;

    if (alpha < stand_pat)
        alpha = stand_pat;

    moves move_list[1];
    generate_moves(move_list);

    // Score and sort captures only
    int scores[256];
    for (int i = 0; i < move_list->count; i++)
    {
        if (get_move_capture(move_list->moves[i]))
            scores[i] = score_move(move_list->moves[i], 0, 0);
        else
            scores[i] = -1000000;
    }

    for (int count = 0; count < move_list->count; count++)
    {
        // Selection sort
        int best_idx = count;
        for (int next = count + 1; next < move_list->count; next++)
        {
            if (scores[next] > scores[best_idx])
                best_idx = next;
        }

        // Swap
        if (best_idx != count)
        {
            int temp_move = move_list->moves[count];
            move_list->moves[count] = move_list->moves[best_idx];
            move_list->moves[best_idx] = temp_move;

            int temp_score = scores[count];
            scores[count] = scores[best_idx];
            scores[best_idx] = temp_score;
        }

        // Skip non-captures
        if (!get_move_capture(move_list->moves[count]))
            continue;

        // SEE pruning - skip bad captures
        if (!see_ge(move_list->moves[count], 0))
            continue;

        copy_board();
        if (!make_move(move_list->moves[count], only_captures))
            continue;

        int score = -quiescence(-beta, -alpha);
        take_back();

        if (times_up)
            return 0;

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }
    return alpha;
}

// ============================================ \\
//              NEGAMAX SEARCH                  \\
// ============================================ \\

int negamax(int alpha, int beta, int depth, int ply)
{
    // Initialize PV length
    pv_length[ply] = ply;

    // Is this a PV node?
    int pv_node = (beta - alpha > 1);

    // Time check
    if ((nodes & 2047) == 0)
        communicate();
    if (times_up)
        return 0;

    // Repetition detection
    if (ply > 0 && is_repetition())
        return 0;

    // Check TT
    int tt_score = read_tt(alpha, beta, depth, ply);
    int pv_move = get_tt_move();

    if (ply && tt_score != -INF - 1)
        return tt_score;

    // Base case: quiescence
    if (depth <= 0)
        return quiescence(alpha, beta);

    nodes++;

    // Safety check
    if (ply >= MAX_PLY - 1)
        return evaluate();

    // Check detection
    int in_check = is_square_attacked(
        (side == white) ? get_ls1b_index(bitboards[K]) : get_ls1b_index(bitboards[k]),
        side ^ 1);

    // Check extension
    if (in_check)
        depth++;

    // Null move pruning
    int non_pawn_material = (side == white) ? (count_bits(bitboards[N]) + count_bits(bitboards[B]) + count_bits(bitboards[R]) + count_bits(bitboards[Q])) : (count_bits(bitboards[n]) + count_bits(bitboards[b]) + count_bits(bitboards[r]) + count_bits(bitboards[q]));

    if (depth >= 3 && !in_check && ply > 0 && non_pawn_material > 0)
    {
        copy_board();

        int old_rep_index = repetition_index;

        side ^= 1;
        hash_key ^= side_key;
        repetition_index++;
        repetition_table[repetition_index] = hash_key;

        if (en_passant != no_sq)
        {
            hash_key ^= enpassant_keys[en_passant];
            en_passant = no_sq;
        }

        int R = 3 + depth / 6;
        if (R > depth - 1)
            R = depth - 1;

        int score = -negamax(-beta, -beta + 1, depth - 1 - R, ply + 1);

        repetition_index = old_rep_index;
        take_back();

        if (times_up)
            return 0;
        if (score >= beta)
            return beta;
    }

    // Razoring
    if (depth <= 3 && !in_check && ply > 0)
    {
        int razor_margin = 300 + 60 * depth;
        int eval = evaluate();
        if (eval + razor_margin < alpha)
        {
            int razor_score = quiescence(alpha - razor_margin, beta - razor_margin);
            if (razor_score + razor_margin <= alpha)
                return alpha;
        }
    }

    // Reverse futility pruning
    if (depth <= 6 && !in_check && ply > 0)
    {
        int eval = evaluate();
        int futility_margin = 80 * depth;
        if (eval - futility_margin >= beta)
            return eval - futility_margin;
    }

    moves move_list[1];
    generate_moves(move_list);

    // Score moves
    int scores[256];
    for (int i = 0; i < move_list->count; i++)
    {
        scores[i] = score_move(move_list->moves[i], pv_move, ply);
    }

    int moves_searched = 0;
    int best_so_far = -INF;
    int best_move_found = 0;
    int old_alpha = alpha;

    for (int count = 0; count < move_list->count; count++)
    {
        // Selection sort
        int best_idx = count;
        for (int next = count + 1; next < move_list->count; next++)
        {
            if (scores[next] > scores[best_idx])
                best_idx = next;
        }

        // Swap
        if (best_idx != count)
        {
            int temp = move_list->moves[count];
            move_list->moves[count] = move_list->moves[best_idx];
            move_list->moves[best_idx] = temp;

            int temp_score = scores[count];
            scores[count] = scores[best_idx];
            scores[best_idx] = temp_score;
        }

        copy_board();

        int old_rep_index = repetition_index;

        if (!make_move(move_list->moves[count], all_moves))
            continue;

        repetition_index++;
        repetition_table[repetition_index] = hash_key;
        last_move_made[ply] = move_list->moves[count];

        moves_searched++;
        int score;

        int is_capture = get_move_capture(move_list->moves[count]);
        int is_promotion = get_move_promoted(move_list->moves[count]);
        int is_quiet = !is_capture && !is_promotion;
        int gives_check = is_square_attacked(
            (side == white) ? get_ls1b_index(bitboards[k]) : get_ls1b_index(bitboards[K]),
            side);

        // Late move pruning
        if (depth <= 7 && !pv_node && !in_check && !gives_check && is_quiet &&
            moves_searched > lmp_margins[depth < 8 ? depth : 7])
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // Futility pruning at move level
        if (depth <= 6 && !pv_node && !in_check && !gives_check && is_quiet && moves_searched > 1)
        {
            int static_eval = evaluate();
            if (static_eval + futility_margins[depth] <= alpha)
            {
                repetition_index = old_rep_index;
                take_back();
                continue;
            }
        }

        // SEE pruning for bad captures
        if (depth <= 8 && !pv_node && is_capture && !see_ge(move_list->moves[count], -30 * depth * depth))
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // Extensions
        int extension = 0;
        if (gives_check)
            extension = 1;

        // Passed pawn extension
        if (get_move_piece(move_list->moves[count]) == P || get_move_piece(move_list->moves[count]) == p)
        {
            int target = get_move_target(move_list->moves[count]);
            int rank = target / 8;
            if ((side == black && rank == 1) || (side == white && rank == 6))
                extension = 1;
        }

        // PVS + LMR
        if (moves_searched == 1)
        {
            score = -negamax(-beta, -alpha, depth - 1 + extension, ply + 1);
        }
        else
        {
            int reduction = 0;

            if (moves_searched >= 3 && depth >= 3 && !in_check && is_quiet)
            {
                reduction = lmr_table[depth < MAX_PLY ? depth : MAX_PLY - 1][moves_searched < 64 ? moves_searched : 63];

                if (pv_node)
                    reduction--;

                if (move_list->moves[count] == killer_moves[0][ply] ||
                    move_list->moves[count] == killer_moves[1][ply])
                    reduction--;

                if (ply > 0 && last_move_made[ply - 1])
                {
                    int lm = last_move_made[ply - 1];
                    if (counter_moves[get_move_piece(lm)][get_move_target(lm)] == move_list->moves[count])
                        reduction--;
                }

                int hist = history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])];
                if (hist > 1000)
                    reduction--;
                else if (hist < -1000)
                    reduction++;

                if (!pv_node && depth > 8)
                    reduction++;

                if (reduction > depth - 2)
                    reduction = depth - 2;
                if (reduction < 0)
                    reduction = 0;
            }

            score = -negamax(-alpha - 1, -alpha, depth - 1 - reduction + extension, ply + 1);

            if (score > alpha && (reduction > 0 || score < beta))
            {
                score = -negamax(-beta, -alpha, depth - 1 + extension, ply + 1);
            }
        }

        repetition_index = old_rep_index;
        take_back();

        if (times_up)
            return 0;

        if (score > best_so_far)
        {
            best_so_far = score;
            best_move_found = move_list->moves[count];

            pv_table[ply][ply] = move_list->moves[count];
            for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++)
            {
                pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
            }
            pv_length[ply] = pv_length[ply + 1];
        }

        if (score >= beta)
        {
            int move = move_list->moves[count];
            int piece = get_move_piece(move);
            int target = get_move_target(move);
            int from = get_move_source(move);

            int bonus = depth * depth;
            if (bonus > 400)
                bonus = 400;

            if (is_capture)
            {
                int victim = P;
                int start = (side == white) ? p : P;
                int end = (side == white) ? k : K;
                for (int p = start; p <= end; p++)
                {
                    if (get_bit(bitboards[p], target))
                    {
                        victim = p;
                        break;
                    }
                }
                capture_history[piece][target][victim % 6] += bonus * 4;
                if (capture_history[piece][target][victim % 6] > history_max)
                    capture_history[piece][target][victim % 6] = history_max;
            }
            else
            {
                if (move != killer_moves[0][ply])
                {
                    killer_moves[1][ply] = killer_moves[0][ply];
                    killer_moves[0][ply] = move;
                }

                history_moves[piece][target] += bonus;
                if (history_moves[piece][target] > history_max)
                    history_moves[piece][target] = history_max;

                butterfly_history[side][from][target] += bonus;
                if (butterfly_history[side][from][target] > history_max)
                    butterfly_history[side][from][target] = history_max;

                if (ply > 0 && last_move_made[ply - 1])
                {
                    int lm = last_move_made[ply - 1];
                    counter_moves[get_move_piece(lm)][get_move_target(lm)] = move;
                }

                for (int i = 0; i < count; i++)
                {
                    int bad_move = move_list->moves[i];
                    if (!get_move_capture(bad_move) && bad_move != move)
                    {
                        history_moves[get_move_piece(bad_move)][get_move_target(bad_move)] -= bonus / 2;
                        if (history_moves[get_move_piece(bad_move)][get_move_target(bad_move)] < -history_max)
                            history_moves[get_move_piece(bad_move)][get_move_target(bad_move)] = -history_max;
                    }
                }
            }

            write_tt(depth, beta, HASH_BETA, move, ply);
            return beta;
        }

        if (score > alpha)
        {
            alpha = score;
            if (ply == 0)
                best_move = move_list->moves[count];
        }
    }

    // No legal moves
    if (moves_searched == 0)
    {
        if (in_check)
            return -MATE + ply;
        else
            return contempt;
    }

    // Store in TT
    int flag = (alpha > old_alpha) ? HASH_EXACT : HASH_ALPHA;
    write_tt(depth, alpha, flag, best_move_found, ply);

    return alpha;
}

// ============================================ \\
//              PERFT (Performance Test)        \\
// ============================================ \\

void perft_driver(int depth)
{
    if (depth == 0)
    {
        nodes++;
        return;
    }

    moves move_list[1];
    generate_moves(move_list);

    for (int count = 0; count < move_list->count; count++)
    {
        copy_board();

        if (!make_move(move_list->moves[count], all_moves))
            continue;

        perft_driver(depth - 1);
        take_back();
    }
}

void perft_test(int depth)
{
    nodes = 0LL;
    printf("\n  Performance test\n\n");

    moves move_list[1];
    generate_moves(move_list);

    for (int count = 0; count < move_list->count; count++)
    {
        copy_board();

        if (!make_move(move_list->moves[count], all_moves))
            continue;

        long cumulative_nodes = nodes;
        perft_driver(depth - 1);
        take_back();

        long old_nodes = nodes - cumulative_nodes;
        printf("  move: %d  ", count + 1);
        print_move(move_list->moves[count]);
        printf("  nodes: %ld\n", old_nodes);
    }

    printf("\n  Depth: %d\n", depth);
    printf("  Nodes: %lld\n", nodes);
}
