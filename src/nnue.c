// ============================================ \\
//       FE64 CHESS ENGINE - NNUE               \\
//    Neural Network Evaluation Support         \\
// ============================================ \\

#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// ============================================ \\
//           NNUE CONSTANTS & STRUCTURES        \\
// ============================================ \\

#define NNUE_INPUT_SIZE 768   // 12 pieces * 64 squares
#define NNUE_HIDDEN1_SIZE 256 // First hidden layer
#define NNUE_HIDDEN2_SIZE 32  // Second hidden layer
#define NNUE_OUTPUT_SIZE 1    // Single evaluation output
#define NNUE_SCALE 400        // Scale factor for final output

// Compile with -DUSE_NNUE to enable
#ifdef USE_NNUE
#define NNUE_ENABLED 1
#else
#define NNUE_ENABLED 0
#endif

// NNUE Weight structure
typedef struct
{
    float input_weights[NNUE_INPUT_SIZE][NNUE_HIDDEN1_SIZE];
    float hidden1_bias[NNUE_HIDDEN1_SIZE];
    float hidden1_weights[NNUE_HIDDEN1_SIZE][NNUE_HIDDEN2_SIZE];
    float hidden2_bias[NNUE_HIDDEN2_SIZE];
    float hidden2_weights[NNUE_HIDDEN2_SIZE];
    float output_bias;
    int loaded;
} NNUEWeights;

NNUEWeights nnue_weights = {0};

// NNUE Accumulator for incremental updates
typedef struct
{
    float hidden1[NNUE_HIDDEN1_SIZE];
    int valid;
} NNUEAccumulator;

NNUEAccumulator nnue_accum[2]; // [side]

// ============================================ \\
//           ACTIVATION FUNCTIONS               \\
// ============================================ \\

// ReLU activation function
static inline float relu(float x)
{
    return x > 0 ? x : 0;
}

// Clipped ReLU (CReLU) - common in NNUE
static inline float crelu(float x)
{
    if (x < 0)
        return 0;
    if (x > 1)
        return 1;
    return x;
}

// ============================================ \\
//           FILE I/O FUNCTIONS                 \\
// ============================================ \\

int load_nnue(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        printf("info string NNUE file not found: %s\n", filename);
        return 0;
    }

    size_t read = 0;
    read += fread(nnue_weights.input_weights, sizeof(float), NNUE_INPUT_SIZE * NNUE_HIDDEN1_SIZE, f);
    read += fread(nnue_weights.hidden1_bias, sizeof(float), NNUE_HIDDEN1_SIZE, f);
    read += fread(nnue_weights.hidden1_weights, sizeof(float), NNUE_HIDDEN1_SIZE * NNUE_HIDDEN2_SIZE, f);
    read += fread(nnue_weights.hidden2_bias, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    read += fread(nnue_weights.hidden2_weights, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    read += fread(&nnue_weights.output_bias, sizeof(float), 1, f);

    fclose(f);

    nnue_weights.loaded = 1;
    printf("info string NNUE loaded successfully (%zu parameters)\n", read);
    return 1;
}

int save_nnue(const char *filename)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        return 0;

    fwrite(nnue_weights.input_weights, sizeof(float), NNUE_INPUT_SIZE * NNUE_HIDDEN1_SIZE, f);
    fwrite(nnue_weights.hidden1_bias, sizeof(float), NNUE_HIDDEN1_SIZE, f);
    fwrite(nnue_weights.hidden1_weights, sizeof(float), NNUE_HIDDEN1_SIZE * NNUE_HIDDEN2_SIZE, f);
    fwrite(nnue_weights.hidden2_bias, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    fwrite(nnue_weights.hidden2_weights, sizeof(float), NNUE_HIDDEN2_SIZE, f);
    fwrite(&nnue_weights.output_bias, sizeof(float), 1, f);

    fclose(f);
    return 1;
}

// ============================================ \\
//           INITIALIZATION                     \\
// ============================================ \\

void init_nnue_random()
{
    srand(42); // Fixed seed for reproducibility

    float scale1 = sqrtf(2.0f / NNUE_INPUT_SIZE);
    float scale2 = sqrtf(2.0f / NNUE_HIDDEN1_SIZE);
    float scale3 = sqrtf(2.0f / NNUE_HIDDEN2_SIZE);

    for (int i = 0; i < NNUE_INPUT_SIZE; i++)
        for (int j = 0; j < NNUE_HIDDEN1_SIZE; j++)
            nnue_weights.input_weights[i][j] = ((float)rand() / RAND_MAX - 0.5f) * scale1;

    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
    {
        nnue_weights.hidden1_bias[i] = 0;
        for (int j = 0; j < NNUE_HIDDEN2_SIZE; j++)
            nnue_weights.hidden1_weights[i][j] = ((float)rand() / RAND_MAX - 0.5f) * scale2;
    }

    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++)
    {
        nnue_weights.hidden2_bias[i] = 0;
        nnue_weights.hidden2_weights[i] = ((float)rand() / RAND_MAX - 0.5f) * scale3;
    }

    nnue_weights.output_bias = 0;
    nnue_weights.loaded = 1;
}

// ============================================ \\
//           NNUE EVALUATION                    \\
// ============================================ \\

// Check if NNUE weights are loaded
int nnue_weights_loaded()
{
    return nnue_weights.loaded;
}

// NNUE Evaluation (OPTIMIZED: only iterate over active pieces)
int evaluate_nnue()
{
    if (!nnue_weights.loaded)
        return 0;

    // Collect active piece indices (max 32 pieces)
    int active_indices[32];
    int num_active = 0;

    for (int piece = P; piece <= k; piece++)
    {
        U64 bb = bitboards[piece];
        while (bb)
        {
            int sq = get_ls1b_index(bb);
            int idx = piece * 64 + sq;
            if (idx < NNUE_INPUT_SIZE && num_active < 32)
                active_indices[num_active++] = idx;
            pop_bit(bb, sq);
        }
    }

    // Forward pass - Layer 1 (OPTIMIZED: only add active weights)
    float hidden1[NNUE_HIDDEN1_SIZE];

    // Start with biases
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
        hidden1[i] = nnue_weights.hidden1_bias[i];

    // Only add weights for active pieces
    for (int a = 0; a < num_active; a++)
    {
        int j = active_indices[a];
        for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
            hidden1[i] += nnue_weights.input_weights[j][i];
    }

    // Apply activation
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
        hidden1[i] = crelu(hidden1[i]);

    // Forward pass - Layer 2
    float hidden2[NNUE_HIDDEN2_SIZE];
    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++)
    {
        hidden2[i] = nnue_weights.hidden2_bias[i];
        for (int j = 0; j < NNUE_HIDDEN1_SIZE; j++)
        {
            hidden2[i] += hidden1[j] * nnue_weights.hidden1_weights[j][i];
        }
        hidden2[i] = crelu(hidden2[i]);
    }

    // Output layer
    float output = nnue_weights.output_bias;
    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++)
    {
        output += hidden2[i] * nnue_weights.hidden2_weights[i];
    }

    // Scale and return
    int score = (int)(output * NNUE_SCALE);
    return (side == white) ? score : -score;
}
