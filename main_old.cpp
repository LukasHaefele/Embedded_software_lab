#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <benchmark/benchmark.h>

#define SEQUENCE_LENGTH 1023
#define REGISTER_SIZE 10
#define MIN_CORRELATION (1023 - 3 * 65)

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

    int* array = NULL;
    int number;
    int count = 0;

    while (fscanf(file, "%d", &number) == 1) {
        int* temp = (int*)realloc(array, (count + 1) * sizeof(int)); 
        if (temp == NULL) {
            perror("Memory allocation error");
            free(array);
            fclose(file);
            return NULL;
        }
        array = temp;
        array[count] = number; 
        count++;
    }

    fclose(file);

    return array; 
}

int* calcSequences(int t1, int t2) {
    int* chipSequence = (int*)malloc(SEQUENCE_LENGTH * sizeof(int));
    if (chipSequence == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    int reg1[REGISTER_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int reg2[REGISTER_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    for (int i = 0; i < SEQUENCE_LENGTH; i++) {
        chipSequence[i] = (reg1[9] ^ reg2[t1 - 1] ^ reg2[t2 - 1]) == 1 ? 1 : -1;

        int feedback1 = reg1[9] ^ reg1[2];
        int feedback2 = reg2[1] ^ reg2[2] ^ reg2[5] ^ reg2[7] ^ reg2[8] ^ reg2[9];

        for (int k = 9; k > 0; k--) {
            reg1[k] = reg1[k - 1];
            reg2[k] = reg2[k - 1];
        }

        reg1[0] = feedback1;
        reg2[0] = feedback2;
    }
    return chipSequence;
}

int cross_correlate(int* signal, int* sequence) {
    for (int i = 0; i<SEQUENCE_LENGTH; i++){
        int sum = 0;
        for (int j = 0; j<SEQUENCE_LENGTH; j++){
            sum += (signal[(i+j) % SEQUENCE_LENGTH] * sequence[j]);
        }

        if (abs(sum) > MIN_CORRELATION){
            return i * (sum / abs(sum));
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
        int* sequence = calcSequences(2, 6);
        if (sequence != NULL) {
            free(sequence);
        }
    }
}
BENCHMARK(BM_CalcSequences);
// Benchmark for cross_correlate
static void BM_CrossCorrelate(benchmark::State& state) {
    int* signal = (int*)malloc(SEQUENCE_LENGTH * sizeof(int));
    int* sequence = calcSequences(2, 6);
    if (signal == NULL || sequence == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        state.SkipWithError("Memory allocation failed");
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

// Full code benchmark
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
        int* chipSequences[24];
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
        // Original main logic
        // Benchmarking variables

        for (int t = 0; t < 1; t++) {
            if (argc < 2) {
                fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
                return 1;
            }
            int* signal = readNumbersFromFile(argv[1]);
            
            int tValues[24][2] = {
                {2, 6}, {3, 7}, {4, 8}, {5, 9}, {1, 9}, {2, 10}, {1, 8},
                {2, 9}, {3, 10}, {2, 3}, {3, 4}, {5, 6}, {6, 7}, {7, 8},
                {8, 9}, {9, 10}, {1, 4}, {2, 5}, {3, 6}, {4, 7}, {5, 8},
                {6, 9}, {1, 3}, {4, 6}
            };
            
            int* chipSequences[24];
            for (int i = 0; i < 24; i++) {
                chipSequences[i] = calcSequences(tValues[i][0], tValues[i][1]);
                // print chipsequence for debugging
                // if (i == 0) {
                //     printf("chipSequence[%d]: ", i);
                //     for (int j = 0; j < SEQUENCE_LENGTH; j++)
                //         printf("%d ", chipSequences[i][j]);
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
                // if (i == 0) {
                //     // Print the first chip sequence for debugging
                //     printf("chipSequence[%d]: ", i);
                //     for (int j = 0; j < SEQUENCE_LENGTH; j++) {
                //         printf("%d ", chipSequences[i][j]);
                //     }
                //     printf("\n");
                // }
            }

            for (int i = 0; i < 24; i++) {
                int delta = cross_correlate(signal, chipSequences[i]);
                if (delta < 10000) {
                    int bit = (delta > 0) ? 1 : 0;
                    printf("Satellite %d: %d  |  %d\n", i + 1, bit, delta);
                }
            }

            for (int i = 0; i < 24; i++) {
                free(chipSequences[i]);
            }
            free(signal);

        }

    } else {
        // Run benchmarks
        benchmark::Initialize(&argc, argv);
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
    }
    return 0;
}

