// ============================================ \\

// system headers
#include <stdio.h>

// defining bitboard data type
#define U64 unsigned long long 


// ============================================ \\

// set/get/pop bit macros
#define get_bit(bitboard, square) (bitboard & (1ULL << square))


// print board

void print_bitboard(U64 bitboard){
    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over board files
        for (int file = 0; file < 8; file++)
        {
            // Print Ranks
            if(!file){
                printf("  %d ", (8 - rank));
            }

            int square = rank * 8 + file;
            // print bit state (Either 1 or 0)
            printf(" %d", get_bit(bitboard, square) ? 1 : 0);
        }
        // print new line every rank
        printf("\n");
        }
    // Print Files
    printf("\n     a b c d e f g h \n\n");
}

        
        
// ============================================ \\












int main(){
    U64 bitbaord = 3ULL;
    print_bitboard(bitbaord);
    return 0;
}