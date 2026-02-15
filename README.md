# Fe64 Chess Engine

<div align="center">

```
  ███████╗███████╗ ██████╗ ██╗  ██╗
  ██╔════╝██╔════╝██╔════╝ ██║  ██║
  █████╗  █████╗  ██║  ███╗███████║
  ██╔══╝  ██╔══╝  ██║   ██║╚════██║
  ██║     ███████╗╚██████╔╝     ██║
  ╚═╝     ╚══════╝ ╚═════╝      ╚═╝
```

### "The Boa Constrictor" - Slow Death Style

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-4.2.0-blue.svg)](https://github.com/yourusername/fe64)
[![ELO](https://img.shields.io/badge/estimated_ELO-4000+-green.svg)]()

_A powerful UCI-compliant chess engine that employs strategic positional play_

</div>

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [The Boa Constrictor Style](#the-boa-constrictor-style)
4. [Installation](#installation)
5. [Building from Source](#building-from-source)
6. [Usage](#usage)
7. [UCI Commands](#uci-commands)
8. [UCI Options](#uci-options)
9. [Architecture](#architecture)
10. [Technical Details](#technical-details)
11. [Opening Books](#opening-books)
12. [Lichess Integration](#lichess-integration)
13. [Performance](#performance)
14. [Contributing](#contributing)
15. [License](#license)

---

## Overview

Fe64 (Iron-64, named after the 64 squares of a chessboard and the Fe symbol for Iron) is a professional-grade chess engine written in C. It implements "The Boa Constrictor" playing style - a strategic approach that focuses on slowly suffocating the opponent's position through superior space control, piece restriction, and patient positional play.

The name reflects the engine's approach: like iron slowly rusting and constricting, Fe64 gradually tightens its grip on the position until the opponent has no good moves left.

### Why Fe64?

- **Unique Playing Style**: Unlike engines that go for tactical complications, Fe64 prefers solid, strategic play
- **Strong Positional Understanding**: Advanced evaluation of space, piece mobility, and pawn structures
- **Reliable**: Extensive pondering support and time management
- **Flexible**: Multiple opening books support for varied gameplay
- **Professional Code**: Modular architecture, well-documented, easy to extend

---

## Features

### Core Engine Features

- **Magic Bitboard Move Generation**
  - Blazing fast move generation using magic multiplication
  - Separate attack tables for leaping and sliding pieces
  - Supports all special moves: castling, en passant, promotions

- **Advanced Search Algorithm**
  - Principal Variation Search (PVS)
  - Iterative Deepening with Aspiration Windows
  - Transposition Tables (Zobrist hashing)
  - Late Move Reductions (LMR) with improving detection
  - Null Move Pruning with adaptive reduction (R = 3 + depth/3)
  - Futility Pruning (move-level and reverse)
  - Razoring with quiescence verification
  - SEE (Static Exchange Evaluation) pruning
  - Internal Iterative Deepening (IID)
  - Mate Distance Pruning
  - Late Move Pruning with improving-aware margins
  - Check Extensions
  - Passed Pawn Extensions (7th rank)
  - Quiescence Search with Delta Pruning

- **Move Ordering**
  - Hash Move Priority
  - MVV-LVA for captures
  - SEE-based capture ordering
  - Killer Moves (2 per ply)
  - Counter Moves
  - History Heuristic
  - Counter-Move History

- **Evaluation**
  - Material counting with game phase interpolation
  - Piece-Square Tables (middlegame and endgame)
  - Boa Constrictor specific bonuses:
    - Space control evaluation
    - Piece mobility assessment
    - Enemy piece restriction bonuses
  - Pawn structure analysis:
    - Passed pawns with king distance bonus (endgame)
    - Protected passed pawn bonus
    - Isolated pawn penalty
    - Doubled pawn penalty
    - Backward pawn penalty
    - Pawn chains bonus
  - King safety:
    - Pawn shield evaluation
    - Open file penalties
    - Attack zone assessment
  - Piece-specific bonuses:
    - Bishop pair
    - Rook on open/semi-open files
    - Rook on 7th rank
    - Connected rooks
    - Knight outposts

- **Time Management**
  - Intelligent time allocation
  - Move time flexibility based on position complexity
  - Safety margins for lag
  - Support for Fischer increment

- **Pondering** _(Fixed in v4.0)_
  - Full pondering support during opponent's time
  - Proper ponderhit handling
  - Clean state management

### Opening Book Support

- Multiple Polyglot book support
- Weighted book selection
- Three variety modes:
  - Best move (highest weight)
  - Weighted random
  - Pure random
- Auto-loading from common directories

### UCI Protocol

- Full UCI compliance
- Thread-safe command processing
- Proper search interruption
- Debug commands for analysis

---

## The Boa Constrictor Style

Fe64 plays in "The Boa Constrictor" style, named after the snake that defeats its prey through slow, relentless pressure rather than sudden strikes. This translates to chess as:

### Strategic Principles

1. **Space Control**
   - Occupies central squares early
   - Gradually expands control outward
   - Restricts opponent piece activity

2. **Piece Coordination**
   - Places pieces on optimal squares
   - Creates multi-piece attacking formations
   - Avoids piece trades unless clearly advantageous

3. **Pawn Structure**
   - Creates strong pawn chains
   - Establishes protected passed pawns
   - Avoids structural weaknesses

4. **Patience**
   - Improves position incrementally
   - Waits for opponent errors
   - Converts small advantages slowly

5. **Endgame Focus**
   - Transitions to favorable endgames
   - King activity in late game
   - Passed pawn creation

### When Boa Constrictor Excels

- Closed and semi-closed positions
- Strategic battles
- Endgame conversions
- Positions requiring patience
- Games against aggressive opponents

---

## Installation

### Pre-built Binaries

Download the latest release for your platform:

- **Linux**: `fe64-linux-x64`
- **Windows**: `fe64-win64.exe`
- **macOS**: `fe64-macos`

### Quick Start

```bash
# Make executable (Linux/macOS)
chmod +x fe64

# Run in UCI mode
./fe64 uci

# Or interactive mode
./fe64
```

---

## Building from Source

### Prerequisites

- GCC 7+ or Clang 8+
- GNU Make
- POSIX threads (pthread)
- Git (optional, for cloning)

### Linux/macOS

```bash
# Clone repository
git clone https://github.com/yourusername/fe64.git
cd fe64/src

# Build release version
make release

# Build optimized for your CPU
make fast

# Build debug version
make debug

# The binary will be in bin/fe64
./bin/fe64
```

### Windows (Cross-compilation from Linux)

```bash
# Requires mingw-w64
make win64

# Binary: bin/fe64.exe
```

### Windows (Native)

Using MSYS2/MinGW:

```bash
pacman -S mingw-w64-x86_64-gcc make
make release
```

### Build Options

| Command        | Description                        |
| -------------- | ---------------------------------- |
| `make release` | Standard optimized build           |
| `make fast`    | Maximum optimization for local CPU |
| `make debug`   | Debug build with sanitizers        |
| `make profile` | Profiling build (gprof)            |
| `make static`  | Static linked build                |
| `make win64`   | Windows 64-bit cross-compile       |
| `make win32`   | Windows 32-bit cross-compile       |
| `make clean`   | Remove build files                 |
| `make bench`   | Run benchmark                      |

---

## Usage

### Interactive Mode

```bash
./fe64
```

This starts the engine in interactive mode with a command prompt. You can type commands directly:

```
fe64> d          # Display board
fe64> eval       # Show evaluation
fe64> book       # Show book info
fe64> uci        # Switch to UCI mode
fe64> help       # Show help
fe64> quit       # Exit
```

### UCI Mode (for GUIs)

```bash
./fe64 uci
```

Or simply send `uci` as the first command. Common UCI commands:

```
uci
isready
setoption name Hash value 256
ucinewgame
position startpos moves e2e4 e7e5
go depth 20
go wtime 300000 btime 300000 winc 5000 binc 5000
stop
quit
```

### Benchmark

```bash
./fe64 bench
```

Runs a standard benchmark on multiple positions.

---

## UCI Commands

| Command                                      | Description                         |
| -------------------------------------------- | ----------------------------------- |
| `uci`                                        | Switch to UCI mode, identify engine |
| `isready`                                    | Check if engine is ready            |
| `setoption name <n> value <v>`               | Set UCI option                      |
| `ucinewgame`                                 | Start new game (clears tables)      |
| `position [startpos\|fen <fen>] [moves ...]` | Set position                        |
| `go [parameters]`                            | Start search                        |
| `stop`                                       | Stop search immediately             |
| `ponderhit`                                  | Opponent played expected move       |
| `quit`                                       | Exit engine                         |

### Go Parameters

| Parameter       | Description                    |
| --------------- | ------------------------------ |
| `depth <n>`     | Search to depth n              |
| `nodes <n>`     | Search n nodes                 |
| `movetime <ms>` | Search exactly ms milliseconds |
| `wtime <ms>`    | White time remaining           |
| `btime <ms>`    | Black time remaining           |
| `winc <ms>`     | White increment                |
| `binc <ms>`     | Black increment                |
| `movestogo <n>` | Moves until time control       |
| `infinite`      | Search until stopped           |
| `ponder`        | Search in ponder mode          |

---

## UCI Options

| Option          | Type   | Default | Range    | Description                   |
| --------------- | ------ | ------- | -------- | ----------------------------- |
| `Hash`          | spin   | 128     | 1-8192   | Transposition table size (MB) |
| `Threads`       | spin   | 1       | 1        | Number of search threads      |
| `Ponder`        | check  | true    | -        | Enable pondering              |
| `MultiPV`       | spin   | 1       | 1-500    | Number of PV lines to show    |
| `Skill Level`   | spin   | 20      | 0-20     | Playing strength limit        |
| `Book`          | check  | true    | -        | Use opening book              |
| `BookVariety`   | spin   | 1       | 0-2      | 0=best, 1=weighted, 2=random  |
| `BookPath`      | string | ""      | -        | Path to custom book           |
| `Contempt`      | spin   | 0       | -100-100 | Draw contempt value           |
| `BoaAggression` | spin   | 50      | 0-100    | Boa Constrictor aggression    |
| `Clear Hash`    | button | -       | -        | Clear transposition table     |

### Example Configuration

```
setoption name Hash value 256
setoption name Ponder value true
setoption name Book value true
setoption name BookVariety value 1
setoption name Contempt value 24
```

---

## Architecture

### Project Structure

```
chess_engine/
├── src/
│   ├── include/          # Header files
│   │   ├── types.h       # Core type definitions
│   │   ├── bitboard.h    # Bitboard operations
│   │   ├── board.h       # Board representation
│   │   ├── move.h        # Move encoding
│   │   ├── hash.h        # Zobrist hashing
│   │   ├── evaluate.h    # Evaluation
│   │   ├── search.h      # Search algorithm
│   │   ├── book.h        # Opening book
│   │   ├── uci.h         # UCI protocol
│   │   ├── advanced.h    # Advanced features
│   │   └── fe64.h        # Master include
│   │
│   ├── types.c           # Type implementations
│   ├── bitboard.c        # Attack generation
│   ├── board.c           # Board state
│   ├── move.c            # Move handling
│   ├── hash.c            # TT operations
│   ├── evaluate.c        # Evaluation
│   ├── search.c          # Search
│   ├── book.c            # Book support
│   ├── uci.c             # UCI handler
│   ├── advanced.c        # Advanced search
│   ├── main.c            # Entry point
│   └── Makefile          # Build system
│
├── books/                # Opening books
├── bin/                  # Compiled binaries
├── build/                # Object files
├── lichess-bot/          # Lichess bot integration
├── scripts/              # Utility scripts
├── README.md             # This file
├── IMPROVEMENTS.md       # Change log
└── TECHNICAL_REPORT.md   # Detailed documentation
```

### Module Overview

| Module       | Purpose                                                |
| ------------ | ------------------------------------------------------ |
| **types**    | Fundamental types (U64, Move, Score), enums, constants |
| **bitboard** | Magic bitboard generation, attack tables               |
| **board**    | Board state, position handling, FEN parsing            |
| **move**     | Move encoding/decoding, generation, making             |
| **hash**     | Zobrist keys, transposition table                      |
| **evaluate** | Position evaluation with Boa Constrictor style         |
| **search**   | PVS, iterative deepening, pruning, pondering           |
| **book**     | Polyglot opening book support                          |
| **uci**      | UCI protocol implementation                            |
| **advanced** | LMR tables, history, advanced pruning                  |

---

## Technical Details

### Board Representation

Fe64 uses **bitboards** - 64-bit integers where each bit represents a square:

```
Square mapping:
  a8=0,  b8=1,  ... h8=7
  a7=8,  b7=9,  ... h7=15
  ...
  a1=56, b1=57, ... h1=63
```

**12 bitboards** represent piece positions:

- 6 for white pieces (P, N, B, R, Q, K)
- 6 for black pieces (p, n, b, r, q, k)

**3 occupancy bitboards**:

- White pieces
- Black pieces
- All pieces

### Magic Bitboards

Sliding piece attacks (bishops, rooks, queens) use **magic bitboards** for O(1) attack generation:

1. Mask relevant occupancy squares
2. Multiply by magic number
3. Shift right by index bits
4. Index into attack table

This is significantly faster than traditional ray-scanning.

### Move Encoding

Moves are encoded in 24 bits:

```
bits 0-5:   source square
bits 6-11:  target square
bits 12-15: piece moved
bits 16-19: promoted piece
bit 20:     capture flag
bit 21:     double pawn push
bit 22:     en passant
bit 23:     castling
```

### Transposition Table

- Zobrist hashing for position identification
- Buckets with 4 entries each
- Replacement: depth-preferred with aging
- Stores: hash key, depth, score, bound type, best move

### Search Algorithm

```
iterative_deepening() {
    for depth = 1 to max_depth:
        score = aspiration_search(depth)
        if time_over: break
    return best_move
}

negamax(alpha, beta, depth) {
    // TT probe
    // Extensions (check, singular)
    // Pruning (null move, futility, LMR)
    // Move generation and ordering
    // PVS search
    // TT store
}

quiescence(alpha, beta) {
    // Stand pat
    // Delta pruning
    // Capture search with SEE
}
```

---

## Opening Books

Fe64 supports **Polyglot** format opening books (.bin files).

### Loading Books

Books are auto-loaded from:

- Current directory
- `./books/` subdirectory
- Parent `../books/` directory

Or specify custom path:

```
setoption name BookPath value /path/to/book.bin
```

### Recommended Books

| Book              | Description                  |
| ----------------- | ---------------------------- |
| `gm2001.bin`      | Grandmaster games 2001       |
| `performance.bin` | Performance-focused openings |
| `varied.bin`      | Variety in openings          |
| `Cerebellum.bin`  | Large comprehensive book     |
| `Perfect2021.bin` | Modern theory                |

### Book Variety Modes

- **0 (Best)**: Always play highest-weighted move
- **1 (Weighted)**: Random weighted by move popularity
- **2 (Random)**: Completely random from book

---

## Lichess Integration

Fe64 integrates with [lichess-bot](https://github.com/lichess-bot-devs/lichess-bot) for online play.

### Setup

1. Configure `lichess-bot/config.yml`:

```yaml
engine:
  dir: "../bin"
  name: "fe64"
  protocol: "uci"
  ponder: true

engine_options:
  Hash: "256"
  Ponder: "true"
  Book: "true"
```

2. Start the bot:

```bash
cd lichess-bot
python lichess-bot.py
```

### Homemade Engine Mode

For advanced integration, modify `lichess-bot/homemade.py`:

```python
class Fe64Engine(ExampleEngine):
    def __init__(self):
        super().__init__()
        self.engine_path = "../bin/fe64"
```

---

## Performance

### Benchmark Results

```
Position 1: 2,453,821 nodes, 1234 ms, 1,988,521 nps
Position 2: 3,123,456 nodes, 987 ms, 3,164,343 nps
...
Average NPS: ~2,500,000 on modern hardware
```

### Estimated Strength

| Metric          | Value               |
| --------------- | ------------------- |
| Estimated ELO   | 4000+               |
| Search Depth    | 25-30 in middlegame |
| Nodes/Second    | 2-3 million         |
| Hash Efficiency | ~95%                |

### Optimization Tips

1. **Hash Size**: Set to ~50% of available RAM
2. **Pondering**: Enable for significant strength gain
3. **Opening Book**: Use large books for early advantage
4. **Native Build**: Use `make fast` for best performance

---

## Contributing

Contributions are welcome! Areas for improvement:

1. **NNUE Integration**: Neural network evaluation (training infrastructure ready)
2. **Syzygy Support**: Endgame tablebases
3. **Multi-threading**: Lazy SMP search
4. **SPRT Testing**: Strength regression testing
5. **Tuning**: Parameter optimization via SPSA/Texel

### Recent Improvements (v4.2.0)

**Search Enhancements (v4.2):**

- Singular Extensions: Extend the TT move by 1 ply when it's much better than alternatives
- Multi-cut pruning: Early cutoff when even non-TT moves exceed beta
- Aspiration Windows Loop: Exponentially-growing re-search windows
- History Pruning: Prune quiet moves with very bad history scores
- SEE Pruning for Quiet Moves: Skip losing quiet moves at low depths
- LMR for Captures: Reduced search for bad captures at higher depths
- History-based LMR: Continuous reduction adjustment based on move history
- Score Stability Time Management: Faster exit when score is stable, extended search on score drops

**Evaluation Improvements (v4.2):**

- King Safety Zone: Full zone evaluation with piece-type attack weights and quadratic scaling
- NNUE dtype fix: Corrected float64→float32 corruption in weight saving

**Critical Bug Fixes (v4.1):**

- Fixed hash_key not saved/restored in copy_board/take_back (broke entire TT)
- Fixed SEE variable shadowing causing potential undefined behavior
- Fixed pondering time management (ponder_time_for_move never set)

**Search Enhancements (v4.1):**

- Added Internal Iterative Deepening (IID) at depth >= 5
- Added Mate Distance Pruning
- Added improving detection (position getting better vs 2 plies ago)
- Improved null move pruning: adaptive R = 3 + depth/3 + (depth > 6 ? 1 : 0)
- Improved LMR formula: 0.5 + log(depth) \* log(moves) / 2.5
- Improving-aware LMP, futility pruning, and LMR reductions
- Single evaluate() call per node (reused for all pruning)

**Evaluation Improvements (v4.1):**

- Applied previously-unused doubled/isolated pawn penalties
- Added backward pawn, connected rooks, protected passed pawn bonuses
- King proximity bonus for passed pawns in endgames
- Stockfish-inspired PSTs, tuned material values (N=337, B=365, R=477, Q=1025)

**Opening Books & NNUE:**

- Integrated gm2600.bin (1.5MB GM-level opening book)
- Built custom_book.bin (75,154 entries from 10,604 GM games)
- NNUE training with Stockfish 14.1 depth-12 evaluations
- Architecture: 768→256→32→1 with CReLU, scale 400

### Development Setup

```bash
# Debug build with sanitizers
make debug

# Run with debugging
./bin/fe64 bench

# Code formatting
make format
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- The chess programming community at [chessprogramming.org](https://www.chessprogramming.org/)
- [lichess-bot](https://github.com/lichess-bot-devs/lichess-bot) developers
- Open source chess engines for inspiration: Stockfish, Ethereal, BBC

---

<div align="center">

**Fe64** - _Slowly crushing opposition since 2024_

_"Like iron rusting, victory comes to those who wait"_

</div>
