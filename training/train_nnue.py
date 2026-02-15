#!/usr/bin/env python3
"""
Fe64 NNUE Training System
Train neural network weights from chess game databases.
Supports training from PGN files with Stockfish evaluation or self-play.
"""

import chess
import chess.pgn
import chess.engine
import struct
import os
import sys
import random
import math
import numpy as np
from collections import defaultdict

# NNUE Architecture
INPUT_SIZE = 768   # 12 pieces * 64 squares
HIDDEN1_SIZE = 256  # First hidden layer
HIDDEN2_SIZE = 32  # Second hidden layer
OUTPUT_SIZE = 1    # Single evaluation output
SCALE = 400        # Output scale factor


class NNUENetwork:
    """Simple NNUE network for chess position evaluation."""

    def __init__(self):
        """Initialize with random weights."""
        self.input_weights = (np.random.randn(INPUT_SIZE, HIDDEN1_SIZE) *
                              np.sqrt(2.0 / INPUT_SIZE)).astype(np.float32)
        self.hidden1_bias = np.zeros(HIDDEN1_SIZE, dtype=np.float32)
        self.hidden1_weights = (np.random.randn(HIDDEN1_SIZE, HIDDEN2_SIZE) *
                                np.sqrt(2.0 / HIDDEN1_SIZE)).astype(np.float32)
        self.hidden2_bias = np.zeros(HIDDEN2_SIZE, dtype=np.float32)
        self.hidden2_weights = (np.random.randn(HIDDEN2_SIZE) *
                                np.sqrt(2.0 / HIDDEN2_SIZE)).astype(np.float32)
        self.output_bias = np.float32(0.0)

    def load(self, filename):
        """Load weights from binary file."""
        with open(filename, 'rb') as f:
            self.input_weights = np.frombuffer(f.read(
                INPUT_SIZE * HIDDEN1_SIZE * 4), dtype=np.float32).reshape(INPUT_SIZE, HIDDEN1_SIZE)
            self.hidden1_bias = np.frombuffer(
                f.read(HIDDEN1_SIZE * 4), dtype=np.float32)
            self.hidden1_weights = np.frombuffer(f.read(
                HIDDEN1_SIZE * HIDDEN2_SIZE * 4), dtype=np.float32).reshape(HIDDEN1_SIZE, HIDDEN2_SIZE)
            self.hidden2_bias = np.frombuffer(
                f.read(HIDDEN2_SIZE * 4), dtype=np.float32)
            self.hidden2_weights = np.frombuffer(
                f.read(HIDDEN2_SIZE * 4), dtype=np.float32)
            self.output_bias = np.frombuffer(f.read(4), dtype=np.float32)[0]
        print(f"Loaded NNUE from {filename}")

    def save(self, filename):
        """Save weights to binary file (always float32)."""
        with open(filename, 'wb') as f:
            f.write(self.input_weights.astype(np.float32).tobytes())
            f.write(self.hidden1_bias.astype(np.float32).tobytes())
            f.write(self.hidden1_weights.astype(np.float32).tobytes())
            f.write(self.hidden2_bias.astype(np.float32).tobytes())
            f.write(self.hidden2_weights.astype(np.float32).tobytes())
            f.write(np.array([self.output_bias], dtype=np.float32).tobytes())
        print(f"Saved NNUE to {filename}")

    def board_to_features(self, board):
        """Convert a chess board to NNUE input features."""
        features = np.zeros(INPUT_SIZE, dtype=np.float32)

        piece_map = {
            chess.PAWN: 0, chess.KNIGHT: 1, chess.BISHOP: 2,
            chess.ROOK: 3, chess.QUEEN: 4, chess.KING: 5
        }

        for square in chess.SQUARES:
            piece = board.piece_at(square)
            if piece:
                piece_idx = piece_map[piece.piece_type]
                if piece.color == chess.BLACK:
                    piece_idx += 6
                # Convert to our engine's square mapping (a8=0, h1=63)
                rank = chess.square_rank(square)
                file = chess.square_file(square)
                our_sq = (7 - rank) * 8 + file
                feature_idx = piece_idx * 64 + our_sq
                features[feature_idx] = 1.0

        return features

    def forward(self, features):
        """Forward pass through the network."""
        # Layer 1
        hidden1 = self.hidden1_bias.copy()
        active_indices = np.where(features > 0)[0]
        for idx in active_indices:
            hidden1 += self.input_weights[idx]
        hidden1 = np.clip(hidden1, 0, 1)  # CReLU

        # Layer 2
        hidden2 = self.hidden2_bias + hidden1 @ self.hidden1_weights
        hidden2 = np.clip(hidden2, 0, 1)  # CReLU

        # Output
        output = self.output_bias + hidden2 @ self.hidden2_weights
        return output

    def forward_with_cache(self, features):
        """Forward pass with intermediate values cached for backprop."""
        hidden1_pre = self.hidden1_bias.copy()
        active_indices = np.where(features > 0)[0]
        for idx in active_indices:
            hidden1_pre += self.input_weights[idx]
        hidden1 = np.clip(hidden1_pre, 0, 1)

        hidden2_pre = self.hidden2_bias + hidden1 @ self.hidden1_weights
        hidden2 = np.clip(hidden2_pre, 0, 1)

        output = self.output_bias + hidden2 @ self.hidden2_weights

        return output, {
            'features': features,
            'active_indices': active_indices,
            'hidden1_pre': hidden1_pre,
            'hidden1': hidden1,
            'hidden2_pre': hidden2_pre,
            'hidden2': hidden2,
        }

    def backward(self, cache, target, lr=0.001):
        """Backpropagation to update weights (vectorized)."""
        output = cache['hidden2'] @ self.hidden2_weights + self.output_bias

        # MSE loss gradient
        error = output - target

        # Output layer gradients
        d_hidden2_weights = error * cache['hidden2']
        d_hidden2 = error * self.hidden2_weights

        # Hidden2 CReLU gradient
        mask2 = ((cache['hidden2_pre'] > 0) &
                 (cache['hidden2_pre'] < 1)).astype(np.float32)
        d_hidden2_pre = d_hidden2 * mask2

        # Hidden1 -> Hidden2 gradients
        d_hidden1 = d_hidden2_pre @ self.hidden1_weights.T

        # Hidden1 CReLU gradient
        mask1 = ((cache['hidden1_pre'] > 0) &
                 (cache['hidden1_pre'] < 1)).astype(np.float32)
        d_hidden1_pre = d_hidden1 * mask1

        # Update weights with gradient descent
        self.output_bias -= lr * error
        self.hidden2_weights -= lr * d_hidden2_weights
        self.hidden2_bias -= lr * d_hidden2_pre
        self.hidden1_weights -= lr * np.outer(cache['hidden1'], d_hidden2_pre)
        self.hidden1_bias -= lr * d_hidden1_pre

        # Only update active input weights (vectorized)
        if len(cache['active_indices']) > 0:
            self.input_weights[cache['active_indices']
                               ] -= lr * d_hidden1_pre[np.newaxis, :]

        return error ** 2  # Return loss


def sigmoid(x, k=0.004):
    """Sigmoid function to convert centipawn to winning probability."""
    return 1.0 / (1.0 + math.exp(-k * x))


def extract_positions_from_pgn(pgn_file, max_positions=100000, stockfish_path=None):
    """Extract training positions from PGN files."""
    positions = []

    # If we have Stockfish, use it for evaluation
    engine = None
    if stockfish_path and os.path.exists(stockfish_path):
        try:
            engine = chess.engine.SimpleEngine.popen_uci(stockfish_path)
            engine.configure({"Threads": 2, "Hash": 256})
            print(f"Using Stockfish at {stockfish_path} for evaluation")
        except:
            engine = None

    print(f"Extracting positions from {pgn_file}...")

    with open(pgn_file) as f:
        game_count = 0
        while len(positions) < max_positions:
            try:
                game = chess.pgn.read_game(f)
                if game is None:
                    break

                result = game.headers.get("Result", "*")
                if result == "*":
                    continue

                # Result-based evaluation
                if result == "1-0":
                    result_score = 1.0
                elif result == "0-1":
                    result_score = 0.0
                else:
                    result_score = 0.5

                board = game.board()
                moves = list(game.mainline_moves())

                for i, move in enumerate(moves):
                    board.push(move)

                    # Skip first few moves and positions in check
                    if i < 6 or board.is_check():
                        continue

                    # Sample ~40% of positions for better coverage
                    if random.random() > 0.4:
                        continue

                    if engine:
                        # Get Stockfish evaluation at depth 12
                        try:
                            info = engine.analyse(
                                board, chess.engine.Limit(depth=12))
                            score = info["score"].white()
                            if score.is_mate():
                                cp = 10000 if score.mate() > 0 else -10000
                            else:
                                cp = score.score()
                            target = sigmoid(cp)
                        except:
                            target = result_score
                    else:
                        # Use result + simple material evaluation
                        material = simple_material_eval(board)
                        material_score = sigmoid(material)
                        # Blend game result with material
                        progress = min(i / len(moves), 1.0)
                        target = (1 - progress) * material_score + \
                            progress * result_score

                    positions.append((board.copy(), target))

                game_count += 1
                if game_count % 100 == 0:
                    print(
                        f"  Processed {game_count} games, {len(positions)} positions")

            except Exception as e:
                continue

    if engine:
        engine.quit()

    print(f"Extracted {len(positions)} positions from {game_count} games")
    return positions


def simple_material_eval(board):
    """Simple material evaluation in centipawns."""
    piece_values = {
        chess.PAWN: 100, chess.KNIGHT: 337, chess.BISHOP: 365,
        chess.ROOK: 477, chess.QUEEN: 1025, chess.KING: 0
    }

    score = 0
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece:
            value = piece_values[piece.piece_type]
            if piece.color == chess.WHITE:
                score += value
            else:
                score -= value

    return score


def train_nnue(pgn_files, output_file, epochs=50, lr=0.001, batch_size=64, stockfish_path=None):
    """Train NNUE from PGN games."""

    print("=" * 60)
    print("Fe64 NNUE Training System")
    print("=" * 60)

    # Initialize network
    net = NNUENetwork()

    # Check if we have an existing network to continue training
    if os.path.exists(output_file):
        try:
            net.load(output_file)
            print("Continuing training from existing weights")
        except:
            print("Starting with random weights")

    # Extract training positions
    all_positions = []
    for pgn_file in pgn_files:
        if os.path.exists(pgn_file):
            positions = extract_positions_from_pgn(
                pgn_file, max_positions=50000, stockfish_path=stockfish_path)
            all_positions.extend(positions)

    if not all_positions:
        print("No training positions found!")
        return

    print(f"\nTotal training positions: {len(all_positions)}")
    print(
        f"Network architecture: {INPUT_SIZE} -> {HIDDEN1_SIZE} -> {HIDDEN2_SIZE} -> {OUTPUT_SIZE}")
    print(f"Learning rate: {lr}")
    print(f"Epochs: {epochs}")
    print()

    # Training loop
    for epoch in range(epochs):
        random.shuffle(all_positions)
        total_loss = 0.0
        num_samples = 0

        for i in range(0, len(all_positions), batch_size):
            batch = all_positions[i:i+batch_size]
            batch_loss = 0.0

            for board, target in batch:
                features = net.board_to_features(board)
                output, cache = net.forward_with_cache(features)

                # Scale output to [0, 1]
                pred = sigmoid(output * SCALE)

                # Compute loss and backprop
                loss = net.backward(cache, target / SCALE, lr=lr)
                batch_loss += (pred - target) ** 2
                num_samples += 1

            total_loss += batch_loss

        avg_loss = total_loss / max(num_samples, 1)

        if (epoch + 1) % 5 == 0 or epoch == 0:
            print(f"Epoch {epoch+1:3d}/{epochs}: Loss = {avg_loss:.6f}")

        # Decay learning rate
        if (epoch + 1) % 20 == 0:
            lr *= 0.5
            print(f"  Learning rate decayed to {lr:.6f}")

        # Save checkpoint
        if (epoch + 1) % 10 == 0:
            net.save(output_file)

    # Final save
    net.save(output_file)
    print("\nTraining complete!")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Fe64 NNUE Training")
    parser.add_argument("--pgn", nargs="+", help="PGN files to train from")
    parser.add_argument("--output", default="nnue.bin",
                        help="Output NNUE file")
    parser.add_argument("--epochs", type=int, default=30,
                        help="Number of training epochs")
    parser.add_argument("--lr", type=float, default=0.001,
                        help="Learning rate")
    parser.add_argument("--stockfish", help="Path to Stockfish for evaluation")

    args = parser.parse_args()

    if args.pgn:
        pgn_files = args.pgn
    else:
        # Default: use training games
        games_dir = os.path.join(os.path.dirname(
            os.path.abspath(__file__)), '..', 'training', 'games')
        pgn_files = []
        if os.path.exists(games_dir):
            for f in sorted(os.listdir(games_dir)):
                if f.endswith('.pgn'):
                    pgn_files.append(os.path.join(games_dir, f))

    if not pgn_files:
        print("No PGN files found! Provide --pgn arguments or add PGN files to training/games/")
        sys.exit(1)

    output = os.path.join(os.path.dirname(
        os.path.abspath(__file__)), '..', 'bin', args.output)

    train_nnue(pgn_files, output, epochs=args.epochs,
               lr=args.lr, stockfish_path=args.stockfish)
