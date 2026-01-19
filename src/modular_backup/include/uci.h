/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file uci.h
 * @brief UCI protocol implementation
 * @author Syed Masood
 * @version 4.0.0
 */

#ifndef FE64_UCI_H
#define FE64_UCI_H

#include "types.h"
#include "move.h"

// ============================================
//          UCI OPTIONS
// ============================================

// Hash table size in MB
extern int hash_size_mb;

// Multi-PV support
extern int multi_pv;

// Pondering enabled
extern int ponder_enabled;

// ============================================
//          UCI FUNCTIONS
// ============================================

/**
 * @brief Main UCI protocol loop
 */
void uci_loop(void);

/**
 * @brief Parse UCI position command
 * @param command The position command string
 */
void parse_position(char *command);

/**
 * @brief Parse UCI go command
 * @param command The go command string
 * @param info Search info structure
 */
void parse_go(char *command, SearchInfo *info);

/**
 * @brief Send UCI info string
 * @param format Printf-style format string
 */
void send_info(const char *format, ...);

/**
 * @brief Send search info during search
 * @param depth Current depth
 * @param score Score (centipawns or mate)
 * @param nodes Nodes searched
 * @param time Time elapsed (ms)
 * @param pv Principal variation moves
 * @param pv_length Length of PV
 */
void send_search_info(int depth, int score, long long nodes,
                      long long time, Move *pv, int pv_length);

/**
 * @brief Send best move result
 * @param best_move The best move found
 * @param ponder_move The ponder move (if any)
 */
void send_bestmove(Move best_move, Move ponder_move);

/**
 * @brief Handle setoption command
 * @param command The setoption command string
 */
void handle_setoption(char *command);

/**
 * @brief Print UCI identification
 */
void print_uci_id(void);

/**
 * @brief Print available UCI options
 */
void print_uci_options(void);

#endif // FE64_UCI_H
