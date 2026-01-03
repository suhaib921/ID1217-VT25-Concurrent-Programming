# Question 4: The 8-Queens Problem - Analysis

## Summary of Task
Develop a parallel program using OpenMP to generate all 92 solutions to the 8-queens problem. The problem requires placing 8 queens on an 8×8 chessboard such that no queen can attack another (no two queens share the same row, column, or diagonal).

**Requirements:**
- Use OpenMP (suggested: tasks for recursive generation)
- Find all 92 valid solutions
- Benchmark on different thread counts (up to at least 4)
- Report speedup using median of 5+ runs
- Use `omp_get_wtime()` for timing
- Measure only the parallel part

**Hint:** Use tasks so the master thread recursively generates queen placements and other tasks check whether placements are acceptable.

## Analysis of Current Implementation

### Implementation Status: NOT IMPLEMENTED

The Question_4 directory contains only a README.md file. **No code implementation exists.**

## Required Implementation Approach

### Algorithm Overview: N-Queens with Backtracking

**Classic Backtracking Algorithm:**
1. Place queens column by column (or row by row)
2. For each column, try placing queen in each row
3. Check if placement is safe (no conflicts)
4. If safe, recursively place queens in remaining columns
5. If successful placement of all queens, record solution
6. Backtrack and try next position

**Constraints:**
- No two queens in same row
- No two queens in same column
- No two queens on same diagonal (two types: / and \)

### Parallelization Challenges

**Why N-Queens is Difficult to Parallelize:**

1. **Irregular Tree Structure:**
   - Backtracking creates irregular computation tree
   - Early pruning leads to varying subtree sizes
   - Load imbalance is significant

2. **Fine-Grained Recursion:**
   - Very small computation per recursive call
   - Task creation overhead can dominate
   - Need careful task cutoff strategy

3. **Shared State:**
   - Board configuration needs to be copied for each task
   - Cannot share mutable board state between tasks
   - Memory overhead for task proliferation

4. **Search Space:**
   - Total search space is large but heavily pruned
   - Most branches lead to dead ends quickly
   - Parallelism opportunity mainly at top levels of tree

### Recommended Implementation Structure

```c
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 8
#define MAX_SOLUTIONS 100

// Global solution counter (needs synchronization)
int solution_count = 0;

// Store solutions
int solutions[MAX_SOLUTIONS][N];

// Check if placing queen at (row, col) is safe
int is_safe(int board[], int row, int col) {
    // Check row conflict (shouldn't happen with our approach)
    // We place one queen per column, so column conflicts impossible

    // Check row conflicts with previously placed queens
    for (int prev_col = 0; prev_col < col; prev_col++) {
        int prev_row = board[prev_col];

        // Same row?
        if (prev_row == row)
            return 0;

        // Same diagonal?
        // Diagonal: |row - prev_row| == |col - prev_col|
        if (abs(row - prev_row) == abs(col - prev_col))
            return 0;
    }

    return 1;
}

// Sequential backtracking solver
void solve_sequential(int board[], int col, int *count) {
    if (col == N) {
        // Found a solution
        memcpy(solutions[*count], board, N * sizeof(int));
        (*count)++;
        return;
    }

    // Try placing queen in each row of current column
    for (int row = 0; row < N; row++) {
        if (is_safe(board, row, col)) {
            board[col] = row;
            solve_sequential(board, col + 1, count);
            // Backtrack (implicit - next iteration overwrites)
        }
    }
}

// Parallel solver using tasks
void solve_parallel(int board[], int col, int depth, int max_depth) {
    if (col == N) {
        // Found a solution
        #pragma omp critical
        {
            memcpy(solutions[solution_count], board, N * sizeof(int));
            solution_count++;
        }
        return;
    }

    // Create tasks only up to certain depth to avoid overhead
    if (depth < max_depth) {
        // Try placing queen in each row of current column
        for (int row = 0; row < N; row++) {
            if (is_safe(board, row, col)) {
                // Create a copy of the board for the task
                #pragma omp task firstprivate(board, row, col, depth)
                {
                    int local_board[N];
                    memcpy(local_board, board, N * sizeof(int));
                    local_board[col] = row;
                    solve_parallel(local_board, col + 1, depth + 1, max_depth);
                }
            }
        }
        #pragma omp taskwait
    }
    else {
        // Beyond max depth, solve sequentially
        int local_count = 0;
        solve_sequential(board, col, &local_count);

        // Add to global count
        #pragma omp atomic
        solution_count += local_count;
    }
}

int main(int argc, char *argv[]) {
    int num_threads = (argc > 1) ? atoi(argv[1]) : 4;
    int max_task_depth = (argc > 2) ? atoi(argv[2]) : 3;

    omp_set_num_threads(num_threads);

    printf("Solving %d-Queens with %d threads (task depth: %d)\n",
           N, num_threads, max_task_depth);

    int board[N];
    memset(board, 0, N * sizeof(int));

    double start = omp_get_wtime();

    #pragma omp parallel
    {
        #pragma omp single
        {
            solve_parallel(board, 0, 0, max_task_depth);
        }
    }

    double end = omp_get_wtime();

    printf("Found %d solutions in %g seconds\n", solution_count, end - start);

    // Verify: should be 92 solutions for 8-queens
    if (solution_count == 92) {
        printf("Correct! Found all 92 solutions.\n");
    } else {
        printf("ERROR: Expected 92 solutions, found %d\n", solution_count);
    }

    return 0;
}
```

## Correctness Considerations

### Critical Synchronization Issues

1. **Solution Counting Race Condition:**

   **INCORRECT (Data Race):**
   ```c
   if (col == N) {
       solutions[solution_count] = board;  // RACE on solution_count
       solution_count++;                    // RACE on increment
   }
   ```

   **CORRECT Approach:**
   ```c
   #pragma omp critical
   {
       memcpy(solutions[solution_count], board, N * sizeof(int));
       solution_count++;
   }
   ```
   OR:
   ```c
   #pragma omp atomic capture
   local_idx = solution_count++;
   memcpy(solutions[local_idx], board, N * sizeof(int));
   ```

2. **Board State Sharing:**

   **INCORRECT (Shared Mutable State):**
   ```c
   #pragma omp task shared(board)  // WRONG!
   {
       board[col] = row;  // Multiple tasks modify same board
       solve_parallel(board, col + 1);
   }
   ```

   **CORRECT Approach:**
   ```c
   #pragma omp task firstprivate(board, row, col)
   {
       int local_board[N];
       memcpy(local_board, board, N * sizeof(int));
       local_board[col] = row;
       solve_parallel(local_board, col + 1);
   }
   ```

3. **Task Dependencies:**
   - Must wait for all child tasks before returning: `#pragma omp taskwait`
   - Without taskwait, parent may return before children complete
   - Leads to incorrect solution count

### Common Implementation Pitfalls

1. **Excessive Task Creation:**
   - Creating tasks at every recursion level
   - Overhead >> computation for deep recursion
   - **Solution**: Task cutoff depth (e.g., only first 3 levels)

2. **Missing firstprivate:**
   - Using `private(board)` leaves board uninitialized
   - Must use `firstprivate(board)` to copy current state

3. **Critical Section Overhead:**
   - Recording every solution in critical section is acceptable (only 92 times)
   - Recording every failed placement would be disastrous

4. **Stack Overflow:**
   - Deep recursion with large board copies on stack
   - For N=8, not an issue; for larger N, consider heap allocation

### Verification and Testing

**Correctness Checks:**

1. **Solution Count:**
   - N=1: 1 solution
   - N=4: 2 solutions
   - N=5: 10 solutions
   - N=8: 92 solutions
   - N=10: 724 solutions

2. **Solution Validity:**
   - Each solution has exactly N queens
   - No two queens in same row/column/diagonal
   - Verify all 92 solutions are distinct

3. **Determinism:**
   - Run multiple times with same parameters
   - Should always find exactly 92 solutions
   - Solutions may be found in different order (acceptable)

## Performance Considerations

### Expected Performance Characteristics

**Parallelization Potential:**

The 8-queens problem has **limited parallelism** due to:
1. Small problem size (only 92 solutions to find)
2. Heavy pruning (most branches terminate quickly)
3. Irregular tree (load imbalance)
4. High task overhead relative to computation

**Realistic Speedup Expectations:**

For N=8 (8-Queens):
- **1 thread**: Baseline (very fast, ~0.001-0.01 seconds)
- **2 threads**: 1.3-1.5x speedup
- **4 threads**: 1.5-2.2x speedup
- **8 threads**: 1.6-2.5x speedup

**Why Poor Speedup?**

1. **Amdahl's Law:**
   - First queen placement: only 8 choices (limited parallelism)
   - Second queen: 7-8 choices per first queen
   - Task creation overhead significant

2. **Load Imbalance:**
   - Some subtrees (e.g., placing queen at corner) get pruned faster
   - Other subtrees (e.g., placing queen at center) have more valid placements
   - Work distribution highly uneven

3. **Task Granularity:**
   - Each task does very little work (few recursive calls before pruning)
   - Task creation/scheduling overhead dominates
   - Even with cutoff depth, still overhead-heavy

4. **Problem Size:**
   - Absolute runtime is tiny (~milliseconds for N=8)
   - Hard to measure accurately
   - Overhead percentage is high

**Better Performance for Larger N:**

For N=12 or N=14:
- More solutions (14200 and 365596 respectively)
- Deeper recursion trees
- More parallelism opportunity
- Better speedup possible (2-3x on 4 cores)

### Optimization Strategies

1. **Task Cutoff Depth:**
   - Only create tasks for first K levels (typically 2-4)
   - After cutoff, switch to sequential backtracking
   - **Tuning**: Experiment with depths 1, 2, 3, 4
   - Optimal depends on N and number of threads

2. **Pruning Optimizations:**
   - Better pruning → less work → but also less parallelism
   - Symmetry breaking: only try half of first row (others are mirrors)
   - This reduces solutions from 92 to 12 unique (rest are symmetries)

3. **Work Distribution:**
   - **Static distribution** at first level:
     ```c
     #pragma omp parallel for
     for (int row = 0; row < N; row++) {
         int board[N] = {row};
         solve_sequential(board, 1, &local_count);
     }
     ```
   - Better load balancing than tasks for this problem
   - Less overhead than deep task recursion

4. **Reduce Synchronization:**
   - Use thread-local solution arrays
   - Merge at end instead of critical section per solution
   - Significant improvement if finding many solutions (large N)

### Alternative Parallel Strategies

**Approach 1: Parallel For at Top Level**

```c
#pragma omp parallel for reduction(+:solution_count)
for (int row = 0; row < N; row++) {
    int board[N];
    board[0] = row;
    int local_count = 0;
    solve_sequential(board, 1, &local_count);
    solution_count += local_count;
}
```

**Advantages:**
- Simple implementation
- Low overhead (only N tasks)
- Good load balancing with dynamic scheduling

**Disadvantages:**
- Only parallelizes first level (limited parallelism for small N)
- For N=8, only 8 parallel units

**Approach 2: Task Pool at Two Levels**

```c
#pragma omp parallel
#pragma omp single
{
    for (int row1 = 0; row1 < N; row1++) {
        for (int row2 = 0; row2 < N; row2++) {
            #pragma omp task
            {
                int board[N] = {row1, row2};
                if (is_safe(board, row2, 1)) {
                    solve_sequential(board, 2, &local_count);
                }
            }
        }
    }
}
```

**Advantages:**
- More parallel units (N² tasks)
- Better load balancing

**Disadvantages:**
- Many tasks may be pruned immediately (wasted overhead)
- More complex logic

### Performance Analysis by Task Depth

**Depth 0 (No Tasks):**
- Sequential execution
- Baseline for speedup calculation

**Depth 1 (Tasks at first level):**
- 8 tasks created
- Good for 8+ threads, underutilized for fewer
- Low overhead

**Depth 2 (Tasks at first two levels):**
- Up to 8×7 = 56 tasks (many pruned)
- Better load balancing
- Moderate overhead

**Depth 3 (Tasks at first three levels):**
- Up to 8×7×6 = 336 tasks (many pruned)
- Excellent load balancing
- Higher overhead
- Best for 4-8 threads

**Depth 4+:**
- Excessive task creation
- Overhead dominates
- Performance degrades

**Recommendation:** Depth 2-3 for N=8, depending on thread count

## Synchronization and Concurrency Analysis

### Thread Safety Assessment

**Thread-Safe Components:**
1. `is_safe()` function (pure function, no shared state)
2. Board arrays (each task has its own copy with firstprivate)
3. Local variables in recursive calls

**Requires Synchronization:**
1. `solution_count` increment (critical section or atomic)
2. `solutions` array writes (critical section)

**No Synchronization Needed:**
- Most of the computation (checking safety, recursion)
- Only synchronize when recording solutions

### Data Race Analysis

**Potential Races:**

1. **Solution counter:**
   ```c
   solution_count++;  // RACE if multiple threads
   ```
   **Fix:** Critical section or atomic operation

2. **Solution array:**
   ```c
   solutions[solution_count] = board;  // RACE on index
   ```
   **Fix:** Critical section around both operations

3. **Board sharing:**
   - Already handled by firstprivate + local copy

**Correct Implementation:**
- Critical section encompasses both solution storage and counter increment
- Ensures atomic update of both data structures
- Small overhead (only 92 executions for N=8)

## Benchmarking Methodology

### Test Configuration

**Thread Counts:**
- 1 thread (baseline)
- 2 threads
- 4 threads
- 8 threads (if available)

**Task Depth Variations:**
- Depth 1
- Depth 2
- Depth 3
- Depth 4

**Timing Considerations:**

1. **Warm-up Run:**
   - First run may be slower (cache cold, page faults)
   - Run once before timing measurements

2. **Multiple Runs:**
   - At least 5 runs per configuration
   - Report median (more robust than mean)
   - Small problem → high variance possible

3. **Timing Precision:**
   - For N=8, runtime may be ~1ms
   - Need high-resolution timer (omp_get_wtime is sufficient)
   - Consider repeating algorithm many times for longer measurement

4. **Verification:**
   - Always verify 92 solutions found
   - Check a few solutions for validity

### Expected Results Table

```
Task Depth = 2

Threads | Time (ms) | Speedup | Efficiency
--------|-----------|---------|------------
   1    |   2.50    |  1.00   |   100%
   2    |   1.80    |  1.39   |    69%
   4    |   1.30    |  1.92   |    48%
   8    |   1.15    |  2.17   |    27%
```

**Observations:**
- Limited speedup due to problem size
- Diminishing returns with more threads
- Efficiency drops significantly (Amdahl's law + overhead)

## Recommendations

### Implementation Strategy

1. **Start with Sequential Version:**
   - Implement and verify correct backtracking
   - Ensure exactly 92 solutions found
   - This becomes baseline and fallback

2. **Add Basic Task Parallelism:**
   - Simple task creation at first level
   - Verify solution count remains 92
   - Measure speedup

3. **Optimize Task Depth:**
   - Experiment with depth 2, 3, 4
   - Find optimal for your system
   - Document findings

4. **Compare with Parallel For:**
   - Implement alternative approach
   - Compare performance
   - May be simpler and faster for N=8

5. **Benchmark Systematically:**
   - Test all configurations
   - Document results in table/graph
   - Explain performance characteristics

### Code Quality

1. **Correctness Verification:**
   - Always check solution count = 92
   - Optionally verify uniqueness and validity
   - Print sample solutions for manual verification

2. **Compilation:**
   ```bash
   gcc -O3 -fopenmp -o queens queens.c -lm
   ```

3. **Error Handling:**
   - Validate command-line arguments
   - Handle edge cases (N < 4 has limited solutions)

4. **Documentation:**
   - Comment task creation strategy
   - Explain cutoff depth choice
   - Document expected performance

### Extensions and Variations

1. **Larger N:**
   - Test N=10 (724 solutions), N=12 (14200 solutions)
   - Better parallelization opportunity
   - More meaningful performance measurements

2. **Symmetry Breaking:**
   - Exploit board symmetries to reduce search space
   - Find 12 unique solutions, generate 92 via symmetry
   - Less parallel work but faster overall

3. **Iterative Deepening:**
   - Instead of backtracking, use iterative approach
   - May enable better parallelization patterns

4. **Alternative Task Distribution:**
   - Work-stealing queue
   - Explicit task pool with atomic task assignment

## Conclusion

**Implementation Status:** NOT IMPLEMENTED - requires complete development.

**Algorithm Suitability:** The 8-queens problem has **limited parallelization potential**:
- Small problem size (only 92 solutions)
- Irregular computation tree (load imbalance)
- High task overhead relative to computation
- Better suited for demonstrating OpenMP tasks than achieving high speedup

**Expected Performance:**
- Modest speedup: 1.5-2.5x on 4 cores
- Efficiency: 40-60%
- Highly dependent on task depth tuning
- Better results for larger N (10, 12, 14)

**Learning Value:**
- Excellent for learning OpenMP task-based parallelism
- Demonstrates challenges of irregular parallel algorithms
- Good case study for task cutoff strategies
- Highlights importance of granularity control

**Critical Success Factors:**
1. Correct task data sharing (firstprivate for board state)
2. Proper synchronization for solution counting
3. Optimal task cutoff depth
4. Verification of solution count
5. Understanding that limited speedup is expected and normal

**Alternative Recommendation:**
For N=8 specifically, a simple `#pragma omp parallel for` at the first level may actually outperform deep task recursion due to lower overhead. Both approaches should be implemented and compared.
