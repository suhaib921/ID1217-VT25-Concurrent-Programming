#include <stdio.h>      
#include <stdlib.h>     
#include <limits.h>     
#include <omp.h>       

#define MAXWORKERS 2    
#define ITERATIONS 10   

void initializeMatrix(int **matrix, int size) {
    #pragma omp parallel
    {
        unsigned int seed = omp_get_thread_num() + 42; // Unique seed per thread
        #pragma omp for
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                matrix[i][j] = rand_r(&seed) % 99 + 1;  // Random values between 1-99
            }
        }
    }
}

/* Function to allocate a 2D matrix dynamically */
int **allocateMatrix(int size) {
    int **matrix = (int **)malloc(size * sizeof(int *));
    for (int i = 0; i < size; i++) {
        matrix[i] = (int *)malloc(size * sizeof(int));
    }
    return matrix;
}

/* Function to free allocated matrix */
void freeMatrix(int **matrix, int size) {
    for (int i = 0; i < size; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

int main() {
    int matrix_sizes[] = {200, 400, 800, 1600, 3200, 6400};  // Matrix sizes to test
    int numWorkers = MAXWORKERS;
    omp_set_num_threads(numWorkers);

    printf("Matrix Size | Avg Time (ms) | Min Value | Max Value | Total Sum\n");
    printf("--------------------------------------------------------------\n");

    for (int test = 0; test < sizeof(matrix_sizes)/sizeof(matrix_sizes[0]); test++) {
        int size = matrix_sizes[test];

        int **matrix = allocateMatrix(size);
        double total_time = 0.0;
        int global_min, global_max, total;

        /* Perform 10 iterations for each matrix size */
        for (int iter = 0; iter < ITERATIONS; iter++) {
            initializeMatrix(matrix, size);

            /* Start measuring execution time */
            double start_time = omp_get_wtime();

            global_min = INT_MAX;
            global_max = INT_MIN;
            total = 0;

            #pragma omp parallel
            {
                int local_total = 0;
                int local_min = INT_MAX, local_max = INT_MIN;
                int thread_min_row, thread_min_col;
                int thread_max_row, thread_max_col;

                #pragma omp for reduction(+:total)
                for (int i = 0; i < size; i++) {
                    for (int j = 0; j < size; j++) {
                        int value = matrix[i][j];
                        local_total += value;

                        if (value < local_min) {
                            local_min = value;
                            thread_min_row = i;
                            thread_min_col = j;
                        }

                        if (value > local_max) {
                            local_max = value;
                            thread_max_row = i;
                            thread_max_col = j;
                        }
                    }
                }

                #pragma omp atomic
                total += local_total;

                if (local_min < global_min) {
                    #pragma omp critical
                    {
                        if (local_min < global_min) {
                            global_min = local_min;
                        }
                    }
                }

                if (local_max > global_max) {
                    #pragma omp critical
                    {
                        if (local_max > global_max) {
                            global_max = local_max;
                        }
                    }
                }
            }

            /* End measuring execution time */
            double end_time = omp_get_wtime();
            total_time += (end_time - start_time) * 1000;  // Convert to milliseconds
        }

        /* Compute average execution time */
        double avg_time = total_time / ITERATIONS;

        printf("%10d | %12.3f | %9d | %9d | %10d\n", size, avg_time, global_min, global_max, total);

        freeMatrix(matrix, size);
    }

    return 0;
}