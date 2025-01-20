/* matrix summation using pthreads

   features: uses a barrier; the Worker[0] computes
             the total sum from partial sums computed by Workers
             and prints the total sum to the standard output

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


pthread_mutex_t barrier, minMaxLock;  /* mutex lock for the barrier and min/max */
pthread_cond_t go;        /* condition variable for leaving */
int numWorkers;           /* number of workers */ 
int numArrived = 0;       /* number who have arrived */

/* global variables for min and max */
int globalSum = 0;  // Corrected initialization
int globalMin = __INT_MAX__;
int globalMax = -__INT_MAX__;
int globalMinPos[2], globalMaxPos[2];


/* a reusable counter barrier */
void Barrier() {
  pthread_mutex_lock(&barrier);
  numArrived++;
  if (numArrived == numWorkers) {
    numArrived = 0;
    pthread_cond_broadcast(&go);
  } else
    pthread_cond_wait(&go, &barrier);
  pthread_mutex_unlock(&barrier);
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
int sums[MAXWORKERS]; /* partial sums */
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

  /* initialize mutex and condition variable */
  pthread_mutex_init(&barrier, NULL);
  pthread_mutex_init(&minMaxLock, NULL);
  pthread_cond_init(&go, NULL);

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

 for (l = 0; l < numWorkers; l++)
        pthread_join(workerid[l], NULL);

  /* print results */
    printf("The total sum is %d\n", globalSum);
    printf("The minimum value is %d at position [%d][%d]\n", globalMin, globalMinPos[0], globalMinPos[1]);
    printf("The maximum value is %d at position [%d][%d]\n", globalMax, globalMaxPos[0], globalMaxPos[1]);
    printf("The execution time is %g sec\n", read_timer() - start_time);

    return 0;
}

/* Each worker sums the values in one strip of the matrix.
   After a barrier, worker(0) computes and prints the total */
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
    sums[myid] = total;

    /* update global min and max */
    pthread_mutex_lock(&minMaxLock);
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

    Barrier();

    // Special Role of Worker 0
    if (myid == 0) {
        total = 0;
        for (i = 0; i < numWorkers; i++)
        total += sums[i];
        /* get end time */
        end_time = read_timer();
        /* print results */
        globalSum = total;

        printf("The total is %d\n", total);
        printf("The execution time is %g sec\n", end_time - start_time);
    }

    return NULL;  // Added return statement
}
