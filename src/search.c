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
extern int get_tt_score_raw(int ply, int *tt_depth_out, int *tt_flags_out);
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
        int start_piece = (side == white) ? p : P;
        int end_piece = (side == white) ? k : K;
        for (int pp = start_piece; pp <= end_piece; pp++)
        {
            if (get_bit(bitboards[pp], to))
            {
                switch (pp % 6)
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
    {
        int start_piece = (side == white) ? p : P;
        int end_piece = (side == white) ? k : K;
        for (int pp = start_piece; pp <= end_piece; pp++)
        {
            if (get_bit(bitboards[pp], to))
            {
                switch (pp % 6)
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
    // Time check - check more frequently (every 1024 nodes)
    if ((nodes & 1023) == 0)
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

    // Time check - check more frequently (every 1024 nodes)
    if ((nodes & 1023) == 0)
        communicate();
    if (times_up)
        return 0;

    // Repetition detection
    if (ply > 0 && is_repetition())
        return 0;

    // Mate distance pruning - if we already found a mate closer to root
    if (ply > 0)
    {
        int r_alpha = alpha > -MATE + ply ? alpha : -MATE + ply;
        int r_beta = beta < MATE - ply - 1 ? beta : MATE - ply - 1;
        if (r_alpha >= r_beta)
            return r_alpha;
    }

    // Check TT (skip if we have an excluded move for singular extension search)
    int tt_score = -INF - 1;
    int pv_move = 0;
    int tt_depth = 0;
    int tt_flags = 0;
    int raw_tt_score = -INF - 1;

    if (!excluded_move[ply])
    {
        tt_score = read_tt(alpha, beta, depth, ply);
        pv_move = get_tt_move();
        raw_tt_score = get_tt_score_raw(ply, &tt_depth, &tt_flags);

        if (tt_score != -INF - 1 && ply)
            return tt_score;
    }
    else
    {
        pv_move = get_tt_move(); // Still get TT move for ordering
    }

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

    // Check extension - but limit to prevent explosion
    // Only extend if we're not too deep already
    if (in_check && depth < MAX_PLY / 2)
        depth++;

    // Static evaluation for pruning decisions
    int static_eval = evaluate();
    static_eval_stack[ply] = static_eval;

    // Improving flag - position is getting better compared to 2 plies ago
    int improving = (ply >= 2 && static_eval > static_eval_stack[ply - 2]);

    // Null move pruning (with verification)
    int non_pawn_material = (side == white) ? (count_bits(bitboards[N]) + count_bits(bitboards[B]) + count_bits(bitboards[R]) + count_bits(bitboards[Q])) : (count_bits(bitboards[n]) + count_bits(bitboards[b]) + count_bits(bitboards[r]) + count_bits(bitboards[q]));

    if (depth >= 3 && !in_check && ply > 0 && non_pawn_material > 1)
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

        // Adaptive null move reduction
        int R = 3 + depth / 3 + (depth > 6 ? 1 : 0);
        if (R > depth - 1)
            R = depth - 1;

        int score = -negamax(-beta, -beta + 1, depth - 1 - R, ply + 1);

        repetition_index = old_rep_index;
        take_back();

        if (times_up)
            return 0;
        if (score >= beta)
        {
            // Don't return unproven mate scores
            if (score >= MATE - 100)
                score = beta;
            return score;
        }
    }

    // Razoring
    if (depth <= 3 && !in_check && ply > 0)
    {
        int razor_margin = 300 + 60 * depth;
        if (static_eval + razor_margin < alpha)
        {
            int razor_score = quiescence(alpha - razor_margin, beta - razor_margin);
            if (razor_score + razor_margin <= alpha)
                return alpha;
        }
    }

    // Probcut - if a shallow search at a higher beta finds a cutoff,
    // the full depth search likely will too
    if (depth >= 5 && !pv_node && !in_check && ply > 0 &&
        abs(beta) < MATE - 100)
    {
        int probcut_beta = beta + probcut_margin;
        int probcut_depth = depth - 4;
        if (probcut_depth < 1)
            probcut_depth = 1;

        moves probcut_moves[1];
        generate_moves(probcut_moves);

        // Score and sort for probcut (only try captures and good moves)
        int pc_scores[256];
        for (int i = 0; i < probcut_moves->count; i++)
            pc_scores[i] = score_move(probcut_moves->moves[i], pv_move, ply);

        for (int i = 0; i < probcut_moves->count; i++)
        {
            // Selection sort
            int best_idx = i;
            for (int j = i + 1; j < probcut_moves->count; j++)
                if (pc_scores[j] > pc_scores[best_idx])
                    best_idx = j;
            if (best_idx != i)
            {
                int tmp = probcut_moves->moves[i];
                probcut_moves->moves[i] = probcut_moves->moves[best_idx];
                probcut_moves->moves[best_idx] = tmp;
                int ts = pc_scores[i];
                pc_scores[i] = pc_scores[best_idx];
                pc_scores[best_idx] = ts;
            }

            // Only try captures for probcut
            if (!get_move_capture(probcut_moves->moves[i]))
                continue;

            // Skip bad captures
            if (!see_ge(probcut_moves->moves[i], 0))
                continue;

            copy_board();
            int old_rep = repetition_index;
            if (!make_move(probcut_moves->moves[i], all_moves))
                continue;
            repetition_index++;
            repetition_table[repetition_index] = hash_key;

            // Do a shallow verification search
            int pc_score = -negamax(-probcut_beta, -probcut_beta + 1, probcut_depth, ply + 1);

            repetition_index = old_rep;
            take_back();

            if (times_up)
                return 0;

            if (pc_score >= probcut_beta)
                return pc_score;
        }
    }

    // Reverse futility pruning
    if (depth <= 6 && !in_check && ply > 0 && !pv_node)
    {
        int futility_margin = (improving ? 70 : 80) * depth;
        if (static_eval - futility_margin >= beta)
            return static_eval - futility_margin;
    }

    moves move_list[1];
    generate_moves(move_list);

    // Internal Iterative Deepening (IID)
    if (depth >= 5 && !pv_move && !in_check)
    {
        int iid_score = negamax(alpha, beta, depth - 3, ply);
        if (!times_up)
            pv_move = get_tt_move();
    }

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

        // Skip excluded move (for singular extension search)
        if (move_list->moves[count] == excluded_move[ply])
            continue;

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
            moves_searched > lmp_margins[depth < 8 ? depth : 7] + (improving ? 3 : 0))
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // Futility pruning at move level
        if (depth <= 6 && !pv_node && !in_check && !gives_check && is_quiet && moves_searched > 1)
        {
            if (static_eval + futility_margins[depth] <= alpha)
            {
                repetition_index = old_rep_index;
                take_back();
                continue;
            }
        }

        // History pruning - prune quiet moves with very negative history
        if (depth <= 4 && !pv_node && !in_check && is_quiet && moves_searched > 1)
        {
            int hist = history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])];
            int hist_threshold = -1024 * depth;
            if (hist < hist_threshold)
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

        // SEE pruning for quiet moves at low depths
        if (depth <= 6 && !pv_node && is_quiet && moves_searched > 3 &&
            !see_ge(move_list->moves[count], -20 * depth))
        {
            repetition_index = old_rep_index;
            take_back();
            continue;
        }

        // Extensions
        int extension = 0;
        if (gives_check)
            extension = 1;

        // Singular extensions - if TT move appears much better than alternatives
        if (depth >= 8 && move_list->moves[count] == pv_move && pv_move &&
            !excluded_move[ply] && !in_check &&
            raw_tt_score != -INF - 1 && tt_depth >= depth - 3 &&
            (tt_flags == HASH_EXACT || tt_flags == HASH_BETA))
        {
            int se_beta = raw_tt_score - 2 * depth;
            int se_depth = (depth - 1) / 2;

            // Search all moves except the TT move at reduced depth
            excluded_move[ply] = pv_move;
            int se_score = negamax(se_beta - 1, se_beta, se_depth, ply);
            excluded_move[ply] = 0;

            if (!times_up && se_score < se_beta)
            {
                // TT move is singular - extend it
                extension = 1;
            }
            else if (!times_up && se_score >= beta)
            {
                // Multi-cut: even without TT move, we exceed beta
                return se_score;
            }
        }

        // Passed pawn extension
        if (extension == 0 && (get_move_piece(move_list->moves[count]) == P || get_move_piece(move_list->moves[count]) == p))
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

                // Reduce less for PV nodes
                if (pv_node)
                    reduction--;

                // Reduce less for killer moves
                if (move_list->moves[count] == killer_moves[0][ply] ||
                    move_list->moves[count] == killer_moves[1][ply])
                    reduction--;

                // Reduce less for counter moves
                if (ply > 0 && last_move_made[ply - 1])
                {
                    int lm = last_move_made[ply - 1];
                    if (counter_moves[get_move_piece(lm)][get_move_target(lm)] == move_list->moves[count])
                        reduction--;
                }

                // History-based LMR adjustments
                int hist = history_moves[get_move_piece(move_list->moves[count])][get_move_target(move_list->moves[count])];
                reduction -= hist / 5000; // Good history reduces less, bad history increases

                // Increase reduction for non-PV nodes at higher depths
                if (!pv_node && depth > 8)
                    reduction++;

                // More aggressive at very high move counts
                if (moves_searched > 12)
                    reduction++;

                // Reduce less when improving
                if (improving)
                    reduction--;

                // Increase reduction for positions with many pieces (complex middlegame)
                if (!pv_node && non_pawn_material > 4)
                    reduction++;

                // Reduce by 1 for captures in LMR (they have their own ordering)
                if (is_capture && !pv_node)
                    reduction++;

                if (reduction > depth - 2)
                    reduction = depth - 2;
                if (reduction < 0)
                    reduction = 0;
            }
            // LMR for captures too (less aggressively)
            else if (moves_searched >= 5 && depth >= 5 && !in_check && is_capture && !pv_node)
            {
                int see_val = see(move_list->moves[count]);
                if (see_val < 0)
                    reduction = 1 + (depth > 8 ? 1 : 0);
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
        // At ply 0, always ensure we have a move to play (first legal move found)
        else if (ply == 0 && best_move == 0)
        {
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
