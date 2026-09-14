# ============================================
#        FE64 CHESS ENGINE - MAKEFILE
#   "The Boa Constrictor" - Slow Death Style
#        MODULAR BUILD SYSTEM
# ============================================

# Compiler
CC = gcc

# Standard flags
CFLAGS = -std=c11 -Wall -Wextra -Wno-comment

# Include path
INCLUDES = -I.

# Release flags (optimized)
RELEASE_FLAGS = -O3 -DNDEBUG -flto -march=native

# Debug flags
DEBUG_FLAGS = -g -O0 -DDEBUG

# Libraries
LIBS = -lpthread -lm

# Directories
BIN_DIR = ../bin
OBJ_DIR = obj

# Source files (modular structure)
SOURCES = main.c \
          bitboard.c \
          attacks.c \
          movegen.c \
          evaluate.c \
          search.c \
          book.c \
          nnue.c \
          uci.c

# Object files
OBJECTS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOURCES))

# Output executable
TARGET = $(BIN_DIR)/fe64

# Header dependencies
HEADERS = types.h

# ============================================
#          BUILD TARGETS
# ============================================

.PHONY: all release fast debug clean test bench info win64 perft

# Default target
all: release

# Release build (optimized)
release: CFLAGS += $(RELEASE_FLAGS)
release: $(TARGET)

# Fast build (maximum optimization for local CPU)
fast: CFLAGS += -O3 -DNDEBUG -flto -march=native -mtune=native -funroll-loops
fast: $(TARGET)

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Profile build
profile: CFLAGS += -O2 -pg -DNDEBUG
profile: LIBS += -pg
profile: $(TARGET)

# NNUE build (with neural network support)
nnue: CFLAGS += $(RELEASE_FLAGS) -DUSE_NNUE
nnue: $(TARGET)

# ============================================
#          COMPILATION RULES
# ============================================

# Create object directory
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Compile source files to object files
$(OBJ_DIR)/%.o: %.c $(HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link all object files into executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBS)
	@echo ""
	@echo "  ███████╗███████╗ ██████╗ ██╗  ██╗"
	@echo "  ██╔════╝██╔════╝██╔════╝ ██║  ██║"
	@echo "  █████╗  █████╗  ██║  ███╗███████║"
	@echo "  ██╔══╝  ██╔══╝  ██║   ██║╚════██║"
	@echo "  ██║     ███████╗╚██████╔╝     ██║"
	@echo "  ╚═╝     ╚══════╝ ╚═════╝      ╚═╝"
	@echo ""
	@echo "  Built $(TARGET) successfully!"
	@echo "  \"The Boa Constrictor\" - Modular Build"
	@echo "  Source files: $(words $(SOURCES))"
	@echo ""

# ============================================
#          UTILITY TARGETS
# ============================================

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET)
	rm -f $(TARGET).exe
	rm -f $(BIN_DIR)/fe64_32.exe
	@echo "Cleaned build artifacts"

# Show source file info
info:
	@echo "Source files:"
	@for src in $(SOURCES); do echo "  - $$src"; done
	@echo ""
	@echo "Header files:"
	@for hdr in $(HEADERS); do echo "  - $$hdr"; done

# Windows 64-bit (cross-compile from Linux)
win64: CC = x86_64-w64-mingw32-gcc
win64: CFLAGS += -O3 -DNDEBUG -static
win64: LIBS = -static -lpthread
win64: TARGET = $(BIN_DIR)/fe64.exe
win64: $(TARGET)

# ============================================
#          TESTING
# ============================================

test: release
	@echo "Running basic UCI test..."
	@echo -e "uci\nisready\nquit" | $(TARGET)

bench: release
	@echo "Running benchmark..."
	@echo -e "uci\nposition startpos\ngo depth 15\nquit" | $(TARGET)

perft: release
	@echo "Running perft test..."
	@echo -e "uci\nposition startpos\ngo perft 5\nquit" | $(TARGET)

# ============================================
#          FILE DEPENDENCIES
# ============================================

# Explicit dependencies (ensures proper rebuilds)
$(OBJ_DIR)/main.o: main.c types.h
$(OBJ_DIR)/bitboard.o: bitboard.c types.h
$(OBJ_DIR)/attacks.o: attacks.c types.h
$(OBJ_DIR)/movegen.o: movegen.c types.h
$(OBJ_DIR)/evaluate.o: evaluate.c types.h
$(OBJ_DIR)/search.o: search.c types.h
$(OBJ_DIR)/book.o: book.c types.h
$(OBJ_DIR)/nnue.o: nnue.c types.h
$(OBJ_DIR)/uci.o: uci.c types.h
