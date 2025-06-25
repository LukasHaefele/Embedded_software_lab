#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <benchmark/benchmark.h>

#define SEQUENCE_LENGTH 1023
#define REGISTER_SIZE 10
#define MIN_CORRELATION (1023 - 3 * 65)

// Function prototypes for benchmarking
static void BM_ReadNumbersFromFile(benchmark::State& state);
static void BM_CalcSequences(benchmark::State& state);
static void BM_CrossCorrelate(benchmark::State& state);
static void BM_full_code(benchmark::State& state);

int* readNumbersFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    int* array = (int*)malloc(SEQUENCE_LENGTH * sizeof(int));
    if (array == NULL) {
        perror("Memory allocation error");
        fclose(file);
        return NULL;
    }
    int number, count = 0;
    int capacity = SEQUENCE_LENGTH;

    while (fscanf(file, "%d", &number) == 1) {
        if (count >= capacity) {
            capacity *= 2; 
            int* temp = (int*)realloc(array, capacity * sizeof(int));
            if (temp == NULL) {
                perror("Memory allocation error");
                free(array);
                fclose(file);
                return NULL;
            }
       
            array = temp;
        }
        array[count++] = number; 
    }

    fclose(file);

    return array; 
}

unsigned int* calcSequences(int t1, int t2) {
    int numInts = (SEQUENCE_LENGTH + 31) / 32; 
    unsigned int* chipSequence = (unsigned int*) calloc(numInts, sizeof(unsigned int));
    if (chipSequence == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    unsigned int reg1 = 0x3FF; // 10 bits set to 1 (binary: 1111111111)
    unsigned int reg2 = 0x3FF; // 10 bits set to 1 (binary: 1111111111)

    for (int i = 0; i < SEQUENCE_LENGTH; i++) {
        int bit1 = (reg1 >> 9) & 1;
        int bit2_t1 = (reg2 >> (t1 - 1)) & 1;
        int bit2_t2 = (reg2 >> (t2 - 1)) & 1;
        int chipBit = (bit1 ^ bit2_t1 ^ bit2_t2) ? 1 : 0;

        if (!chipBit) {
            // Set the bit inverted as sign bit
            chipSequence[i / 32] |= (1U << (i % 32));
        }

        int feedback1 = ((reg1 >> 9) & 1) ^ ((reg1 >> 2) & 1);
        int feedback2 = ((reg2 >> 1) & 1) ^ ((reg2 >> 2) & 1) ^ ((reg2 >> 5) & 1) ^
                        ((reg2 >> 7) & 1) ^ ((reg2 >> 8) & 1) ^ ((reg2 >> 9) & 1);

        reg1 = ((reg1 << 1) | feedback1) & 0x3FF;
        reg2 = ((reg2 << 1) | feedback2) & 0x3FF;
    }

    return chipSequence;
}

// Add this helper to expand the packed sequence to +1/-1
void unpack_sequence(const unsigned int* sequence, int* unpacked) {
    for (int i = 0; i < SEQUENCE_LENGTH; ++i) {
        int word = sequence[i >> 5];
        int bit = (word >> (i & 31)) & 1;
        unpacked[i] = 1 - 2 * bit;
    }
}

// Optimized cross_correlate using precomputed sequence bits
int cross_correlate(int* signal, unsigned int* sequence) {
    static int unpacked_seq[SEQUENCE_LENGTH];
    unpack_sequence(sequence, unpacked_seq);

    int best_shift = 10000;
    int best_sum = 0;

    for (int shift = 0; shift < SEQUENCE_LENGTH; ++shift) {
        int sum = 0;
        int i = 0;
        int idx = shift;
        // Unroll by 4 for a small boost
        for (; idx <= SEQUENCE_LENGTH - 4; idx += 4) {
            int idx_mod = idx % SEQUENCE_LENGTH;
            sum += signal[idx_mod]     * unpacked_seq[i];
            sum += signal[idx_mod + 1] * unpacked_seq[i + 1];
            sum += signal[idx_mod + 2] * unpacked_seq[i + 2];
            sum += signal[idx_mod + 3] * unpacked_seq[i + 3];
            i += 4;
        }
        for (; i < SEQUENCE_LENGTH - 4; i += 4) {
            sum += signal[(i + shift) % SEQUENCE_LENGTH]     * unpacked_seq[i];
            sum += signal[(i + shift + 1) % SEQUENCE_LENGTH] * unpacked_seq[i + 1];
            sum += signal[(i + shift + 2) % SEQUENCE_LENGTH] * unpacked_seq[i + 2];
            sum += signal[(i + shift + 3) % SEQUENCE_LENGTH] * unpacked_seq[i + 3];
        }

        for (; i < SEQUENCE_LENGTH; ++i) {
            sum += signal[i] * unpacked_seq[(i + shift) % SEQUENCE_LENGTH];
        }
        if (abs(sum) > MIN_CORRELATION) {
            return (sum < 0) ? -shift : shift;
        }
    }
    return 10000;
}

// Benchmark for readNumbersFromFile
static void BM_ReadNumbersFromFile(benchmark::State& state) {
    for (auto _ : state) {
        int* signal = readNumbersFromFile("gps_sequence_12.txt");
        if (signal != NULL) {
            free(signal);
        }
    }
}
BENCHMARK(BM_ReadNumbersFromFile);

// Benchmark for calcSequences
static void BM_CalcSequences(benchmark::State& state) {
    for (auto _ : state) {
        unsigned int* sequence = calcSequences(2, 6);
        if (sequence != NULL) {
            free(sequence);
        }
    }
}
BENCHMARK(BM_CalcSequences);

// Benchmark for cross_correlate
static void BM_CrossCorrelate(benchmark::State& state) {
    int* signal = (int*)malloc(SEQUENCE_LENGTH * sizeof(int));
    unsigned int* sequence = calcSequences(2, 6);
    if (signal == NULL || sequence == NULL) {
        state.SkipWithError("Memory allocation failed");
        return;
    }

    for (int i = 0; i < SEQUENCE_LENGTH; i++) {
        signal[i] = (i % 2 == 0) ? 1 : -1; // Example signal
    }

    for (auto _ : state) {
        cross_correlate(signal, sequence);
    }

    free(signal);
    free(sequence);
}
BENCHMARK(BM_CrossCorrelate);

void BM_full_code(benchmark::State& state) {
    for (auto _ : state) {
        int* signal = readNumbersFromFile("gps_sequence_12.txt");
        if (signal == NULL) {
            state.SkipWithError("Failed to read signal from file");
            return;
        }

        int tValues[24][2] = {
            {2, 6}, {3, 7}, {4, 8}, {5, 9}, {1, 9}, {2, 10}, {1, 8},
            {2, 9}, {3, 10}, {2, 3}, {3, 4}, {5, 6}, {6, 7}, {7, 8},
            {8, 9}, {9, 10}, {1, 4}, {2, 5}, {3, 6}, {4, 7}, {5, 8},
            {6, 9}, {1, 3}, {4, 6}
        };

        unsigned int* chipSequences[24];
        for (int i = 0; i < 24; i++) {
            chipSequences[i] = calcSequences(tValues[i][0], tValues[i][1]);
            if (chipSequences[i] == NULL) {
                free(signal);
                state.SkipWithError("Failed to calculate chip sequence for satellite");
                return;
            }
        }

        for (int i = 0; i < 24; i++) {
            int delta = cross_correlate(signal, chipSequences[i]);
            if (delta < 10000) {
                int bit = (delta > 0) ? 1 : 0;
                //printf("Satellite %d: %d | %d\n", i + 1, bit, delta);
            }
            free(chipSequences[i]);
        }

        free(signal);
    }
    return;
}
BENCHMARK(BM_full_code);

int main(int argc, char* argv[]) {
    if (argc > 1) {

        for (int t = 0; t < 1; t++) {
            // Read the signal from the file
            int* signal = readNumbersFromFile(argv[1]);

            if (signal == NULL) {
                fprintf(stderr, "Failed to read signal from file.\n");
                return 1;
            }

            // Define t-values for the satellites
            int tValues[24][2] = {
                {2, 6}, {3, 7}, {4, 8}, {5, 9}, {1, 9}, {2, 10}, {1, 8},
                {2, 9}, {3, 10}, {2, 3}, {3, 4}, {5, 6}, {6, 7}, {7, 8},
                {8, 9}, {9, 10}, {1, 4}, {2, 5}, {3, 6}, {4, 7}, {5, 8},
                {6, 9}, {1, 3}, {4, 6}
            };

            // Calculate chip sequences for each satellite
            unsigned int* chipSequences[24];
            for (int i = 0; i < 24; i++) {
                chipSequences[i] = calcSequences(tValues[i][0], tValues[i][1]);
                // print chipsequence for debugging
                // if (i == 0) {
                //     printf("chipSequence[%d]: ", i);
                //     for (int j = 0; j < SEQUENCE_LENGTH; j++)
                //         printf("%d ", (chipSequences[i][j / 32] >> (j % 32)) & 1);
                //     printf("\n");
                // }
                if (chipSequences[i] == NULL) {
                    fprintf(stderr, "Failed to calculate chip sequence for satellite %d\n", i + 1);
                    for (int j = 0; j < i; j++) {
                        free(chipSequences[j]);
                    }
                    free(signal);
                    return 1;
                }
            }

            // Perform cross-correlation
            for (int i = 0; i < 24; i++) {
                // printf("Satellite %d: ", i + 1);
                int delta = cross_correlate(signal, chipSequences[i]);
                if (delta < 10000) {
                    int bit = (delta > 0) ? 1 : 0;
                    printf("Satellite %d: %d  |  %d\n", i + 1, bit, delta);
                }
                free(chipSequences[i]);
            }

            free(signal);
        }

        // Calculate and display timing statistics
    } else {
        // Run benchmarks
        benchmark::Initialize(&argc, argv);
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
    }
    return 0;
}