#!/usr/bin/env python3
"""
Test script to simulate a full game with the chess engine.
This tests the engine's ability to handle multiple consecutive moves without getting stuck.
"""

import subprocess
import sys
import time


def main():
    engine_path = "/home/syedmasoodhussain/Desktop/chess_engine/bin/fe64"

    # Start the engine
    print("Starting engine...")
    proc = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    def send(cmd):
        print(f">>> {cmd}")
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()

    def read_until(pattern, timeout=30):
        start = time.time()
        lines = []
        while time.time() - start < timeout:
            line = proc.stdout.readline()
            if not line:
                time.sleep(0.01)
                continue
            line = line.strip()
            if line:
                print(f"<<< {line}")
                lines.append(line)
            if pattern in line:
                return lines
        raise TimeoutError(f"Timeout waiting for '{pattern}' after {timeout}s")

    try:
        # Initialize
        send("uci")
        read_until("uciok")

        send("isready")
        read_until("readyok")

        send("ucinewgame")
        send("isready")
        read_until("readyok")

        # Simulate a game - alternating moves
        # We'll play both sides, using the engine for white
        moves = []

        # Predefined black responses (simple moves to test engine)
        black_moves = [
            "e7e5",   # 1...e5
            "b8c6",   # 2...Nc6
            "g8f6",   # 3...Nf6
            "f8b4",   # 4...Bb4
            "e8g8",   # 5...O-O (if legal)
            "d7d6",   # 6...d6
            "c8g4",   # 7...Bg4
            "h7h6",   # 8...h6
            "b4c3",   # 9...Bxc3
            "a7a6",   # 10...a6
            "b7b5",   # 11...b5
            "c6a5",   # 12...Na5
            "g4h5",   # 13...Bh5
            "a5c4",   # 14...Nc4
            "f6d5",   # 15...Nd5
            "h5g6",   # 16...Bg6
            "d5f4",   # 17...Nf4
            "c4e5",   # 18...Ne5
            "e5d3",   # 19...Nd3
            "d3f4",   # 20...Nxf4
        ]

        move_num = 0
        for i in range(20):
            move_num = i + 1
            print(f"\n=== Move {move_num} (White) ===")

            # Build position command
            pos_cmd = "position startpos"
            if moves:
                pos_cmd += " moves " + " ".join(moves)
            send(pos_cmd)

            # Ask engine to search with reasonable time
            send(f"go wtime 60000 btime 60000 winc 1000 binc 1000")

            # Wait for bestmove
            try:
                lines = read_until("bestmove", timeout=15)
            except TimeoutError as e:
                print(f"\n!!! ENGINE GOT STUCK AT MOVE {move_num} !!!")
                print(f"Error: {e}")
                proc.terminate()
                return 1

            # Extract best move
            for line in lines:
                if line.startswith("bestmove"):
                    parts = line.split()
                    if len(parts) >= 2:
                        white_move = parts[1]
                        if white_move == "0000" or white_move == "(none)":
                            print(
                                f"Game over at move {move_num} - no legal moves")
                            proc.terminate()
                            return 0
                        moves.append(white_move)
                        print(f"White plays: {white_move}")
                    break

            # Make black's move (if we have one predefined)
            if i < len(black_moves):
                print(f"\n=== Move {move_num} (Black) ===")
                black_move = black_moves[i]
                moves.append(black_move)
                print(f"Black plays: {black_move}")
            else:
                print(
                    f"\nReached end of predefined black moves at move {move_num}")
                break

        print(
            f"\n=== TEST PASSED: Completed {move_num} moves successfully! ===")
        send("quit")
        proc.wait(timeout=2)
        return 0

    except Exception as e:
        print(f"\n!!! TEST FAILED: {e} !!!")
        proc.terminate()
        return 1
    finally:
        if proc.poll() is None:
            proc.terminate()


if __name__ == "__main__":
    sys.exit(main())
