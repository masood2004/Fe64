// ============================================ \\
//       FE64 CHESS ENGINE - MOVE GENERATION    \\
// ============================================ \\

#include "types.h"

// External function declarations
extern U64 get_bishop_attacks_magic(int square, U64 occupancy);
extern U64 get_rook_attacks_magic(int square, U64 occupancy);
extern U64 get_queen_attacks(int square, U64 block);
extern int is_square_attacked(int square, int side_attacking);
extern U64 generate_hash_key();

// ============================================ \\
//           MOVE LIST HELPERS                  \\
// ============================================ \\

void add_move(moves *move_list, int move)
{
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

void print_move(int move)
{
    printf("%c%d%c%d",
           (get_move_source(move) % 8) + 'a',
           8 - (get_move_source(move) / 8),
           (get_move_target(move) % 8) + 'a',
           8 - (get_move_target(move) / 8));

    int promoted = get_move_promoted(move);
    if (promoted)
    {
        char promo_char;
        switch (promoted)
        {
        case Q:
        case q:
            promo_char = 'q';
            break;
        case R:
        case r:
            promo_char = 'r';
            break;
        case B:
        case b:
            promo_char = 'b';
            break;
        case N:
        case n:
            promo_char = 'n';
            break;
        default:
            promo_char = 'q';
            break;
        }
        printf("%c", promo_char);
    }
}

// ============================================ \\
//           MOVE GENERATION                    \\
// ============================================ \\

void generate_moves(moves *move_list)
{
    move_list->count = 0;

    int source_square, target_square;
    U64 bitboard, attacks;

    // ===== PAWN MOVES =====
    if (side == white)
    {
        bitboard = bitboards[P];
        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);
            target_square = source_square - 8;

            if (!(target_square < a8) && !get_bit(occupancies[both], target_square))
            {
                // Promotion
                if (source_square >= a7 && source_square <= h7)
                {
                    add_move(move_list, encode_move(source_square, target_square, P, Q, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, R, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, B, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, N, 0, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, P, 0, 0, 0, 0, 0));
                    // Double push
                    if ((source_square >= a2 && source_square <= h2) && !get_bit(occupancies[both], target_square - 8))
                    {
                        add_move(move_list, encode_move(source_square, target_square - 8, P, 0, 0, 1, 0, 0));
                    }
                }
            }

            // Captures
            attacks = pawn_attacks[white][source_square] & occupancies[black];
            while (attacks)
            {
                target_square = get_ls1b_index(attacks);
                if (source_square >= a7 && source_square <= h7)
                {
                    add_move(move_list, encode_move(source_square, target_square, P, Q, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, R, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, B, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, P, N, 1, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, P, 0, 1, 0, 0, 0));
                }
                pop_bit(attacks, target_square);
            }

            // En Passant
            if (en_passant != no_sq)
            {
                U64 enpassant_attacks = pawn_attacks[white][source_square] & (1ULL << en_passant);
                if (enpassant_attacks)
                {
                    int target_enpassant = get_ls1b_index(enpassant_attacks);
                    add_move(move_list, encode_move(source_square, target_enpassant, P, 0, 1, 0, 1, 0));
                }
            }
            pop_bit(bitboard, source_square);
        }
    }
    else
    {
        // BLACK PAWNS
        bitboard = bitboards[p];
        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);
            target_square = source_square + 8;

            if (!(target_square > h1) && !get_bit(occupancies[both], target_square))
            {
                if (source_square >= a2 && source_square <= h2)
                {
                    add_move(move_list, encode_move(source_square, target_square, p, q, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, r, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, b, 0, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, n, 0, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, p, 0, 0, 0, 0, 0));
                    if ((source_square >= a7 && source_square <= h7) && !get_bit(occupancies[both], target_square + 8))
                    {
                        add_move(move_list, encode_move(source_square, target_square + 8, p, 0, 0, 1, 0, 0));
                    }
                }
            }

            attacks = pawn_attacks[black][source_square] & occupancies[white];
            while (attacks)
            {
                target_square = get_ls1b_index(attacks);
                if (source_square >= a2 && source_square <= h2)
                {
                    add_move(move_list, encode_move(source_square, target_square, p, q, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, r, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, b, 1, 0, 0, 0));
                    add_move(move_list, encode_move(source_square, target_square, p, n, 1, 0, 0, 0));
                }
                else
                {
                    add_move(move_list, encode_move(source_square, target_square, p, 0, 1, 0, 0, 0));
                }
                pop_bit(attacks, target_square);
            }

            if (en_passant != no_sq)
            {
                U64 enpassant_attacks = pawn_attacks[black][source_square] & (1ULL << en_passant);
                if (enpassant_attacks)
                {
                    int target_enpassant = get_ls1b_index(enpassant_attacks);
                    add_move(move_list, encode_move(source_square, target_enpassant, p, 0, 1, 0, 1, 0));
                }
            }
            pop_bit(bitboard, source_square);
        }
    }

    // ===== CASTLING MOVES =====
    if (side == white)
    {
        if (castle & wk)
        {
            if (!get_bit(occupancies[both], f1) && !get_bit(occupancies[both], g1))
            {
                if (!is_square_attacked(e1, black) && !is_square_attacked(f1, black) && !is_square_attacked(g1, black))
                {
                    add_move(move_list, encode_move(e1, g1, K, 0, 0, 0, 0, 1));
                }
            }
        }
        if (castle & wq)
        {
            if (!get_bit(occupancies[both], d1) && !get_bit(occupancies[both], c1) && !get_bit(occupancies[both], b1))
            {
                if (!is_square_attacked(e1, black) && !is_square_attacked(d1, black) && !is_square_attacked(c1, black))
                {
                    add_move(move_list, encode_move(e1, c1, K, 0, 0, 0, 0, 1));
                }
            }
        }
    }
    else
    {
        if (castle & bk)
        {
            if (!get_bit(occupancies[both], f8) && !get_bit(occupancies[both], g8))
            {
                if (!is_square_attacked(e8, white) && !is_square_attacked(f8, white) && !is_square_attacked(g8, white))
                {
                    add_move(move_list, encode_move(e8, g8, k, 0, 0, 0, 0, 1));
                }
            }
        }
        if (castle & bq)
        {
            if (!get_bit(occupancies[both], d8) && !get_bit(occupancies[both], c8) && !get_bit(occupancies[both], b8))
            {
                if (!is_square_attacked(e8, white) && !is_square_attacked(d8, white) && !is_square_attacked(c8, white))
                {
                    add_move(move_list, encode_move(e8, c8, k, 0, 0, 0, 0, 1));
                }
            }
        }
    }

    // ===== PIECE MOVES (Knight, Bishop, Rook, Queen, King) =====
    int p_start = (side == white) ? N : n;
    int p_end = (side == white) ? K : k;

    for (int piece = p_start; piece <= p_end; piece++)
    {
        bitboard = bitboards[piece];

        while (bitboard)
        {
            source_square = get_ls1b_index(bitboard);

            if (side == white)
            {
                if (piece == N)
                    attacks = knight_attacks[source_square];
                else if (piece == B)
                    attacks = get_bishop_attacks_magic(source_square, occupancies[both]);
                else if (piece == R)
                    attacks = get_rook_attacks_magic(source_square, occupancies[both]);
                else if (piece == Q)
                    attacks = get_queen_attacks(source_square, occupancies[both]);
                else if (piece == K)
                    attacks = king_attacks[source_square];
                attacks &= ~occupancies[white];
            }
            else
            {
                if (piece == n)
                    attacks = knight_attacks[source_square];
                else if (piece == b)
                    attacks = get_bishop_attacks_magic(source_square, occupancies[both]);
                else if (piece == r)
                    attacks = get_rook_attacks_magic(source_square, occupancies[both]);
                else if (piece == q)
                    attacks = get_queen_attacks(source_square, occupancies[both]);
                else if (piece == k)
                    attacks = king_attacks[source_square];
                attacks &= ~occupancies[black];
            }

            while (attacks)
            {
                target_square = get_ls1b_index(attacks);
                int capture = get_bit(occupancies[(!side) ? black : white], target_square) ? 1 : 0;
                add_move(move_list, encode_move(source_square, target_square, piece, 0, capture, 0, 0, 0));
                pop_bit(attacks, target_square);
            }
            pop_bit(bitboard, source_square);
        }
    }
}

// ============================================ \\
//           MAKE MOVE                          \\
// ============================================ \\

int make_move(int move, int move_flag)
{
    if (move_flag == only_captures)
    {
        if (!get_move_capture(move))
            return 0;
    }

    copy_board();

    int source_square = get_move_source(move);
    int target_square = get_move_target(move);
    int piece = get_move_piece(move);
    int promoted_piece = get_move_promoted(move);
    int capture = get_move_capture(move);
    int double_push = get_move_double(move);
    int enpass = get_move_enpassant(move);
    int castling = get_move_castling(move);

    // Move piece
    pop_bit(bitboards[piece], source_square);
    set_bit(bitboards[piece], target_square);
    hash_key ^= piece_keys[piece][source_square];
    hash_key ^= piece_keys[piece][target_square];

    // Handle captures
    if (capture)
    {
        int start_piece = (side == white) ? p : P;
        int end_piece = (side == white) ? k : K;
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            if (get_bit(bitboards[bb_piece], target_square))
            {
                pop_bit(bitboards[bb_piece], target_square);
                hash_key ^= piece_keys[bb_piece][target_square];
                break;
            }
        }
    }

    // Handle promotions
    if (promoted_piece)
    {
        pop_bit(bitboards[(side == white) ? P : p], target_square);
        set_bit(bitboards[promoted_piece], target_square);
        hash_key ^= piece_keys[(side == white) ? P : p][target_square];
        hash_key ^= piece_keys[promoted_piece][target_square];
    }

    // Handle en passant capture
    if (enpass)
    {
        if (side == white)
        {
            pop_bit(bitboards[p], target_square + 8);
            hash_key ^= piece_keys[p][target_square + 8];
        }
        else
        {
            pop_bit(bitboards[P], target_square - 8);
            hash_key ^= piece_keys[P][target_square - 8];
        }
    }

    // Update en passant state
    if (en_passant != no_sq)
        hash_key ^= enpassant_keys[en_passant];
    en_passant = no_sq;

    if (double_push)
    {
        if (side == white)
        {
            en_passant = target_square + 8;
            hash_key ^= enpassant_keys[target_square + 8];
        }
        else
        {
            en_passant = target_square - 8;
            hash_key ^= enpassant_keys[target_square - 8];
        }
    }

    // Handle castling rook moves
    if (castling)
    {
        switch (target_square)
        {
        case g1:
            pop_bit(bitboards[R], h1);
            set_bit(bitboards[R], f1);
            hash_key ^= piece_keys[R][h1];
            hash_key ^= piece_keys[R][f1];
            break;
        case c1:
            pop_bit(bitboards[R], a1);
            set_bit(bitboards[R], d1);
            hash_key ^= piece_keys[R][a1];
            hash_key ^= piece_keys[R][d1];
            break;
        case g8:
            pop_bit(bitboards[r], h8);
            set_bit(bitboards[r], f8);
            hash_key ^= piece_keys[r][h8];
            hash_key ^= piece_keys[r][f8];
            break;
        case c8:
            pop_bit(bitboards[r], a8);
            set_bit(bitboards[r], d8);
            hash_key ^= piece_keys[r][a8];
            hash_key ^= piece_keys[r][d8];
            break;
        }
    }

    // Update castling rights
    hash_key ^= castle_keys[castle];
    castle &= castling_rights[source_square];
    castle &= castling_rights[target_square];
    hash_key ^= castle_keys[castle];

    // Update occupancies
    for (int i = 0; i < 3; i++)
        occupancies[i] = 0ULL;
    for (int bb_piece = P; bb_piece <= K; bb_piece++)
        occupancies[white] |= bitboards[bb_piece];
    for (int bb_piece = p; bb_piece <= k; bb_piece++)
        occupancies[black] |= bitboards[bb_piece];
    occupancies[both] = occupancies[white] | occupancies[black];

    // Change side
    side ^= 1;
    hash_key ^= side_key;

    // Legality check
    if (is_square_attacked((side == white) ? get_ls1b_index(bitboards[k]) : get_ls1b_index(bitboards[K]), side))
    {
        take_back();
        return 0;
    }
    return 1;
}

// ============================================ \\
//           FEN PARSING                        \\
// ============================================ \\

void parse_fen(char *fen)
{
    for (int i = 0; i < 12; i++)
        bitboards[i] = 0ULL;
    for (int i = 0; i < 3; i++)
        occupancies[i] = 0ULL;
    side = 0;
    en_passant = no_sq;
    castle = 0;

    int rank = 0, file = 0;
    while (rank < 8 && *fen && *fen != ' ')
    {
        int square = rank * 8 + file;
        if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
        {
            int piece = -1;
            switch (*fen)
            {
            case 'P':
                piece = P;
                break;
            case 'N':
                piece = N;
                break;
            case 'B':
                piece = B;
                break;
            case 'R':
                piece = R;
                break;
            case 'Q':
                piece = Q;
                break;
            case 'K':
                piece = K;
                break;
            case 'p':
                piece = p;
                break;
            case 'n':
                piece = n;
                break;
            case 'b':
                piece = b;
                break;
            case 'r':
                piece = r;
                break;
            case 'q':
                piece = q;
                break;
            case 'k':
                piece = k;
                break;
            }
            if (piece != -1)
                set_bit(bitboards[piece], square);
            file++;
            fen++;
        }
        else if (*fen >= '0' && *fen <= '9')
        {
            file += *fen - '0';
            fen++;
        }
        else if (*fen == '/')
        {
            file = 0;
            rank++;
            fen++;
        }
        else
        {
            fen++;
        }
    }

    fen++;
    side = (*fen == 'w') ? white : black;
    fen += 2;

    while (*fen != ' ')
    {
        switch (*fen)
        {
        case 'K':
            castle |= wk;
            break;
        case 'Q':
            castle |= wq;
            break;
        case 'k':
            castle |= bk;
            break;
        case 'q':
            castle |= bq;
            break;
        }
        fen++;
    }

    fen++;
    if (*fen != '-')
    {
        int f = fen[0] - 'a';
        int r = 8 - (fen[1] - '0');
        en_passant = r * 8 + f;
    }

    for (int piece = P; piece <= K; piece++)
        occupancies[white] |= bitboards[piece];
    for (int piece = p; piece <= k; piece++)
        occupancies[black] |= bitboards[piece];
    occupancies[both] = occupancies[white] | occupancies[black];

    hash_key = generate_hash_key();
}

int parse_move(char *move_string)
{
    if (!move_string || strlen(move_string) < 4)
        return 0;

    moves move_list[1];
    generate_moves(move_list);

    int source_file = move_string[0] - 'a';
    int source_rank = 8 - (move_string[1] - '0');
    int target_file = move_string[2] - 'a';
    int target_rank = 8 - (move_string[3] - '0');

    if (source_file < 0 || source_file > 7 || source_rank < 0 || source_rank > 7 ||
        target_file < 0 || target_file > 7 || target_rank < 0 || target_rank > 7)
        return 0;

    int source = source_rank * 8 + source_file;
    int target = target_rank * 8 + target_file;

    for (int count = 0; count < move_list->count; count++)
    {
        int move = move_list->moves[count];
        if (get_move_source(move) == source && get_move_target(move) == target)
        {
            int promoted = get_move_promoted(move);
            if (promoted)
            {
                if (strlen(move_string) < 5)
                    continue;
                if ((promoted == Q || promoted == q) && move_string[4] == 'q')
                    return move;
                if ((promoted == R || promoted == r) && move_string[4] == 'r')
                    return move;
                if ((promoted == B || promoted == b) && move_string[4] == 'b')
                    return move;
                if ((promoted == N || promoted == n) && move_string[4] == 'n')
                    return move;
                continue;
            }
            return move;
        }
    }
    return 0;
}

void parse_position(char *command)
{
    command += 9;
    char *current_char = command;
    repetition_index = 0;

    if (strncmp(command, "startpos", 8) == 0)
    {
        parse_fen(start_position);
    }
    else
    {
        current_char = strstr(command, "fen");
        if (current_char == NULL)
        {
            parse_fen(start_position);
        }
        else
        {
            current_char += 4;
            parse_fen(current_char);
        }
    }

    repetition_table[repetition_index] = hash_key;

    current_char = strstr(command, "moves");
    if (current_char != NULL)
    {
        current_char += 6;
        while (*current_char)
        {
            int move = parse_move(current_char);
            if (move == 0)
                break;
            make_move(move, all_moves);
            repetition_index++;
            repetition_table[repetition_index] = hash_key;
            while (*current_char && *current_char != ' ')
                current_char++;
            current_char++;
        }
    }
}
