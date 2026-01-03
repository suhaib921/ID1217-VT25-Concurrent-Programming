/**
 * @file 8_queens.c
 * @brief Solves the 8-Queens problem using a parallel backtracking algorithm
 *        with OpenMP.
 *
 * The 8-Queens problem involves placing 8 queens on an 8x8 chessboard such
 * that no two queens threaten each other. This program uses a recursive
 * backtracking approach to find all 92 possible solutions.
 *
 * Parallelism is introduced at the top level of the search tree. The master
 * thread creates a separate OpenMP task for each possible placement of the
 * first queen in the first column. Each task then proceeds to solve its
 * sub-problem sequentially. This strategy effectively divides the search
 * space among the available threads.
 *
 * The total number of solutions is counted using an atomic operation to
 * ensure thread safety.
 *
 * To compile:
 *   gcc -o 8_queens 8_queens.c -fopenmp
 *
 * To run:
 *   export OMP_NUM_THREADS=<number_of_threads>
 *   ./8_queens
 *   Example:
 *   export OMP_NUM_THREADS=4
 *   ./8_queens
 */

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 8

/**
 * @brief Checks if placing a queen at board[row][col] is safe.
 *
 * This function checks if a newly placed queen at `(row, col)` is under
 * attack from any queens placed in previous columns (`0` to `col-1`).
 *
 * @param board The chessboard configuration. board[c] stores the row of the queen in column c.
 * @param row The row of the new queen.
 * @param col The column of the new queen.
 * @return 1 if safe, 0 otherwise.
 */
int is_safe(int board[N], int row, int col) {
    for (int i = 0; i < col; i++) {
        // Check same row or diagonals
        if (board[i] == row || abs(board[i] - row) == abs(i - col)) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Recursive function to solve the N-Queens problem using backtracking.
 *
 * @param board The chessboard configuration array.
 * @param col The current column to place a queen in.
 * @param solution_count A pointer to the shared counter for solutions found.
 */
void solve_queens_recursive(int board[N], int col, int* solution_count) {
    // Base case: If all queens are placed, a solution is found.
    if (col >= N) {
        #pragma omp atomic
        (*solution_count)++;
        
        // Optional: Print the solution
        // #pragma omp critical
        // {
        //     printf("Solution #%d: ", *solution_count);
        //     for (int i = 0; i < N; i++) printf("%d ", board[i]);
        //     printf("\n");
        // }
        return;
    }

    // Try placing a queen in each row of the current column
    for (int i = 0; i < N; i++) {
        if (is_safe(board, i, col)) {
            board[col] = i;
            solve_queens_recursive(board, col + 1, solution_count);
            // No need to "un-place" the queen (board[col] = -1) since
            // the next iteration of this loop will just overwrite it.
        }
    }
}

int main() {
    int solution_count = 0;

    printf("Starting 8-Queens solver...\n");
    
    double start_time = omp_get_wtime();

    #pragma omp parallel
    {
        #pragma omp single
        {
            printf("Using %d threads.\n", omp_get_num_threads());
            // Create tasks for the first column placements
            for (int i = 0; i < N; i++) {
                #pragma omp task firstprivate(i)
                {
                    int board[N];
                    // Initialize board for this task
                    memset(board, -1, N * sizeof(int));
                    board[0] = i;
                    solve_queens_recursive(board, 1, &solution_count);
                }
            }
        }
    } // Implicit barrier here waits for all tasks to complete

    double end_time = omp_get_wtime();

    printf("Found %d solutions.\n", solution_count);
    printf("Execution time: %f seconds\n", end_time - start_time);

    return 0;
}
