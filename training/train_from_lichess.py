#!/usr/bin/env python3
"""
Fe64 Chess Engine - Self-Learning from Lichess Games
====================================================
This script downloads your bot's games from Lichess and uses them to train
a NNUE (Efficiently Updatable Neural Network) for improved evaluation.

Usage:
    python train_from_lichess.py --username Fe64 --output nnue.bin

Requirements:
    pip install requests numpy chess
"""

import argparse
import struct
import os
import sys
import json
import random
from collections import defaultdict

try:
    import requests
    import numpy as np
    import chess
    import chess.pgn
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Install with: pip install requests numpy chess")
    sys.exit(1)

# NNUE Architecture (must match bbc.c)
NNUE_INPUT_SIZE = 768      # 12 pieces * 64 squares
NNUE_HIDDEN1_SIZE = 256    # First hidden layer
NNUE_HIDDEN2_SIZE = 32     # Second hidden layer
NNUE_OUTPUT_SIZE = 1       # Single evaluation output
NNUE_SCALE = 400           # Scale factor


class NNUETrainer:
    """Train NNUE weights from position-evaluation pairs"""

    def __init__(self):
        # Initialize weights with Xavier initialization
        self.input_weights = np.random.randn(NNUE_INPUT_SIZE, NNUE_HIDDEN1_SIZE).astype(
            np.float32) * np.sqrt(2.0 / NNUE_INPUT_SIZE)
        self.hidden1_bias = np.zeros(NNUE_HIDDEN1_SIZE, dtype=np.float32)
        self.hidden1_weights = np.random.randn(NNUE_HIDDEN1_SIZE, NNUE_HIDDEN2_SIZE).astype(
            np.float32) * np.sqrt(2.0 / NNUE_HIDDEN1_SIZE)
        self.hidden2_bias = np.zeros(NNUE_HIDDEN2_SIZE, dtype=np.float32)
        self.hidden2_weights = np.random.randn(NNUE_HIDDEN2_SIZE).astype(
            np.float32) * np.sqrt(2.0 / NNUE_HIDDEN2_SIZE)
        self.output_bias = np.float32(0.0)

    def load_weights(self, filename):
        """Load existing NNUE weights"""
        with open(filename, 'rb') as f:
            self.input_weights = np.frombuffer(f.read(
                NNUE_INPUT_SIZE * NNUE_HIDDEN1_SIZE * 4), dtype=np.float32).reshape(NNUE_INPUT_SIZE, NNUE_HIDDEN1_SIZE)
            self.hidden1_bias = np.frombuffer(
                f.read(NNUE_HIDDEN1_SIZE * 4), dtype=np.float32)
            self.hidden1_weights = np.frombuffer(f.read(
                NNUE_HIDDEN1_SIZE * NNUE_HIDDEN2_SIZE * 4), dtype=np.float32).reshape(NNUE_HIDDEN1_SIZE, NNUE_HIDDEN2_SIZE)
            self.hidden2_bias = np.frombuffer(
                f.read(NNUE_HIDDEN2_SIZE * 4), dtype=np.float32)
            self.hidden2_weights = np.frombuffer(
                f.read(NNUE_HIDDEN2_SIZE * 4), dtype=np.float32)
            self.output_bias = np.frombuffer(f.read(4), dtype=np.float32)[0]
        print(f"Loaded existing NNUE weights from {filename}")

    def save_weights(self, filename):
        """Save NNUE weights in format compatible with bbc.c"""
        with open(filename, 'wb') as f:
            # Ensure all arrays are float32 before saving
            f.write(self.input_weights.astype(np.float32).tobytes())
            f.write(self.hidden1_bias.astype(np.float32).tobytes())
            f.write(self.hidden1_weights.astype(np.float32).tobytes())
            f.write(self.hidden2_bias.astype(np.float32).tobytes())
            f.write(self.hidden2_weights.astype(np.float32).tobytes())
            f.write(np.array([self.output_bias], dtype=np.float32).tobytes())

        # Verify file size
        expected_size = (NNUE_INPUT_SIZE * NNUE_HIDDEN1_SIZE + NNUE_HIDDEN1_SIZE +
                         NNUE_HIDDEN1_SIZE * NNUE_HIDDEN2_SIZE + NNUE_HIDDEN2_SIZE +
                         NNUE_HIDDEN2_SIZE + 1) * 4
        actual_size = os.path.getsize(filename)
        if actual_size != expected_size:
            print(
                f"WARNING: File size mismatch! Expected {expected_size}, got {actual_size}")
        else:
            print(f"Saved NNUE weights to {filename} ({actual_size} bytes)")

    @staticmethod
    def crelu(x):
        """Clipped ReLU activation"""
        return np.clip(x, 0, 1)

    @staticmethod
    def crelu_derivative(x):
        """Derivative of CReLU"""
        return ((x > 0) & (x < 1)).astype(np.float32)

    def board_to_input(self, board):
        """Convert chess.Board to NNUE input vector"""
        input_vec = np.zeros(NNUE_INPUT_SIZE, dtype=np.float32)

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
                idx = piece_idx * 64 + square
                input_vec[idx] = 1.0

        return input_vec

    def forward(self, input_vec):
        """Forward pass through the network"""
        # Layer 1
        hidden1 = self.crelu(
            np.dot(input_vec, self.input_weights) + self.hidden1_bias)
        # Layer 2
        hidden2 = self.crelu(
            np.dot(hidden1, self.hidden1_weights) + self.hidden2_bias)
        # Output
        output = np.dot(hidden2, self.hidden2_weights) + self.output_bias
        return output, hidden1, hidden2

    def train_batch(self, positions, targets, learning_rate=0.001):
        """Train on a batch of positions"""
        total_loss = 0.0

        # Gradient accumulators
        grad_input_weights = np.zeros_like(self.input_weights)
        grad_hidden1_bias = np.zeros_like(self.hidden1_bias)
        grad_hidden1_weights = np.zeros_like(self.hidden1_weights)
        grad_hidden2_bias = np.zeros_like(self.hidden2_bias)
        grad_hidden2_weights = np.zeros_like(self.hidden2_weights)
        grad_output_bias = 0.0

        batch_size = len(positions)

        for input_vec, target in zip(positions, targets):
            # Forward pass
            z1 = np.dot(input_vec, self.input_weights) + self.hidden1_bias
            hidden1 = self.crelu(z1)
            z2 = np.dot(hidden1, self.hidden1_weights) + self.hidden2_bias
            hidden2 = self.crelu(z2)
            output = np.dot(hidden2, self.hidden2_weights) + self.output_bias

            # Loss (MSE scaled)
            scaled_target = target / NNUE_SCALE
            error = output - scaled_target
            total_loss += error ** 2

            # Backward pass
            d_output = 2 * error / batch_size

            grad_output_bias += d_output
            grad_hidden2_weights += d_output * hidden2

            d_hidden2 = d_output * self.hidden2_weights * \
                self.crelu_derivative(z2)
            grad_hidden2_bias += d_hidden2
            grad_hidden1_weights += np.outer(hidden1, d_hidden2)

            d_hidden1 = np.dot(
                d_hidden2, self.hidden1_weights.T) * self.crelu_derivative(z1)
            grad_hidden1_bias += d_hidden1
            grad_input_weights += np.outer(input_vec, d_hidden1)

        # Update weights (ensure float32 after operations)
        self.input_weights = (
            self.input_weights - learning_rate * grad_input_weights).astype(np.float32)
        self.hidden1_bias = (self.hidden1_bias - learning_rate *
                             grad_hidden1_bias).astype(np.float32)
        self.hidden1_weights = (
            self.hidden1_weights - learning_rate * grad_hidden1_weights).astype(np.float32)
        self.hidden2_bias = (self.hidden2_bias - learning_rate *
                             grad_hidden2_bias).astype(np.float32)
        self.hidden2_weights = (
            self.hidden2_weights - learning_rate * grad_hidden2_weights).astype(np.float32)
        self.output_bias = np.float32(
            self.output_bias - learning_rate * grad_output_bias)

        return total_loss / batch_size


def download_lichess_games(username, max_games=500):
    """Download games from Lichess API"""
    url = f"https://lichess.org/api/games/user/{username}"
    params = {
        'max': max_games,
        'rated': 'true',
        'perfType': 'bullet,blitz,rapid,classical',
        'opening': 'true'
    }
    headers = {'Accept': 'application/x-ndjson'}

    print(f"Downloading games for {username}...")

    games = []
    try:
        response = requests.get(
            url, params=params, headers=headers, stream=True)
        response.raise_for_status()

        for line in response.iter_lines():
            if line:
                game = json.loads(line)
                games.append(game)
                if len(games) % 50 == 0:
                    print(f"Downloaded {len(games)} games...")
    except requests.RequestException as e:
        print(f"Error downloading games: {e}")

    print(f"Downloaded {len(games)} games total")
    return games


def extract_positions_from_game(game_data, bot_username):
    """Extract training positions from a game"""
    positions = []

    if 'moves' not in game_data:
        return positions

    moves_str = game_data['moves']
    if not moves_str:
        return positions

    # Determine game result from bot's perspective
    white_player = game_data.get('players', {}).get(
        'white', {}).get('user', {}).get('name', '').lower()
    result = game_data.get('winner', 'draw')

    is_bot_white = white_player == bot_username.lower()

    if result == 'white':
        bot_score = 1.0 if is_bot_white else -1.0
    elif result == 'black':
        bot_score = -1.0 if is_bot_white else 1.0
    else:
        bot_score = 0.0

    # Parse moves
    board = chess.Board()
    moves = moves_str.split()

    for i, move_str in enumerate(moves):
        try:
            move = board.parse_san(move_str)

            # Only use positions from middlegame/endgame (after move 10)
            if i >= 20:  # After move 10 for both sides
                # Create position data
                pos = {
                    'board': board.copy(),
                    'ply': i,
                    'result': bot_score,
                    'is_bot_turn': (board.turn == chess.WHITE) == is_bot_white
                }
                positions.append(pos)

            board.push(move)
        except (chess.IllegalMoveError, chess.InvalidMoveError):
            break

    return positions


def calculate_target_eval(position, material_eval):
    """Calculate target evaluation for training

    Combines:
    - Game result (win/loss/draw)
    - Material evaluation
    - Position in game (earlier positions get more weight from result)
    """
    result = position['result']
    ply = position['ply']

    # Blend result with material - result matters more in endgame
    # More weight to result as game progresses
    result_weight = min(0.8, ply / 100.0)

    # Target = blend of material eval and result
    result_eval = result * 300  # Scale result to centipawns
    target = (1 - result_weight) * material_eval + result_weight * result_eval

    return target


def simple_material_eval(board):
    """Simple material evaluation in centipawns"""
    values = {
        chess.PAWN: 100,
        chess.KNIGHT: 320,
        chess.BISHOP: 330,
        chess.ROOK: 500,
        chess.QUEEN: 900,
        chess.KING: 0
    }

    score = 0
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece:
            value = values[piece.piece_type]
            if piece.color == chess.WHITE:
                score += value
            else:
                score -= value

    return score


def main():
    parser = argparse.ArgumentParser(
        description='Train Fe64 NNUE from Lichess games')
    parser.add_argument('--username', '-u', default='Fe64',
                        help='Lichess bot username')
    parser.add_argument('--output', '-o', default='nnue.bin',
                        help='Output NNUE file')
    parser.add_argument(
        '--input', '-i', help='Existing NNUE file to continue training')
    parser.add_argument('--max-games', '-m', type=int,
                        default=500, help='Maximum games to download')
    parser.add_argument('--epochs', '-e', type=int,
                        default=10, help='Training epochs')
    parser.add_argument('--batch-size', '-b', type=int,
                        default=64, help='Batch size')
    parser.add_argument('--learning-rate', '-lr', type=float,
                        default=0.001, help='Learning rate')
    args = parser.parse_args()

    # Initialize trainer
    trainer = NNUETrainer()

    # Load existing weights if provided
    if args.input and os.path.exists(args.input):
        trainer.load_weights(args.input)

    # Download games
    games = download_lichess_games(args.username, args.max_games)

    if not games:
        print("No games found!")
        return

    # Extract training positions
    print("\nExtracting training positions...")
    all_positions = []

    for game in games:
        positions = extract_positions_from_game(game, args.username)
        all_positions.extend(positions)

    print(f"Extracted {len(all_positions)} training positions")

    if len(all_positions) < 100:
        print("Not enough positions for training!")
        return

    # Prepare training data
    print("\nPreparing training data...")
    training_data = []

    for pos in all_positions:
        board = pos['board']
        input_vec = trainer.board_to_input(board)
        material_eval = simple_material_eval(board)
        target = calculate_target_eval(pos, material_eval)

        # Flip perspective if black to move
        if board.turn == chess.BLACK:
            target = -target

        training_data.append((input_vec, target))

    # Train
    print(f"\nTraining for {args.epochs} epochs...")

    for epoch in range(args.epochs):
        random.shuffle(training_data)

        epoch_loss = 0.0
        num_batches = 0

        for i in range(0, len(training_data), args.batch_size):
            batch = training_data[i:i + args.batch_size]
            if len(batch) < args.batch_size // 2:
                continue

            inputs = [x[0] for x in batch]
            targets = [x[1] for x in batch]

            loss = trainer.train_batch(inputs, targets, args.learning_rate)
            epoch_loss += loss
            num_batches += 1

        avg_loss = epoch_loss / max(num_batches, 1)
        print(f"Epoch {epoch + 1}/{args.epochs} - Loss: {avg_loss:.4f}")

    # Save trained weights
    trainer.save_weights(args.output)
    print(f"\nTraining complete! NNUE saved to {args.output}")
    print(f"\nTo use in your engine:")
    print(f"  1. Copy {args.output} to your engine directory")
    print(f"  2. Compile with: make (or gcc -DUSE_NNUE ...)")
    print(f"  3. In UCI: setoption name UseNNUE value true")


if __name__ == '__main__':
    main()
