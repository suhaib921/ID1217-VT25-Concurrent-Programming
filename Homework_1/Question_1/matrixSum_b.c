/* matrix summation, min, and max using pthreads (Version b)

   features: Main thread prints results. No barrier function is used.
             No arrays for partial results are used. Global mutex-protected
             variables are used for accumulating results.
             Matrix elements are initialized to random values.

   usage under Linux:
     gcc matrixSum_b.c -lpthread -o matrixSum_b
     ./matrixSum_b <size> <numWorkers>

*/
#ifndef _REENTRANT 
#define _REENTRANT 
#endif 
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h> // For INT_MAX, INT_MIN

#define MAXSIZE 10000  /* maximum matrix size */
#define MAXWORKERS 10   /* maximum number of workers */

pthread_mutex_t result_mutex; /* mutex lock for protecting global results */

/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;
    if( !initialized )
    {
        gettimeofday( &start, NULL );
        initialized = true;
    }
    gettimeofday( &end, NULL );
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

double start_time, end_time; /* start and end times */
int size, numWorkers, stripSize;  /* assume size is multiple of numWorkers */
int matrix[MAXSIZE][MAXSIZE]; /* matrix */

// Global results, protected by result_mutex
long long global_sum = 0;
int global_min = INT_MAX, global_max = INT_MIN;
int global_min_row = -1, global_min_col = -1;
int global_max_row = -1, global_max_col = -1;

void *Worker(void *);

/* Each worker sums the values in one strip of the matrix, and finds local min/max.
   Then updates global results with mutex protection. */
void *Worker(void *arg) {
  long myid = (long) arg;
  int i, j;
  int local_sum_strip = 0;
  int local_min = INT_MAX, local_max = INT_MIN;
  int local_min_row = -1, local_min_col = -1;
  int local_max_row = -1, local_max_col = -1;

  int first_row = myid*stripSize;
  int last_row = (myid == numWorkers - 1) ? (size - 1) : (first_row + stripSize - 1);

  /* sum values in my strip and find local min/max */
  for (i = first_row; i <= last_row; i++) {
    for (j = 0; j < size; j++) {
      local_sum_strip += matrix[i][j];
      if (matrix[i][j] < local_min) {
        local_min = matrix[i][j];
        local_min_row = i;
        local_min_col = j;
      }
      if (matrix[i][j] > local_max) {
        local_max = matrix[i][j];
        local_max_row = i;
        local_max_col = j;
      }
    }
  }

  // Update global results with mutex protection
  pthread_mutex_lock(&result_mutex);
  global_sum += local_sum_strip;
  
  if (local_min < global_min) {
    global_min = local_min;
    global_min_row = local_min_row;
    global_min_col = local_min_col;
  }
  if (local_max > global_max) {
    global_max = local_max;
    global_max_row = local_max_row;
    global_max_col = local_max_col;
  }
  pthread_mutex_unlock(&result_mutex);

  return NULL;
}

/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
  int i, j;
  long l; /* use long in case of a 64-bit system */
  pthread_attr_t attr;
  pthread_t workerid[MAXWORKERS];

  /* set global thread attributes */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  /* initialize mutex */
  pthread_mutex_init(&result_mutex, NULL);

  /* read command line args if any */
  size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
  numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
  if (size > MAXSIZE) size = MAXSIZE;
  if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;
  if (numWorkers == 0) numWorkers = 1; // Ensure at least one worker
  stripSize = size/numWorkers;

  /* initialize the matrix with random values */
  srand(time(NULL));
  for (i = 0; i < size; i++) {
	  for (j = 0; j < size; j++) {
          matrix[i][j] = rand()%100; // Random values between 0 and 99
	  }
  }

  /* do the parallel work: create the workers */
  start_time = read_timer();
  for (l = 0; l < numWorkers; l++)
    pthread_create(&workerid[l], &attr, Worker, (void *) l);
  
  // Main thread waits for all workers to finish
  for (l = 0; l < numWorkers; l++)
    pthread_join(workerid[l], NULL);

  end_time = read_timer(); /* get end time */

  /* print results */
  printf("The total sum is %lld\n", global_sum);
  printf("The minimum element is %d at (%d, %d)\n", global_min, global_min_row, global_min_col);
  printf("The maximum element is %d at (%d, %d)\n", global_max, global_max_row, global_max_col);
  printf("The execution time is %g sec\n", end_time - start_time);

  // Destroy mutex
  pthread_mutex_destroy(&result_mutex);

  return 0; // Main thread exits gracefully
}
