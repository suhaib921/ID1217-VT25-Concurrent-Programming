/* matrix summation, min, and max using pthreads (Version c)

   features: Uses a "bag of tasks" pattern with a shared row counter.
             Workers atomically fetch rows to process. Main thread prints results.
             Global mutex-protected variables are used for accumulating results.
             Matrix elements are initialized to random values.

   usage under Linux:
     gcc matrixSum_c.c -lpthread -o matrixSum_c
     ./matrixSum_c <size> <numWorkers>

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
pthread_mutex_t row_counter_mutex; /* mutex lock for protecting row counter */
int next_row_to_process = 0; /* shared row counter */

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
int size, numWorkers;  
int matrix[MAXSIZE][MAXSIZE]; /* matrix */

// Global results, protected by result_mutex
long long global_sum = 0;
int global_min = INT_MAX, global_max = INT_MIN;
int global_min_row = -1, global_min_col = -1;
int global_max_row = -1, global_max_col = -1;

void *Worker(void *);

/* Each worker fetches rows from the shared counter, processes them,
   and updates global results with mutex protection. */
void *Worker(void *arg) {
  long myid = (long) arg;
  int i, j, row;
  int local_sum_worker = 0; // Sum for this worker across all rows it processes
  int local_min_worker = INT_MAX, local_max_worker = INT_MIN;
  int local_min_row_worker = -1, local_min_col_worker = -1;
  int local_max_row_worker = -1, local_max_col_worker = -1;

  while (true) {
    // Atomically get the next row to process
    pthread_mutex_lock(&row_counter_mutex);
    row = next_row_to_process;
    next_row_to_process++;
    pthread_mutex_unlock(&row_counter_mutex);

    if (row >= size) { // No more rows to process
      break;
    }

    /* Process this row */
    for (j = 0; j < size; j++) {
      local_sum_worker += matrix[row][j];
      if (matrix[row][j] < local_min_worker) {
        local_min_worker = matrix[row][j];
        local_min_row_worker = row;
        local_min_col_worker = j;
      }
      if (matrix[row][j] > local_max_worker) {
        local_max_worker = matrix[row][j];
        local_max_row_worker = row;
        local_max_col_worker = j;
      }
    }
  }

  // Update global results with mutex protection after processing all assigned rows
  pthread_mutex_lock(&result_mutex);
  global_sum += local_sum_worker;
  
  if (local_min_worker < global_min) {
    global_min = local_min_worker;
    global_min_row = local_min_row_worker;
    global_min_col = local_min_col_worker;
  }
  if (local_max_worker > global_max) {
    global_max = local_max_worker;
    global_max_row = local_max_row_worker;
    global_max_col = local_max_col_worker;
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

  /* initialize mutexes */
  pthread_mutex_init(&result_mutex, NULL);
  pthread_mutex_init(&row_counter_mutex, NULL);

  /* read command line args if any */
  size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
  numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
  if (size > MAXSIZE) size = MAXSIZE;
  if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;
  if (numWorkers == 0) numWorkers = 1; // Ensure at least one worker

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
  printf("The total sum is %%lld\n", global_sum);
  printf("The minimum element is %%d at (%%d, %%d)\n", global_min, global_min_row, global_min_col);
  printf("The maximum element is %%d at (%%d, %%d)\n", global_max, global_max_row, global_max_col);
  printf("The execution time is %%g sec\n", end_time - start_time);

  // Destroy mutexes
  pthread_mutex_destroy(&result_mutex);
  pthread_mutex_destroy(&row_counter_mutex);

  return 0; // Main thread exits gracefully
}
