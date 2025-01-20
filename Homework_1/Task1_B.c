/* matrix summation using pthreads

   features: the main thread computes and prints the final results
             without using a barrier or arrays for partial results

   usage under Linux:
     gcc matrixSum.c -lpthread
     a.out size numWorkers

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
#define MAXSIZE 10  /* maximum matrix size */
#define MAXWORKERS 10   /* maximum number of workers */


pthread_mutex_t minMaxLock;  /* mutex lock for the min/max and global sum */
int numWorkers;           /* number of workers */ 

/* global variables for sum, min, and max */
int globalSum = 0;
int globalMin = __INT_MAX__;
int globalMax = -__INT_MAX__;
int globalMinPos[2], globalMaxPos[2];


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

  /* initialize mutex */
  pthread_mutex_init(&minMaxLock, NULL);

  /* read command line args if any */
  size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
  numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
  if (size > MAXSIZE) size = MAXSIZE;
  if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;
  stripSize = size/numWorkers;

  /* initialize the matrix with random values */
  srand(time(NULL));
  for (i = 0; i < size; i++) {
	  for (j = 0; j < size; j++) {
          matrix[i][j] = rand() % 100; /*random values 0-99*/
	  }
  }

  /* print the matrix */
    for (i = 0; i < size; i++) {
        printf("[ ");
        for (j = 0; j < size; j++) {
            printf(" %d", matrix[i][j]);
        }
        printf(" ]\n");
    }

  /* do the parallel work: create the workers */
  start_time = read_timer();
  for (l = 0; l < numWorkers; l++)
    pthread_create(&workerid[l], &attr, Worker, (void *) l);

  /* wait for all workers to finish */
  for (l = 0; l < numWorkers; l++)
    pthread_join(workerid[l], NULL);

  /* print results */
  printf("The total sum is %d\n", globalSum);
  printf("The minimum value is %d at position [%d][%d]\n", globalMin, globalMinPos[0], globalMinPos[1]);
  printf("The maximum value is %d at position [%d][%d]\n", globalMax, globalMaxPos[0], globalMaxPos[1]);
  printf("The execution time is %g sec\n", read_timer() - start_time);

  return 0;
}

/* Each worker sums the values in one strip of the matrix and updates global variables */
void *Worker(void *arg) {
    long myid = (long) arg;
    int total = 0, i, j, first, last, localMin, localMax, localMinPos[2], localMaxPos[2];

    printf("worker %ld (pthread id %lu) has started\n", myid, pthread_self());    

   

    /* determine first and last rows of my strip */
    first = myid*stripSize;
    last = (myid == numWorkers - 1) ? (size - 1) : (first + stripSize - 1);

    /* initialize local min and max */
    localMin = __INT_MAX__;
    localMax = -__INT_MAX__;

    /* process the strip */
    for (i = first; i <= last; i++) {
        for (j = 0; j < size; j++) {
            total += matrix[i][j];
            if (matrix[i][j] < localMin) {
                localMin = matrix[i][j];
                localMinPos[0] = i;
                localMinPos[1] = j;
            }
            if (matrix[i][j] > localMax) {
                localMax = matrix[i][j];
                localMaxPos[0] = i;
                localMaxPos[1] = j;
            }
        }
    }

    /* update global sum, min, and max */
    pthread_mutex_lock(&minMaxLock);
    globalSum += total;
    if (localMin < globalMin) {
        globalMin = localMin;
        globalMinPos[0] = localMinPos[0];
        globalMinPos[1] = localMinPos[1];
    }
    if (localMax > globalMax) {
        globalMax = localMax;
        globalMaxPos[0] = localMaxPos[0];
        globalMaxPos[1] = localMaxPos[1];
    }
    pthread_mutex_unlock(&minMaxLock);

    return NULL;
}