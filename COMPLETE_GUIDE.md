# Fe64 Chess Engine - Complete Setup & Usage Guide

## 🎯 Overview

**Fe64** (bbc.c) is a UCI-compatible chess engine written in C with the following features:

### Core Features

- **Magic Bitboard Move Generation** - Ultra-fast legal move generation
- **Neural Network Evaluation (NNUE)** - Optional advanced evaluation
- **Polyglot Opening Book** - Opening theory support
- **Pondering** - Thinks during opponent's time
- **SEE (Static Exchange Evaluation)** - Smart capture ordering
- **Advanced Pruning** - LMR, LMP, Null-Move, Delta, Futility
- **Counter-Move Heuristic** - Enhanced move ordering
- **Butterfly History** - Position-independent history scores
- **Transposition Table** - 4M+ positions cached

### Current Lichess Ratings

- Bullet: 1897
- Blitz: 1660
- Rapid: 1614

### Target Rating: 3000+

---

## 📦 Installation

### Prerequisites

**Linux (Ubuntu/Debian):**

```bash
sudo apt update
sudo apt install -y build-essential git python3 python3-pip python3-venv curl wget
```

**macOS:**

```bash
xcode-select --install
brew install python3 wget curl
```

**Windows:**

- Install [MSYS2](https://www.msys2.org/) or [WSL2](https://docs.microsoft.com/en-us/windows/wsl/install)
- Install [MinGW-w64](https://www.mingw-w64.org/) for cross-compilation

### Step 1: Compile the Engine

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine/src
make
```

This creates:

- `../bin/bbc` - Linux executable
- `../bin/bbc.exe` - Windows executable

### Step 2: Verify Installation

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine/bin
./bbc
```

Then type these UCI commands:

```
uci
isready
go depth 10
quit
```

You should see the engine respond with moves.

---

## 🎮 Running the Engine

### Method 1: Command Line (UCI Mode)

```bash
./bin/bbc
```

UCI commands:

```
uci                          # Initialize UCI protocol
setoption name Hash value 256    # Set hash table to 256MB
setoption name Contempt value 20 # Slightly avoid draws
position startpos            # Set starting position
position fen <fen_string>    # Set custom position
position startpos moves e2e4 e7e5  # Play moves from start
go depth 20                  # Search to depth 20
go movetime 5000             # Search for 5 seconds
go wtime 60000 btime 60000   # Tournament time control
go infinite                  # Search until "stop"
stop                         # Stop searching
quit                         # Exit engine
```

### Method 2: Chess GUI

Load `bin/bbc` (or `bin/bbc.exe`) in any UCI-compatible GUI:

- **Arena Chess GUI** (Free) - https://www.playwitharena.de/
- **CuteChess** (Free) - https://cutechess.com/
- **ChessBase** (Paid)
- **Scid vs PC** (Free) - http://scidvspc.sourceforge.net/

### Method 3: Lichess Bot

See the **Lichess Bot Setup** section below.

---

## ⚙️ UCI Options

| Option       | Type   | Default | Range       | Description                             |
| ------------ | ------ | ------- | ----------- | --------------------------------------- |
| `Hash`       | spin   | 64      | 1-4096      | Transposition table size in MB          |
| `Contempt`   | spin   | 0       | -100 to 100 | Draw avoidance (positive = avoid draws) |
| `MultiPV`    | spin   | 1       | 1-255       | Number of principal variations to show  |
| `SyzygyPath` | string | empty   | -           | Path to Syzygy tablebase files          |

**Recommended Settings:**

```
setoption name Hash value 256
setoption name Contempt value 20
```

---

## 📚 Opening Book Setup

### Download a Polyglot Opening Book

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine
mkdir -p books
cd books

# Option 1: Human book (varied play)
wget https://github.com/michaeldv/donna_opening_books/raw/master/gm2001.bin -O opening.bin

# Option 2: Cerebellum (strong play)
wget https://github.com/TheUltragon/Chess-Engine/raw/master/Cerebellum_Light.bin -O opening.bin

# Option 3: Perfect2021 (very strong)
wget https://chess.cygnitec.com/perfect2021.bin -O opening.bin
```

### Configure Book in Engine

The engine looks for `performance.bin` by default. Rename your book:

```bash
mv opening.bin performance.bin
```

Or modify line ~4050 in `bbc.c`:

```c
char *book_file = "books/your_book_name.bin";
```

---

## 🧠 NNUE (Neural Network) Setup

NNUE provides much stronger evaluation than hand-crafted evaluation.

### Option 1: Use Pre-trained NNUE

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine
mkdir -p nnue

# Download Stockfish-compatible NNUE (if supported)
wget https://tests.stockfishchess.org/api/nn/nn-5af11540bbfe.nnue -O nnue/default.nnue
```

### Option 2: Train Your Own NNUE

See **Training from Your Games** section below.

### Enable NNUE in Engine

The engine auto-detects NNUE files. Make sure `nnue` flag is set in code.

---

## 📊 Syzygy Tablebase Setup

Syzygy endgame tablebases provide **perfect play** in endgames with ≤7 pieces.

### Download Tablebases

**3-4-5 piece tables (~1GB):**

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine
mkdir -p syzygy

# Download 3-4-5 piece WDL files
cd syzygy
wget -r -np -nH --cut-dirs=2 -R "index.html*" https://tablebase.lichess.ovh/tables/standard/3-4-5/
```

**6 piece tables (~155GB):**

```bash
# Only if you have disk space!
wget -r -np -nH --cut-dirs=2 -R "index.html*" https://tablebase.lichess.ovh/tables/standard/6-wdl/
```

### Configure Syzygy Path

In UCI:

```
setoption name SyzygyPath value /home/syedmasoodhussain/Desktop/chess_engine/syzygy
```

---

## 🤖 Lichess Bot Setup

### Step 1: Create Bot Account

1. Create a new Lichess account at https://lichess.org/signup
2. **Important:** You CANNOT upgrade an account with played games to BOT

### Step 2: Get OAuth Token

1. Go to https://lichess.org/account/oauth/token/create
2. Select scopes:
   - `bot:play`
   - `challenge:read`
   - `challenge:write`
3. Create token and **save it securely**

### Step 3: Upgrade to BOT Account

```bash
# Replace YOUR_TOKEN with your actual token
curl -X POST https://lichess.org/api/bot/account/upgrade \
  -H "Authorization: Bearer YOUR_TOKEN"
```

⚠️ **WARNING:** This is IRREVERSIBLE! The account can only play as a bot.

### Step 4: Configure lichess-bot

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine/lichess-bot
cp config.yml.default config.yml
nano config.yml  # or use any editor
```

**Edit config.yml:**

```yaml
token: "YOUR_LICHESS_TOKEN"

engine:
  dir: "../bin/"
  name: "bbc"
  protocol: "uci"
  ponder: true

  uci_options:
    Hash: 256
    Contempt: 20

challenge:
  accept:
    rated: true
    casual: true
    tc:
      - bullet
      - blitz
      - rapid
```

### Step 5: Install Python Dependencies

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine/lichess-bot
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Step 6: Run the Bot

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine/lichess-bot
source venv/bin/activate
python3 lichess-bot.py
```

### Keep Bot Running 24/7

**Option 1: Screen**

```bash
screen -S lichessbot
python3 lichess-bot.py
# Press Ctrl+A, then D to detach
# Reconnect with: screen -r lichessbot
```

**Option 2: Systemd Service**

```bash
sudo nano /etc/systemd/system/lichess-bot.service
```

```ini
[Unit]
Description=Lichess Bot
After=network.target

[Service]
Type=simple
User=syedmasoodhussain
WorkingDirectory=/home/syedmasoodhussain/Desktop/chess_engine/lichess-bot
ExecStart=/home/syedmasoodhussain/Desktop/chess_engine/lichess-bot/venv/bin/python lichess-bot.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable lichess-bot
sudo systemctl start lichess-bot
sudo systemctl status lichess-bot
```

---

## 📈 Training from Your Lichess Games

### Step 1: Setup Training Environment

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine
python3 -m venv training_env
source training_env/bin/activate
pip install requests chess numpy
```

### Step 2: Download Your Games

```bash
cd /home/syedmasoodhussain/Desktop/chess_engine/training
mkdir -p games

# Replace YOUR_USERNAME with your Lichess username
curl "https://lichess.org/api/games/user/YOUR_USERNAME?max=500&pgnInJson=true" \
  -H "Accept: application/x-ndjson" > games/my_games.ndjson
```

### Step 3: Analyze Games for Weaknesses

```bash
python3 analyze_games.py
```

This will:

- Identify endgame blunder patterns
- Find positions where your engine went wrong
- Generate recommendations for improvement

### Step 4: Train NNUE from Games

```bash
python3 train_from_lichess.py --username YOUR_USERNAME --max-games 381
```

**Advanced training options:**

```bash
python3 train_from_lichess.py \
  --username YOUR_USERNAME \
  --max-games 1000 \
  --epochs 100 \
  --learning-rate 0.001 \
  --output nnue_trained.bin
```

---

## 🔧 Engine Tuning Guide

### For Stronger Endgames

The engine now includes:

1. **Mop-up Evaluation** - Drives opponent king to corner when winning
2. **Endgame King PST** - King becomes active in endgames
3. **Enhanced Passed Pawn Bonuses** - Higher values in endgames

### Adjusting Search Parameters

Edit these in `bbc.c` around line 230:

```c
// Late Move Pruning margins
int lmp_margins[4] = {0, 8, 12, 20};  // Increase for more aggressive pruning

// Futility margins by depth
int futility_margins[4] = {0, 100, 200, 300};

// Contempt (avoid draws when ahead)
int contempt = 20;  // Higher = more aggressive
```

### Time Management

For tournament play, the engine automatically manages time. For analysis:

```
go infinite    # Unlimited time
go depth 30    # Fixed depth
go movetime 10000  # 10 seconds per move
```

---

## 🏆 Rating Improvement Checklist

### Immediate Gains (Done ✅)

- [x] Pondering (think during opponent's time)
- [x] SEE (better capture ordering)
- [x] Counter-move heuristic
- [x] LMP (Late Move Pruning)
- [x] Butterfly history
- [x] Endgame evaluation improvements
- [x] Mop-up evaluation

### Medium-Term Improvements

- [ ] Syzygy tablebase integration
- [ ] Multi-threaded search (Lazy SMP)
- [ ] Better time management
- [ ] NNUE training from 100K+ games
- [ ] Singular extensions
- [ ] Internal Iterative Deepening

### Long-Term Goals

- [ ] Full NNUE HalfKP architecture
- [ ] 7-piece Syzygy support
- [ ] GPU-accelerated NNUE inference
- [ ] Distributed search

---

## 🐛 Troubleshooting

### Engine Won't Compile

```bash
# Check GCC installation
gcc --version

# Install if missing
sudo apt install build-essential

# For pthread errors
sudo apt install libc6-dev
```

### Lichess Bot Won't Connect

1. Check token is correct in config.yml
2. Verify account is upgraded to BOT
3. Check internet connection
4. Look at logs: `lichess_bot_auto_logs/`

### Engine Plays Badly in Endgames

1. Ensure Syzygy path is set correctly
2. Check that tablebase files downloaded properly
3. Increase hash table size

### Pondering Not Working

1. Verify `ponder: true` in lichess-bot config
2. Check that pthread is linked (`-lpthread` in makefile)

---

## 📁 File Structure

```
chess_engine/
├── bin/
│   ├── bbc           # Linux executable
│   └── bbc.exe       # Windows executable
├── src/
│   ├── bbc.c         # Main engine source
│   └── makefile      # Build configuration
├── lichess-bot/
│   ├── config.yml    # Bot configuration
│   └── lichess-bot.py
├── training/
│   ├── analyze_games.py
│   └── train_from_lichess.py
├── books/
│   └── performance.bin   # Opening book
├── syzygy/
│   └── (tablebase files)
├── nnue/
│   └── default.nnue
└── scripts/
    └── setup.sh
```

---

## 🚀 Quick Start Commands

```bash
# Full setup
cd /home/syedmasoodhussain/Desktop/chess_engine
bash scripts/setup.sh

# Compile only
cd src && make

# Test engine
cd bin && ./bbc

# Run Lichess bot
cd lichess-bot
source venv/bin/activate
python3 lichess-bot.py

# Train from games
cd training
source ../training_env/bin/activate
python3 train_from_lichess.py --username YOUR_USERNAME
```

---

## 📞 Support

- **Lichess Bot Documentation:** https://github.com/lichess-bot-devs/lichess-bot
- **UCI Protocol Spec:** https://www.shredderchess.com/chess-features/uci-universal-chess-interface.html
- **Syzygy Tables:** https://syzygy-tables.info/

---

## 📝 Version History

- **v2.0** - Added pondering, SEE, counter-moves, LMP, endgame improvements
- **v1.0** - Initial release with NNUE support

---

**Happy Chess Programming! 🎲♟️**
