/* matrix summation using pthreads

   features: uses a "bag of tasks" approach;
             computes the total sum, minimum, and maximum elements of the matrix
             with their positions

   usage under Linux:
     gcc matrixSum.c -lpthread
     ./a.out size numWorkers

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
#include <limits.h>
#define MAXSIZE 10000  /* maximum matrix size */
#define MAXWORKERS 10   /* maximum number of workers */

pthread_mutex_t barrier;  /* mutex lock for the barrier */
pthread_cond_t go;        /* condition variable for leaving */
int numWorkers;           /* number of workers */ 
int numArrived = 0;       /* number who have arrived */

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

/* Global variables for aggregation */
long global_sum = 0;
int global_min = INT_MAX;
int global_min_row = -1;
int global_min_col = -1;
int global_max = INT_MIN;
int global_max_row = -1;
int global_max_col = -1;

pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t min_max_mutex = PTHREAD_MUTEX_INITIALIZER;
int current_row = 0; // Shared row counter
pthread_mutex_t row_mutex = PTHREAD_MUTEX_INITIALIZER;

/* a reusable counter barrier - removed in sub-task 1b and 1c */
/* Removed the Barrier function */

/* worker thread function */
void *Worker(void *arg) {
    long myid = (long) arg;
    int i, j;
    int row;
    
#ifdef DEBUG
    printf("worker %ld (pthread id %lu) has started\n", myid, pthread_self());
#endif

    /* Initialize partial min and max */
    int partial_min = INT_MAX;
    int partial_max = INT_MIN;
    int min_row = -1, min_col = -1;
    int max_row = -1, max_col = -1;

    /* Initialize partial sum */
    long partial_sum = 0;

    while (1) {
        /* Fetch the next row to process */
        pthread_mutex_lock(&row_mutex);
        row = current_row;
        current_row++;
        pthread_mutex_unlock(&row_mutex);

        /* Check if all rows have been processed */
        if (row >= size)
            break;

        /* Process the fetched row */
        for (j = 0; j < size; j++) {
            int val = matrix[row][j];
            partial_sum += val;

            if (val < partial_min) {
                partial_min = val;
                min_row = row;
                min_col = j;
            }

            if (val > partial_max) {
                partial_max = val;
                max_row = row;
                max_col = j;
            }
        }
    }

    /* Update global sum */
    pthread_mutex_lock(&sum_mutex);
    global_sum += partial_sum;
    pthread_mutex_unlock(&sum_mutex);

    /* Update global min and max */
    pthread_mutex_lock(&min_max_mutex);
    if (partial_min < global_min) {
        global_min = partial_min;
        global_min_row = min_row;
        global_min_col = min_col;
    }
    if (partial_max > global_max) {
        global_max = partial_max;
        global_max_row = max_row;
        global_max_col = j;
    }
    pthread_mutex_unlock(&min_max_mutex);

    pthread_exit(NULL);
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

    /* initialize mutexes and condition variable */
    pthread_mutex_init(&barrier, NULL);
    pthread_cond_init(&go, NULL);

    /* initialize sum and min/max mutexes */
    pthread_mutex_init(&sum_mutex, NULL);
    pthread_mutex_init(&min_max_mutex, NULL);
    pthread_mutex_init(&row_mutex, NULL); // Initialize row mutex

    /* read command line args if any */
    size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
    numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
    if (size > MAXSIZE) size = MAXSIZE;
    if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;
    stripSize = size / numWorkers; // Not used in bag of tasks

    /* initialize the matrix with random values */
    srand(time(NULL));
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            matrix[i][j] = rand() % 100; // Random values between 0 and 99
        }
    }

    /* print the matrix */
#ifdef DEBUG
    for (i = 0; i < size; i++) {
        printf("[ ");
        for (j = 0; j < size; j++) {
            printf(" %d", matrix[i][j]);
        }
        printf(" ]\n");
    }
#endif

    /* do the parallel work: create the workers */
    start_time = read_timer();
    for (l = 0; l < numWorkers; l++)
        pthread_create(&workerid[l], &attr, Worker, (void *) l);

    /* wait for all worker threads to complete */
    for (l = 0; l < numWorkers; l++)
        pthread_join(workerid[l], NULL);

    /* get end time */
    end_time = read_timer();

    /* print results in the main thread */
    printf("The total sum is %ld\n", global_sum);
    printf("The minimum element is %d at position (%d, %d)\n", global_min, global_min_row, global_min_col);
    printf("The maximum element is %d at position (%d, %d)\n", global_max, global_max_row, global_max_col);
    printf("The execution time is %g sec\n", end_time - start_time);

    /* destroy mutexes and condition variables */
    pthread_mutex_destroy(&barrier);
    pthread_mutex_destroy(&sum_mutex);
    pthread_mutex_destroy(&min_max_mutex);
    pthread_mutex_destroy(&row_mutex);
    pthread_cond_destroy(&go);

    return 0;
}
