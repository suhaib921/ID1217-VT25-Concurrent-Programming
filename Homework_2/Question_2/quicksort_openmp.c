/**
 * @file quicksort_openmp.c
 * @brief Implements a parallel quicksort algorithm using OpenMP tasks.
 *
 * This program sorts an array of integers using a recursive quicksort
 * algorithm parallelized with OpenMP tasks. The main thread initiates the
 * sort within a parallel region, and the recursive calls are spawned as
 * independent tasks, allowing for parallel exploration of the sort tree.
 *
 * The program measures the execution time of the parallel sort and can be
 * used to evaluate speedup by varying the number of threads.
 *
 * To compile:
 *   gcc -o quicksort_openmp quicksort_openmp.c -fopenmp
 *
 * To run:
 *   export OMP_NUM_THREADS=<number_of_threads>
 *   ./quicksort_openmp <array_size>
 *   Example:
 *   export OMP_NUM_THREADS=4
 *   ./quicksort_openmp 1000000
 *
 * Note: For performance evaluation, the prompt suggests running at least 5 times
 * and using the median value. This code performs a single run.
 */

#include <omp.h> 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Function prototypes
void swap(int* a, int* b);
int partition(int* array, int low, int high);
void quicksort_omp(int* array, int low, int high);

/**
 * @brief Swaps two integer values.
 */
void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * @brief Partitions the array for quicksort.
 */
int partition(int* array, int low, int high) {
    int pivot = array[high];
    int i = (low - 1);

    for (int j = low; j <= high - 1; j++) {
        if (array[j] < pivot) {
            i++;
            swap(&array[i], &array[j]);
        }
    }
    swap(&array[i + 1], &array[high]);
    return (i + 1);
}

/**
 * @brief Recursive quicksort implementation with OpenMP tasks.
 *
 * If the segment `[low, high]` is valid, it's partitioned. Then, two new
 * tasks are created to sort the left and right sub-arrays concurrently.
 *
 * @param array The array to sort.
 * @param low The starting index.
 * @param high The ending index.
 */
void quicksort_omp(int* array, int low, int high) {
    if (low < high) {
        int pi = partition(array, low, high);

        #pragma omp task
        {
            quicksort_omp(array, low, pi - 1);
        }
        
        #pragma omp task
        {
            quicksort_omp(array, pi + 1, high);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <array_size>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "Array size must be positive.\n");
        return 1;
    }
    
    int* array = (int*)malloc(n * sizeof(int));
    if (!array) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 1;
    }

    // Initialize array with random values
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        array[i] = rand() % (n * 10);
    }

    printf("Sorting an array of %d elements using OpenMP tasks.\n", n);
    
    // Set number of threads if OMP_NUM_THREADS is not set
    // omp_set_num_threads(4); 

    double start_time, end_time;

    start_time = omp_get_wtime();

    // The parallel region is created once.
    #pragma omp parallel
    {
        // One thread (the first one to reach this directive) creates the initial tasks.
        #pragma omp single nowait
        {
            quicksort_omp(array, 0, n - 1);
        }
    } // Implicit barrier here ensures all tasks are completed before proceeding.

    end_time = omp_get_wtime();

    printf("Execution time: %f seconds\n", end_time - start_time);

    // Verification
    int sorted = 1;
    for (int i = 0; i < n - 1; i++) {
        if (array[i] > array[i + 1]) {
            sorted = 0;
            fprintf(stderr, "Verification failed at index %d: %d > %d\n", i, array[i], array[i + 1]);
            break;
        }
    }

    if (sorted) {
        printf("Array successfully sorted.\n");
    } else {
        printf("Array sorting failed.\n");
    }

    free(array);
    return 0;
}
