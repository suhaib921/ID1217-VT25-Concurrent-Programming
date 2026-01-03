/**
 * @file quicksort.c
 * @brief Implements a parallel quicksort algorithm using Pthreads.
 *
 * This program sorts an array of integers in parallel using the quicksort
 * algorithm. It demonstrates recursive parallelism by creating new threads
 * for sorting sub-arrays. To avoid creating an excessive number of threads,
 * a new thread is only created if the sub-array size is above a defined
 * threshold.
 *
 * The execution time of the sorting process is measured and printed.
 *
 * To compile:
 *   gcc -o quicksort quicksort.c -lpthread
 *
 * To run:
 *   ./quicksort <array_size>
 *   Example: ./quicksort 1000000
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define THREAD_THRESHOLD 10000 // Minimum array size to justify creating a new thread

// Arguments for the thread function
typedef struct {
    int* array;
    int low;
    int high;
} ThreadArgs;

// Function prototypes
void swap(int* a, int* b);
int partition(int* array, int low, int high);
void* quicksort_thread(void* args);
void quicksort_recursive(int* array, int low, int high);

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
 *
 * This function takes the last element as a pivot, places the pivot element at
 * its correct position in the sorted array, and places all smaller elements
 * to the left of the pivot and all greater elements to the right.
 *
 * @param array The array to be partitioned.
 * @param low The starting index.
 * @param high The ending index.
 * @return The index of the pivot element.
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
 * @brief The main thread function for parallel quicksort.
 *
 * This function is called by each new thread. It partitions the array segment
 * it's responsible for. For the resulting sub-arrays, it decides whether to
 * handle them via a recursive call or by spawning a new thread.
 *
 * @param args A void pointer to a ThreadArgs struct.
 * @return NULL.
 */
void* quicksort_thread(void* args) {
    ThreadArgs* q_args = (ThreadArgs*)args;
    int low = q_args->low;
    int high = q_args->high;
    int* array = q_args->array;
    free(q_args); // Free the arguments structure

    if (low < high) {
        int pi = partition(array, low, high);

        // Decide whether to create a new thread for the right partition
        if ((high - (pi + 1)) > THREAD_THRESHOLD) {
            pthread_t right_thread;
            ThreadArgs* right_args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
            right_args->array = array;
            right_args->low = pi + 1;
            right_args->high = high;

            if (pthread_create(&right_thread, NULL, quicksort_thread, right_args) == 0) {
                // Recursively sort the left part in the current thread
                quicksort_recursive(array, low, pi - 1);
                // Wait for the right-side thread to finish
                pthread_join(right_thread, NULL);
            } else {
                // If thread creation fails, sort both parts sequentially
                fprintf(stderr, "Failed to create thread. Running sequentially.\n");
                quicksort_recursive(array, low, pi - 1);
                quicksort_recursive(array, pi + 1, high);
            }
        } else {
            // If the right partition is small, sort both sequentially
            quicksort_recursive(array, low, pi - 1);
            quicksort_recursive(array, pi + 1, high);
        }
    }
    return NULL;
}


/**
 * @brief A sequential recursive quicksort implementation.
 *
 * This function is used when the array size is below the thread creation
 * threshold, to avoid excessive thread overhead.
 *
 * @param array The array to sort.
 * @param low The starting index.
 * @param high The ending index.
 */
void quicksort_recursive(int* array, int low, int high) {
    if (low < high) {
        int pi = partition(array, low, high);
        quicksort_recursive(array, low, pi - 1);
        quicksort_recursive(array, pi + 1, high);
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
    if (array == NULL) {
        fprintf(stderr, "Failed to allocate memory for the array.\n");
        return 1;
    }

    // Initialize array with random values
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        array[i] = rand() % (n * 10);
    }
    
    printf("Sorting an array of %d elements.\n", n);

    // Timing
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Start the parallel quicksort
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    args->array = array;
    args->low = 0;
    args->high = n - 1;

    quicksort_thread((void*)args);

    gettimeofday(&end, NULL);

    // Calculate and print execution time
    double execution_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Execution time: %f seconds\n", execution_time);

    // Verification: check if the array is sorted
    int sorted = 1;
    for (int i = 0; i < n - 1; i++) {
        if (array[i] > array[i + 1]) {
            sorted = 0;
            fprintf(stderr, "Verification failed at index %d: %d > %d\n", i, array[i], array[i+1]);
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
