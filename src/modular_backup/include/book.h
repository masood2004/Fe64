/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file book.h
 * @brief Polyglot opening book support
 * @author Syed Masood
 * @version 4.0.0
 */

#ifndef FE64_BOOK_H
#define FE64_BOOK_H

#include "types.h"
#include "move.h"

// ============================================
//          BOOK STATE
// ============================================

// Opening book entries
extern PolyglotEntry *opening_book;
extern int book_entries;

// Book enabled flag
extern int use_book;

// Multiple books support
#define MAX_BOOKS 8
extern PolyglotEntry *books[MAX_BOOKS];
extern int book_entry_counts[MAX_BOOKS];
extern int num_books;

// ============================================
//          POLYGLOT RANDOM NUMBERS
// ============================================

// Standard Polyglot random numbers for hashing
extern const U64 polyglot_random64[781];

// ============================================
//          BOOK FUNCTIONS
// ============================================

/**
 * @brief Get Polyglot hash key for current position
 * @return Polyglot-format hash key
 */
U64 get_polyglot_key(void);

/**
 * @brief Load opening book from file
 * @param filename Path to Polyglot .bin file
 * @return 1 on success, 0 on failure
 */
int load_opening_book(const char *filename);

/**
 * @brief Load multiple opening books
 * @param filenames Array of paths
 * @param count Number of books to load
 * @return Number of books successfully loaded
 */
int load_multiple_books(const char **filenames, int count);

/**
 * @brief Find book entry for current position
 * @param key Polyglot hash key
 * @return Index of first entry, or -1 if not found
 */
int find_book_entry(U64 key);

/**
 * @brief Convert Polyglot move to internal format
 * @param poly_move Polyglot move encoding
 * @return Internal move format, or 0 if invalid
 */
Move polyglot_to_move(unsigned short poly_move);

/**
 * @brief Get a move from the opening book
 * @return Book move, or 0 if not in book
 */
Move get_book_move(void);

/**
 * @brief Get book move with selection method
 * @param method 0=weighted random, 1=best, 2=random
 * @return Book move, or 0 if not in book
 */
Move get_book_move_method(int method);

/**
 * @brief Free opening book memory
 */
void free_opening_book(void);

/**
 * @brief Check if position is in book
 * @return 1 if in book, 0 otherwise
 */
int is_in_book(void);

/**
 * @brief Get number of book moves for current position
 * @return Number of available book moves
 */
int count_book_moves(void);

/**
 * @brief Get all book moves for current position
 * @param moves Array to store moves
 * @param weights Array to store weights
 * @param max_moves Maximum moves to return
 * @return Number of moves found
 */
int get_all_book_moves(Move *moves, int *weights, int max_moves);

// ============================================
//          BOOK LEARNING
// ============================================

/**
 * @brief Update book statistics after game
 * @param result Game result (1=win, 0=draw, -1=loss)
 * @param move The book move that was played
 */
void update_book_stats(int result, Move move);

#endif // FE64_BOOK_H
