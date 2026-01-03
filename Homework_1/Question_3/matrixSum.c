/* pi computation using pthreads */
#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <math.h> // For sqrt

#define MAXWORKERS 10 // maximum number of workers

// Global variables for timing and worker management
double start_time, end_time;     // start and end times
int numWorkers;                  // number of workers
long long total_num_steps;       // total number of integration steps
double partial_sums[MAXWORKERS]; // partial sums of areas

// Function to integrate: f(x) = sqrt(1 - x*x) for a unit circle
double f(double x) {
    return sqrt(1.0 - x * x);
}

/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;
    if (!initialized) {
        gettimeofday(&start, NULL);
        initialized = true;
    }
    gettimeofday(&end, NULL);
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

// Worker function prototype
void *Worker(void *);

int main(int argc, char *argv[]) {
    long l; // use long in case of a 64-bit system
    pthread_attr_t attr;
    pthread_t workerid[MAXWORKERS];

    /* set global thread attributes */
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    /* read command line args */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <num_steps> <numWorkers>\n", argv[0]);
        exit(1);
    }

    total_num_steps = atoll(argv[1]); // Using atoll for long long
    numWorkers = atoi(argv[2]);

    if (numWorkers <= 0 || numWorkers > MAXWORKERS) {
        fprintf(stderr, "Number of workers must be between 1 and %d (MAXWORKERS).\n", MAXWORKERS);
        exit(1);
    }
    if (total_num_steps <= 0) {
        fprintf(stderr, "Number of steps must be positive.\n");
        exit(1);
    }
    
    // Initialize partial_sums to zero
    for (int i = 0; i < numWorkers; ++i) {
        partial_sums[i] = 0.0;
    }

    printf("Computing Pi with %lld steps and %d workers...\n", total_num_steps, numWorkers);

    /* do the parallel work: create the workers */
    start_time = read_timer(); // Start timer after initialization and before thread creation

    for (l = 0; l < numWorkers; l++) {
        pthread_create(&workerid[l], &attr, Worker, (void *)l);
    }

    // Wait for all workers to finish
    for (l = 0; l < numWorkers; l++) {
        pthread_join(workerid[l], NULL);
    }

    // Main thread collects results and calculates final Pi
    double final_area_quadrant = 0.0;
    for (int i = 0; i < numWorkers; ++i) {
        final_area_quadrant += partial_sums[i];
    }
    double pi_estimate = final_area_quadrant * 4.0; // Multiply by 4 for full circle area

    end_time = read_timer(); // End timer after all workers complete and main collected results

    printf("Estimated Pi = %.10lf\n", pi_estimate);
    printf("Execution time = %g sec\n", end_time - start_time);

    pthread_attr_destroy(&attr); // Destroy attributes

    return 0; // Exit main thread cleanly
}

/* Each worker computes a part of the integral for pi. */
void *Worker(void *arg) {
    long myid = (long)arg;
    double my_local_sum = 0.0; // Sum of f(x) values for this worker's strip
    long long steps_per_worker = total_num_steps / numWorkers;
    long long my_start_step = myid * steps_per_worker;
    long long my_end_step;

    if (myid == numWorkers - 1) {
        my_end_step = total_num_steps; // Last worker takes any remaining steps
    } else {
        my_end_step = my_start_step + steps_per_worker;
    }

    double dx = 1.0 / total_num_steps;

    // Use midpoint rule for integration
    for (long long i = my_start_step; i < my_end_step; ++i) {
        double x = (i + 0.5) * dx; // Midpoint of the interval
        my_local_sum += f(x);
    }

    partial_sums[myid] = my_local_sum * dx; // Store partial area (sum of f(x) * dx)

    return NULL;
}