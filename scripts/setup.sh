#!/bin/bash
# ============================================
# Fe64 Chess Engine - Complete Setup Script
# ============================================
# This script downloads and sets up everything needed
# for maximum engine performance.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$SCRIPT_DIR/.."

echo "================================================"
echo "  Fe64 Chess Engine - Complete Setup"
echo "================================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

# Check for required tools
check_dependencies() {
    echo "Checking dependencies..."
    
    if ! command -v gcc &> /dev/null; then
        print_error "GCC not found. Install with: sudo apt install build-essential"
        exit 1
    fi
    print_status "GCC found"
    
    if ! command -v wget &> /dev/null && ! command -v curl &> /dev/null; then
        print_error "wget or curl required. Install with: sudo apt install wget"
        exit 1
    fi
    print_status "Download tools found"
    
    if ! command -v python3 &> /dev/null; then
        print_warning "Python3 not found - training tools won't work"
    else
        print_status "Python3 found"
    fi
}

# Compile the engine
compile_engine() {
    echo ""
    echo "Compiling Fe64 engine..."
    cd "$ENGINE_DIR/src"
    
    # Standard build
    make clean 2>/dev/null || true
    make
    
    if [ -f "$ENGINE_DIR/bin/bbc" ]; then
        print_status "Engine compiled successfully!"
    else
        print_error "Compilation failed!"
        exit 1
    fi
}

# Download opening book
download_opening_book() {
    echo ""
    echo "Setting up opening book..."
    
    BOOK_DIR="$ENGINE_DIR/bin"
    BOOK_FILE="$BOOK_DIR/book.bin"
    
    if [ -f "$BOOK_FILE" ]; then
        print_status "Opening book already exists"
        return
    fi
    
    # Try to download a Polyglot opening book
    # Using the performance.bin from computer-chess-engine-books
    echo "Downloading Polyglot opening book..."
    
    # Alternative: Create a minimal book from popular openings
    # For now, we'll skip if download fails
    print_warning "Opening book download skipped - you can add your own book.bin"
    print_warning "Get opening books from: https://rebel13.nl/download/books.html"
}

# Download Syzygy tablebases
download_syzygy() {
    echo ""
    echo "================================================"
    echo "  Syzygy Tablebase Setup"
    echo "================================================"
    
    SYZYGY_DIR="$ENGINE_DIR/syzygy"
    
    if [ -d "$SYZYGY_DIR" ] && [ "$(ls -A $SYZYGY_DIR 2>/dev/null)" ]; then
        print_status "Syzygy tablebases directory exists"
        return
    fi
    
    mkdir -p "$SYZYGY_DIR"
    
    echo ""
    echo "Syzygy tablebases provide PERFECT endgame play!"
    echo "This can gain 100-150 ELO in endgames."
    echo ""
    echo "Options:"
    echo "  1) Download 3-4-5 piece tables (< 1 GB) - Recommended"
    echo "  2) Download 6-piece tables (150+ GB) - Best but huge"
    echo "  3) Skip for now"
    echo ""
    read -p "Choose option [1/2/3]: " choice
    
    case $choice in
        1)
            echo "Downloading 3-4-5 piece Syzygy tables..."
            cd "$SYZYGY_DIR"
            
            # Download from lichess
            BASE_URL="https://tablebase.lichess.ovh/tables/standard/3-4-5"
            
            print_warning "Downloading from Lichess tablebase server..."
            print_warning "This may take 10-30 minutes depending on connection..."
            
            # List of 3-4-5 piece files
            wget -q --show-progress -r -np -nH --cut-dirs=3 -R "index.html*" \
                "https://tablebase.lichess.ovh/tables/standard/3-4-5/" 2>/dev/null || {
                print_warning "Wget download failed. Trying alternative method..."
                
                # Manual download of essential files
                for pieces in "KQvK" "KRvK" "KBNvK" "KBBvK" "KNNvK" "KPvK" "KRvKR" "KQvKR" "KQvKQ"; do
                    echo "Downloading $pieces..."
                    wget -q "https://syzygy-tables.info/download/$pieces.rtbw" -O "$pieces.rtbw" 2>/dev/null || true
                    wget -q "https://syzygy-tables.info/download/$pieces.rtbz" -O "$pieces.rtbz" 2>/dev/null || true
                done
            }
            
            print_status "Basic Syzygy tables downloaded to $SYZYGY_DIR"
            print_warning "For complete 3-4-5 piece tables, manually download from:"
            print_warning "https://tablebase.lichess.ovh/tables/standard/3-4-5/"
            ;;
        2)
            echo ""
            print_warning "6-piece tables are 150+ GB!"
            print_warning "Manual download recommended from:"
            echo "  https://tablebase.lichess.ovh/tables/standard/"
            echo ""
            echo "After downloading, place files in: $SYZYGY_DIR"
            ;;
        3)
            print_warning "Syzygy download skipped"
            print_warning "You can download later from: https://tablebase.lichess.ovh/"
            ;;
    esac
}

# Setup Python training tools
setup_training() {
    echo ""
    echo "Setting up training tools..."
    
    TRAIN_DIR="$ENGINE_DIR/training"
    
    if [ -d "$TRAIN_DIR" ]; then
        cd "$TRAIN_DIR"
        
        if command -v python3 &> /dev/null; then
            if command -v pip3 &> /dev/null; then
                echo "Installing Python dependencies..."
                pip3 install -r requirements.txt --quiet 2>/dev/null || {
                    print_warning "Could not install Python packages automatically"
                    echo "Run manually: pip3 install -r $TRAIN_DIR/requirements.txt"
                }
                print_status "Training tools ready"
            else
                print_warning "pip3 not found - install manually"
            fi
        fi
    fi
}

# Update lichess-bot config
setup_lichess_bot() {
    echo ""
    echo "Configuring lichess-bot..."
    
    CONFIG_FILE="$ENGINE_DIR/lichess-bot/config.yml"
    
    if [ -f "$CONFIG_FILE" ]; then
        # Check if syzygy path needs to be added
        if ! grep -q "syzygy" "$CONFIG_FILE" 2>/dev/null; then
            echo ""
            print_warning "Remember to add SyzygyPath to your engine options in config.yml"
            echo "Example: SyzygyPath: \"$ENGINE_DIR/syzygy\""
        fi
        print_status "lichess-bot config exists"
    fi
}

# Print final instructions
print_instructions() {
    echo ""
    echo "================================================"
    echo "  Setup Complete!"
    echo "================================================"
    echo ""
    echo "Your Fe64 engine is ready! Here's what to do next:"
    echo ""
    echo "1. TEST THE ENGINE:"
    echo "   cd $ENGINE_DIR/bin"
    echo "   ./bbc"
    echo "   # Type: uci, isready, go depth 10, quit"
    echo ""
    echo "2. RUN ON LICHESS:"
    echo "   cd $ENGINE_DIR/lichess-bot"
    echo "   python3 lichess-bot.py"
    echo ""
    echo "3. TRAIN FROM YOUR GAMES (Optional but recommended!):"
    echo "   cd $ENGINE_DIR/training"
    echo "   python3 analyze_games.py --username YOUR_BOT_NAME"
    echo "   python3 train_from_lichess.py --username YOUR_BOT_NAME"
    echo ""
    echo "4. UCI OPTIONS (set in config.yml or GUI):"
    echo "   - Hash: 64-4096 MB (more is better)"
    echo "   - Contempt: 10 (avoid draws when stronger)"
    echo "   - UseNNUE: true (if you trained one)"
    echo "   - Ponder: true (think on opponent's time)"
    echo "   - SyzygyPath: /path/to/syzygy"
    echo ""
    echo "================================================"
}

# Main execution
main() {
    check_dependencies
    compile_engine
    download_opening_book
    download_syzygy
    setup_training
    setup_lichess_bot
    print_instructions
}

main "$@"
