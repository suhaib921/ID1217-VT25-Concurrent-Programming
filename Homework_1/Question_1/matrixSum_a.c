/* matrix summation, min, and max using pthreads (Version a)

   features: uses a barrier; the Worker[0] computes
             the total sum, min, and max from partial sums, mins, and maxs
             computed by Workers and prints the total sum, min, and max to the standard output.
             Matrix elements are initialized to random values.

   usage under Linux:
     gcc matrixSum_a.c -lpthread -o matrixSum_a
     ./matrixSum_a <size> <numWorkers>

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

pthread_mutex_t barrier_mutex;  /* mutex lock for the barrier */
pthread_cond_t go;        /* condition variable for leaving */
int numWorkers;           /* number of workers */ 
int numArrived = 0;       /* number who have arrived */

/* a reusable counter barrier */
void Barrier() {
  pthread_mutex_lock(&barrier_mutex);
  numArrived++;
  if (numArrived == numWorkers) {
    numArrived = 0;
    pthread_cond_broadcast(&go);
  } else
    pthread_cond_wait(&go, &barrier_mutex);
  pthread_mutex_unlock(&barrier_mutex);
}

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
int size, stripSize;  /* assume size is multiple of numWorkers */
int matrix[MAXSIZE][MAXSIZE]; /* matrix */

// Partial results from each worker
int sums[MAXWORKERS];
int mins[MAXWORKERS];
int maxs[MAXWORKERS];
int minRows[MAXWORKERS], minCols[MAXWORKERS];
int maxRows[MAXWORKERS], maxCols[MAXWORKERS];

void *Worker(void *);

/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
  int i, j;
  long l; /* use long in case of a 64-bit system */
  pthread_attr_t attr;
  pthread_t workerid[MAXWORKERS];

  /* set global thread attributes */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  /* initialize mutex and condition variable */
  pthread_mutex_init(&barrier_mutex, NULL);
  pthread_cond_init(&go, NULL);

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

  // Destroy mutex and condition variable
  pthread_mutex_destroy(&barrier_mutex);
  pthread_cond_destroy(&go);

  return 0; // Main thread exits gracefully
}

/* Each worker sums the values in one strip of the matrix, and finds local min/max.
   After a barrier, worker(0) computes and prints the total sum, global min, and global max. */
void *Worker(void *arg) {
  long myid = (long) arg;
  int total_sum_strip, i, j;
  int local_min = INT_MAX, local_max = INT_MIN;
  int local_min_row = -1, local_min_col = -1;
  int local_max_row = -1, local_max_col = -1;

  int first_row = myid*stripSize;
  int last_row = (myid == numWorkers - 1) ? (size - 1) : (first_row + stripSize - 1);

  /* sum values in my strip and find local min/max */
  total_sum_strip = 0;
  for (i = first_row; i <= last_row; i++) {
    for (j = 0; j < size; j++) {
      total_sum_strip += matrix[i][j];
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
  sums[myid] = total_sum_strip;
  mins[myid] = local_min;
  maxs[myid] = local_max;
  minRows[myid] = local_min_row;
  minCols[myid] = local_min_col;
  maxRows[myid] = local_max_row;
  maxCols[myid] = local_max_col;

  Barrier();

  if (myid == 0) {
    int global_total = 0;
    int global_min = INT_MAX, global_max = INT_MIN;
    int global_min_row = -1, global_min_col = -1;
    int global_max_row = -1, global_max_col = -1;

    for (i = 0; i < numWorkers; i++) {
      global_total += sums[i];
      if (mins[i] < global_min) {
        global_min = mins[i];
        global_min_row = minRows[i];
        global_min_col = minCols[i];
      }
      if (maxs[i] > global_max) {
        global_max = maxs[i];
        global_max_row = maxRows[i];
        global_max_col = maxCols[i];
      }
    }
    
    end_time = read_timer(); /* get end time */

    /* print results */
    printf("The total sum is %d\n", global_total);
    printf("The minimum element is %d at (%d, %d)\n", global_min, global_min_row, global_min_col);
    printf("The maximum element is %d at (%d, %d)\n", global_max, global_max_row, global_max_col);
    printf("The execution time is %g sec\n", end_time - start_time);
  }

  return NULL;
}
