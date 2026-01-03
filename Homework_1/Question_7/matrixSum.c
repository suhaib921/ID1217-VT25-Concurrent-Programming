#ifndef _REENTRANT 
#define _REENTRANT 
#endif 
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#define MAX_BOARD_SIZE 15  // Max N for N-queens we reasonably want to solve
#define DEFAULT_BOARD_SIZE 8 // For 8-queens problem
#define MAX_WORKERS 10    // Maximum number of workers

// Shared variables for synchronization and results
pthread_mutex_t solutions_mutex; // Mutex lock for totalSolutions
int totalSolutions = 0;          // Total number of solutions found

int N = DEFAULT_BOARD_SIZE; // Board size (N for N-queens)
int numWorkers;             // Number of worker threads

// Worker function declaration
void *Worker(void *arg);

// Timer function from original matrixSum.c
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

double start_time, end_time; // Start and end times

// Function to check if a queen can be placed at (row, col)
bool is_safe(int board[], int row, int col) {
    for (int i = 0; i < row; i++) {
        // Check if same column
        // Check if same diagonal (left to right or right to left)
        if (board[i] == col || abs(board[i] - col) == abs(i - row)) {
            return false;
        }
    }
    return true;
}

// Recursive function to solve N-Queens problem
void solve_n_queens(int board[], int row) {
    if (row == N) { // All queens placed, found a solution
        pthread_mutex_lock(&solutions_mutex);
        totalSolutions++;
        pthread_mutex_unlock(&solutions_mutex);
        return;
    }

    for (int col = 0; col < N; col++) {
        if (is_safe(board, row, col)) {
            board[row] = col;
            solve_n_queens(board, row + 1);
        }
    }
}

/* Each worker starts the N-Queens search for a specific initial column */
void *Worker(void *arg) {
    long initial_col = (long)arg; // Worker's assigned starting column for the first queen

    // Create a local board for this worker's search path
    int board[MAX_BOARD_SIZE]; 
    // Initialize board (important for `is_safe` logic to not read garbage values)
    for(int i = 0; i < N; ++i) board[i] = -1; 

    // Place the first queen in this worker's assigned column in the first row
    board[0] = initial_col;

    // Start the recursive search from the second row (row index 1)
    solve_n_queens(board, 1);

    pthread_exit(NULL);
}

/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
    long l; // Use long for thread arguments

    // Thread attributes
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    // Initialize mutex for shared solution count
    pthread_mutex_init(&solutions_mutex, NULL);

    // Read command line args
    if (argc > 1) N = atoi(argv[1]); // Board size (N)
    if (N <= 0 || N > MAX_BOARD_SIZE) {
        printf("Board size (N) must be between 1 and %d. Using default N = %d.\n", MAX_BOARD_SIZE, DEFAULT_BOARD_SIZE);
        N = DEFAULT_BOARD_SIZE;
    }
    
    numWorkers = (argc > 2) ? atoi(argv[2]) : N; // Number of workers, default to N
    if (numWorkers <= 0 || numWorkers > MAX_WORKERS) {
        printf("Number of workers must be between 1 and %d. Using default numWorkers = %d.\n", MAX_WORKERS, N);
        numWorkers = N;
    }
    // For N-Queens, we assign initial columns. If numWorkers > N, some will be idle.
    if (numWorkers > N) {
        numWorkers = N; // Limit workers to N as there are only N starting positions for the first queen.
    }

    printf("Solving %d-Queens problem with %d worker threads.\n", N, numWorkers);

    pthread_t workerid[MAX_WORKERS]; // Array to hold worker thread IDs

    start_time = read_timer(); // Start timer

    // Create worker threads
    for (l = 0; l < numWorkers; l++) {
        // Each worker takes an initial column for the first queen, starting from 0 up to numWorkers-1
        // We ensure numWorkers <= N, so each worker gets a valid starting column.
        pthread_create(&workerid[l], &attr, Worker, (void *)l); 
    }

    // Wait for all worker threads to finish
    for (l = 0; l < numWorkers; l++) {
        pthread_join(workerid[l], NULL);
    }

    end_time = read_timer(); // End timer

    printf("Total solutions for %d-Queens: %d\n", N, totalSolutions);
    printf("Execution time: %g seconds\n", end_time - start_time);

    pthread_mutex_destroy(&solutions_mutex); // Clean up mutex
    pthread_attr_destroy(&attr); // Clean up attributes
    pthread_exit(NULL); // Terminate main thread (correct for pthreads)
}    
