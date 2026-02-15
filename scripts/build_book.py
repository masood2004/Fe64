#!/usr/bin/env python3
"""
Build a Polyglot opening book from PGN games.
Processes all PGN files in the games directory and creates a comprehensive book.
"""

import chess
import chess.pgn
import chess.polyglot
import struct
import os
import sys
from collections import defaultdict


def build_polyglot_book(pgn_files, output_file, max_moves=20, min_weight=1):
    """Build a Polyglot opening book from PGN games."""

    # Dictionary: position_hash -> {move -> total_weight}
    book_data = defaultdict(lambda: defaultdict(int))

    total_games = 0

    for pgn_file in pgn_files:
        if not os.path.exists(pgn_file):
            print(f"Skipping {pgn_file} (not found)")
            continue

        print(f"Processing {pgn_file}...")
        games_in_file = 0

        with open(pgn_file) as f:
            while True:
                try:
                    game = chess.pgn.read_game(f)
                    if game is None:
                        break

                    # Determine game result weight
                    result = game.headers.get("Result", "*")
                    white_elo = int(game.headers.get("WhiteElo", "0") or "0")
                    black_elo = int(game.headers.get("BlackElo", "0") or "0")

                    # Weight based on result
                    if result == "1-0":
                        white_weight = 2
                        black_weight = 0
                    elif result == "0-1":
                        white_weight = 0
                        black_weight = 2
                    elif result == "1/2-1/2":
                        white_weight = 1
                        black_weight = 1
                    else:
                        white_weight = 1
                        black_weight = 1

                    # ELO bonus
                    avg_elo = (white_elo + black_elo) / 2
                    if avg_elo > 2600:
                        elo_mult = 3
                    elif avg_elo > 2400:
                        elo_mult = 2
                    elif avg_elo > 2200:
                        elo_mult = 1
                    else:
                        elo_mult = 1

                    board = game.board()
                    move_count = 0

                    for move in game.mainline_moves():
                        if move_count >= max_moves:
                            break

                        key = chess.polyglot.zobrist_hash(board)

                        # Convert move to polyglot format
                        from_sq = move.from_square
                        to_sq = move.to_square
                        promotion = 0
                        if move.promotion:
                            promo_map = {
                                chess.KNIGHT: 1, chess.BISHOP: 2, chess.ROOK: 3, chess.QUEEN: 4}
                            promotion = promo_map.get(move.promotion, 0)

                        # Polyglot move encoding
                        poly_move = (from_sq % 8) << 6 | (
                            7 - from_sq // 8) << 9 | (to_sq % 8) | (7 - to_sq // 8) << 3 | (promotion << 12)

                        # Castling special handling
                        if board.is_castling(move):
                            if to_sq == chess.G1:
                                poly_move = (4 << 6) | (0 << 9) | 7 | (0 << 3)
                            elif to_sq == chess.C1:
                                poly_move = (4 << 6) | (0 << 9) | 0 | (0 << 3)
                            elif to_sq == chess.G8:
                                poly_move = (4 << 6) | (7 << 9) | 7 | (7 << 3)
                            elif to_sq == chess.C8:
                                poly_move = (4 << 6) | (7 << 9) | 0 | (7 << 3)

                        weight = (white_weight if board.turn ==
                                  chess.WHITE else black_weight) * elo_mult
                        book_data[key][poly_move] += weight

                        board.push(move)
                        move_count += 1

                    games_in_file += 1
                    total_games += 1

                except Exception as e:
                    continue

        print(f"  Processed {games_in_file} games from {pgn_file}")

    # Write Polyglot book
    entries = []
    for key, moves in book_data.items():
        for move, weight in moves.items():
            if weight >= min_weight:
                entries.append((key, move, weight, 0))

    # Sort by key for binary search
    entries.sort(key=lambda x: x[0])

    with open(output_file, 'wb') as f:
        for key, move, weight, learn in entries:
            f.write(struct.pack('>QHHi', key, move, min(weight, 65535), learn))

    print(f"\nBuilt book with {len(entries)} entries from {total_games} games")
    print(f"Saved to {output_file}")


if __name__ == "__main__":
    games_dir = os.path.join(os.path.dirname(
        __file__), '..', 'training', 'games')
    output_file = os.path.join(os.path.dirname(
        __file__), '..', 'bin', 'custom_book.bin')

    pgn_files = []
    if os.path.exists(games_dir):
        for f in os.listdir(games_dir):
            if f.endswith('.pgn'):
                pgn_files.append(os.path.join(games_dir, f))

    if pgn_files:
        build_polyglot_book(pgn_files, output_file, max_moves=25)
    else:
        print("No PGN files found in training/games/")
