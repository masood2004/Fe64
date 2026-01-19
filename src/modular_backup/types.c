/**
 * ============================================
 *            FE64 CHESS ENGINE
 *   "The Boa Constrictor" - Slow Death Style
 * ============================================
 *
 * @file types.c
 * @brief Global constants and type implementations
 * @author Syed Masood
 * @version 4.0.0
 */

#include "include/types.h"

// ============================================
//          FILE/RANK MASKS
// ============================================

// Masks to prevent piece wrapping around board edges
const U64 not_a_file = 0xFEFEFEFEFEFEFEFEULL;
const U64 not_h_file = 0x7F7F7F7F7F7F7F7FULL;
const U64 not_ab_file = 0xFCFCFCFCFCFCFCFCULL;
const U64 not_gh_file = 0x3F3F3F3F3F3F3F3FULL;

// File masks (A-H files)
const U64 file_masks[8] = {
    0x0101010101010101ULL, // A file
    0x0202020202020202ULL, // B file
    0x0404040404040404ULL, // C file
    0x0808080808080808ULL, // D file
    0x1010101010101010ULL, // E file
    0x2020202020202020ULL, // F file
    0x4040404040404040ULL, // G file
    0x8080808080808080ULL  // H file
};

// Rank masks (1-8 ranks)
const U64 rank_masks[8] = {
    0xFF00000000000000ULL, // 8th rank
    0x00FF000000000000ULL, // 7th rank
    0x0000FF0000000000ULL, // 6th rank
    0x000000FF00000000ULL, // 5th rank
    0x00000000FF000000ULL, // 4th rank
    0x0000000000FF0000ULL, // 3rd rank
    0x000000000000FF00ULL, // 2nd rank
    0x00000000000000FFULL  // 1st rank
};

// ============================================
//          ASCII PIECE CHARACTERS
// ============================================

const char ascii_pieces[12] = "PNBRQKpnbrqk";

const char *piece_names[12] = {
    "White Pawn", "White Knight", "White Bishop",
    "White Rook", "White Queen", "White King",
    "Black Pawn", "Black Knight", "Black Bishop",
    "Black Rook", "Black Queen", "Black King"};

// Square to coordinate string (for debugging)
const char *square_to_coord[65] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "-" // no_sq
};
