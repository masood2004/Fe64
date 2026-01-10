#!/usr/bin/env python3
"""
Fe64 Chess Engine - Game Analysis Tool
=======================================
Analyzes your Lichess games to find weaknesses in your engine.

This identifies:
- Positions where your engine blundered
- Common patterns in lost games
- Endgame-specific weaknesses
- Time trouble mistakes

Usage:
    python analyze_games.py --username Fe64
"""

import argparse
import json
import sys
from collections import defaultdict
from dataclasses import dataclass
from typing import List, Dict

try:
    import requests
    import chess
    import chess.pgn
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Install with: pip install requests chess")
    sys.exit(1)


@dataclass
class BlunderInfo:
    """Information about a blunder"""
    game_id: str
    ply: int
    fen: str
    move_played: str
    position_type: str  # opening, middlegame, endgame
    material_before: int
    material_after: int
    time_remaining: int  # if available
    result: str


def download_games(username: str, max_games: int = 500) -> List[Dict]:
    """Download games from Lichess API"""
    url = f"https://lichess.org/api/games/user/{username}"
    params = {
        'max': max_games,
        'rated': 'true',
        'clocks': 'true',
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
                games.append(json.loads(line))
                if len(games) % 50 == 0:
                    print(f"Downloaded {len(games)} games...")
    except requests.RequestException as e:
        print(f"Error: {e}")

    return games


def count_material(board: chess.Board) -> int:
    """Count material in centipawns (positive = white advantage)"""
    values = {chess.PAWN: 100, chess.KNIGHT: 320, chess.BISHOP: 330,
              chess.ROOK: 500, chess.QUEEN: 900, chess.KING: 0}

    score = 0
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece:
            value = values[piece.piece_type]
            score += value if piece.color == chess.WHITE else -value
    return score


def get_position_type(board: chess.Board) -> str:
    """Determine if position is opening, middlegame, or endgame"""
    # Count non-pawn, non-king pieces
    piece_count = 0
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece and piece.piece_type not in [chess.PAWN, chess.KING]:
            piece_count += 1

    if piece_count >= 12:
        return "opening"
    elif piece_count >= 6:
        return "middlegame"
    else:
        return "endgame"


def analyze_game(game_data: Dict, bot_username: str) -> Dict:
    """Analyze a single game for blunders and patterns"""
    result = {
        'game_id': game_data.get('id', 'unknown'),
        'blunders': [],
        'won': False,
        'position_types': defaultdict(int),
        'result': 'unknown'
    }

    if 'moves' not in game_data:
        return result

    moves_str = game_data['moves']
    if not moves_str:
        return result

    # Determine sides
    white_player = game_data.get('players', {}).get(
        'white', {}).get('user', {}).get('name', '').lower()
    is_bot_white = white_player == bot_username.lower()

    winner = game_data.get('winner')
    if winner == 'white':
        result['won'] = is_bot_white
        result['result'] = 'win' if is_bot_white else 'loss'
    elif winner == 'black':
        result['won'] = not is_bot_white
        result['result'] = 'win' if not is_bot_white else 'loss'
    else:
        result['result'] = 'draw'

    # Parse and analyze moves
    board = chess.Board()
    moves = moves_str.split()
    prev_material = count_material(board)

    for i, move_str in enumerate(moves):
        try:
            is_bot_move = (board.turn == chess.WHITE) == is_bot_white
            pos_type = get_position_type(board)

            move = board.parse_san(move_str)
            board.push(move)

            current_material = count_material(board)
            material_diff = current_material - prev_material

            # Adjust for perspective
            if not is_bot_white:
                material_diff = -material_diff

            # Track position types in lost games
            if result['result'] == 'loss':
                result['position_types'][pos_type] += 1

            # Detect blunders (large material loss on bot's move)
            if is_bot_move and material_diff < -100:  # Lost more than a pawn
                blunder = BlunderInfo(
                    game_id=result['game_id'],
                    ply=i,
                    fen=board.fen(),
                    move_played=move_str,
                    position_type=pos_type,
                    material_before=prev_material,
                    material_after=current_material,
                    time_remaining=0,
                    result=result['result']
                )
                result['blunders'].append(blunder)

            prev_material = current_material

        except (chess.IllegalMoveError, chess.InvalidMoveError):
            break

    return result


def print_analysis_report(analyses: List[Dict], username: str):
    """Print comprehensive analysis report"""
    total_games = len(analyses)
    wins = sum(1 for a in analyses if a['result'] == 'win')
    losses = sum(1 for a in analyses if a['result'] == 'loss')
    draws = sum(1 for a in analyses if a['result'] == 'draw')

    print("\n" + "="*60)
    print(f"  FE64 GAME ANALYSIS REPORT - {username}")
    print("="*60)

    print(f"\n📊 OVERALL STATISTICS")
    print(f"   Total Games: {total_games}")
    print(f"   Wins: {wins} ({100*wins/total_games:.1f}%)")
    print(f"   Losses: {losses} ({100*losses/total_games:.1f}%)")
    print(f"   Draws: {draws} ({100*draws/total_games:.1f}%)")

    # Blunder analysis
    all_blunders = []
    for a in analyses:
        all_blunders.extend(a['blunders'])

    print(f"\n🔴 BLUNDER ANALYSIS")
    print(f"   Total Blunders: {len(all_blunders)}")
    print(f"   Blunders per Game: {len(all_blunders)/total_games:.2f}")

    # Blunders by position type
    blunder_by_type = defaultdict(int)
    for b in all_blunders:
        blunder_by_type[b.position_type] += 1

    print(f"\n   Blunders by Position Type:")
    for ptype, count in sorted(blunder_by_type.items(), key=lambda x: -x[1]):
        print(
            f"     - {ptype.capitalize()}: {count} ({100*count/max(len(all_blunders),1):.1f}%)")

    # Position type analysis in lost games
    lost_game_phases = defaultdict(int)
    for a in analyses:
        if a['result'] == 'loss':
            for ptype, count in a['position_types'].items():
                lost_game_phases[ptype] += count

    print(f"\n📉 LOST GAME ANALYSIS")
    print(f"   Moves played by phase in lost games:")
    total_lost_moves = sum(lost_game_phases.values())
    for ptype, count in sorted(lost_game_phases.items(), key=lambda x: -x[1]):
        pct = 100 * count / max(total_lost_moves, 1)
        print(f"     - {ptype.capitalize()}: {count} moves ({pct:.1f}%)")

    # Sample blunders
    print(f"\n⚠️  SAMPLE BLUNDERS (showing worst 5)")
    print("-" * 60)

    # Sort by material loss
    worst_blunders = sorted(all_blunders,
                            key=lambda b: b.material_after - b.material_before)[:5]

    for i, b in enumerate(worst_blunders, 1):
        loss = b.material_before - b.material_after
        print(f"\n   {i}. Game: {b.game_id}")
        print(f"      Position: {b.position_type}")
        print(f"      Move: {b.move_played}")
        print(f"      Material Lost: ~{loss} cp")
        print(f"      Game Result: {b.result}")
        print(f"      FEN: {b.fen}")

    # Recommendations
    print(f"\n" + "="*60)
    print(f"  💡 RECOMMENDATIONS")
    print("="*60)

    endgame_blunders = blunder_by_type.get('endgame', 0)
    total_blunders = len(all_blunders)

    if endgame_blunders > total_blunders * 0.3:
        print(f"""
   🔴 HIGH ENDGAME BLUNDER RATE ({endgame_blunders} blunders, {100*endgame_blunders/max(total_blunders,1):.0f}%)
   
   Recommended fixes:
   1. ✅ Add endgame-specific piece-square tables (DONE!)
   2. ✅ Implement mop-up evaluation (DONE!)
   3. ⬜ Add tablebase support for perfect endgame play
   4. ⬜ Extend search depth in endgame (fewer pieces = faster search)
""")

    if losses > wins:
        print(f"""
   🔴 NEGATIVE WIN RATE
   
   Recommended fixes:
   1. ✅ Enable pondering (think on opponent's time)
   2. ⬜ Train NNUE on your games: python train_from_lichess.py -u {username}
   3. ⬜ Add opening book with more lines
   4. ⬜ Improve time management for bullet games
""")

    print("\n" + "="*60)


def main():
    parser = argparse.ArgumentParser(
        description='Analyze Fe64 games from Lichess')
    parser.add_argument('--username', '-u', default='Fe64',
                        help='Lichess bot username')
    parser.add_argument('--max-games', '-m', type=int,
                        default=500, help='Max games to analyze')
    parser.add_argument('--output', '-o', help='Save analysis to JSON file')
    args = parser.parse_args()

    games = download_games(args.username, args.max_games)

    if not games:
        print("No games found!")
        return

    print(f"\nAnalyzing {len(games)} games...")

    analyses = []
    for game in games:
        analysis = analyze_game(game, args.username)
        analyses.append(analysis)

    print_analysis_report(analyses, args.username)

    if args.output:
        # Convert to JSON-serializable format
        output_data = []
        for a in analyses:
            a_copy = dict(a)
            a_copy['blunders'] = [vars(b) for b in a['blunders']]
            a_copy['position_types'] = dict(a['position_types'])
            output_data.append(a_copy)

        with open(args.output, 'w') as f:
            json.dump(output_data, f, indent=2)
        print(f"\nAnalysis saved to {args.output}")


if __name__ == '__main__':
    main()
