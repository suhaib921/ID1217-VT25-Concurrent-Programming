/* matrix summation using OpenMP

   usage with gcc (version 4.2 or higher required):
     gcc -O -fopenmp -o matrixSum-openmp matrixSum-openmp.c 
     ./matrixSum-openmp size numWorkers

*/

/* matrix summation, min, and max using OpenMP

   usage with gcc (version 4.2 or higher required):
     gcc -O -fopenmp -o matrixSum-openmp matrixSum-openmp.c 
     ./matrixSum-openmp size numWorkers

*/

#include <omp.h>
#include <stdio.h>
#include <stdlib.h> // For atoi, rand, srand
#include <time.h>   // For time
#include <limits.h> // For INT_MAX, INT_MIN

#define MAXSIZE 10000  /* maximum matrix size */
#define MAXWORKERS 8   /* maximum number of workers */

double start_time, end_time;

int numWorkers;
int size; 
int matrix[MAXSIZE][MAXSIZE];

// Global variables for min/max and their positions, updated by critical sections
int global_min_val = INT_MAX;
int global_max_val = INT_MIN;
int global_min_row = -1, global_min_col = -1;
int global_max_row = -1, global_max_col = -1;


/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
  int i, j;
  long long total_sum = 0; // Use long long for sum to prevent overflow on large matrices

  /* read command line args if any */
  size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
  numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
  if (size > MAXSIZE) size = MAXSIZE;
  if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;

  // Ensure numWorkers is at least 1
  if (numWorkers <= 0) numWorkers = 1;

  omp_set_num_threads(numWorkers);

  /* initialize the matrix with random values */
  srand(time(NULL)); // Seed random number generator once for varied results
  for (i = 0; i < size; i++) {
	  for (j = 0; j < size; j++) {
      matrix[i][j] = rand()%100; // Random values between 0 and 99
	  }
  }

  start_time = omp_get_wtime();

  // OpenMP parallel region to compute sum, min, and max
  #pragma omp parallel reduction(+:total_sum)
  {
    // Thread-private variables for min/max and their positions
    int thread_local_min_val = INT_MAX;
    int thread_local_max_val = INT_MIN;
    int thread_local_min_row = -1, thread_local_min_col = -1;
    int thread_local_max_row = -1, thread_local_max_col = -1;

    // Use OpenMP for-loop. 'private(j)' is implicitly handled if 'j' is declared
    // within the loop, but explicitly stating it is safer for older standards or complex cases.
    #pragma omp for private(j)
    for (i = 0; i < size; i++) {
      for (j = 0; j < size; j++) {
        int current_val = matrix[i][j];
        total_sum += current_val; // Sum reduction is handled by OpenMP

        // Update thread-local min/max
        if (current_val < thread_local_min_val) {
          thread_local_min_val = current_val;
          thread_local_min_row = i;
          thread_local_min_col = j;
        }
        if (current_val > thread_local_max_val) {
          thread_local_max_val = current_val;
          thread_local_max_row = i;
          thread_local_max_col = j;
        }
      }
    }

    // Combine thread-local min/max results into global min/max using a critical section.
    // This ensures only one thread updates the global variables at a time, preventing race conditions.
    #pragma omp critical
    {
      if (thread_local_min_val < global_min_val) {
        global_min_val = thread_local_min_val;
        global_min_row = thread_local_min_row;
        global_min_col = thread_local_min_col;
      }
      if (thread_local_max_val > global_max_val) {
        global_max_val = thread_local_max_val;
        global_max_row = thread_local_max_row;
        global_max_col = thread_local_max_col;
      }
    }
  } // Implicit barrier here ensures all threads complete before proceeding

  end_time = omp_get_wtime();

  printf("The total sum is %lld\n", total_sum);
  printf("The minimum element is %d at (%d, %d)\n", global_min_val, global_min_row, global_min_col);
  printf("The maximum element is %d at (%d, %d)\n", global_max_val, global_max_row, global_max_col);
  printf("It took %g seconds\n", end_time - start_time);

  return 0;
}

