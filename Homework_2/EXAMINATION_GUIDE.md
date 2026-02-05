# Homework 2 - OpenMP Examination Guide

This guide contains exam questions on core OpenMP concepts from Homework 2, with answers, code snippets, and test commands.

---

## Question 1: Matrix Sum with Min/Max - Basic OpenMP Usage

### Q1.1: Explain the purpose of the `reduction(+:total_sum)` clause in the parallel region.

**Answer:**
The `reduction(+:total_sum)` clause tells OpenMP to:
1. Create a private copy of `total_sum` for each thread
2. Initialize each private copy to 0 (the identity value for addition)
3. Allow each thread to accumulate its partial sum independently
4. At the end of the parallel region, combine all partial sums using the `+` operator into the original `total_sum` variable

This prevents race conditions when multiple threads try to update the same variable.

**Code Snippet (matrixSum-openmp.c:66):**
```c
#pragma omp parallel reduction(+:total_sum)
{
    // Each thread has its own copy of total_sum
    // All copies are automatically summed at the end
}
```

**Test Command:**
```bash
cd Question_1
gcc -O -fopenmp -o matrixSum-openmp matrixSum-openmp.c
./matrixSum-openmp 1000 4
```

---

### Q1.2: Why are thread-local variables used for min/max instead of using reduction clauses?

**Answer:**
OpenMP's reduction clause doesn't support custom reductions that track both the value AND its position (row, col). Therefore, the code uses:
1. **Thread-local variables** to track each thread's local min/max and their positions
2. **Critical section** to safely merge thread-local results into global variables

This pattern is necessary when you need to track additional metadata (like positions) along with the reduced value.

**Code Snippet (matrixSum-openmp.c:69-94):**
```c
// Thread-private variables for min/max and their positions
int thread_local_min_val = INT_MAX;
int thread_local_max_val = INT_MIN;
int thread_local_min_row = -1, thread_local_min_col = -1;
int thread_local_max_row = -1, thread_local_max_col = -1;

#pragma omp for private(j)
for (i = 0; i < size; i++) {
    for (j = 0; j < size; j++) {
        int current_val = matrix[i][j];
        total_sum += current_val;

        // Update thread-local min/max
        if (current_val < thread_local_min_val) {
            thread_local_min_val = current_val;
            thread_local_min_row = i;
            thread_local_min_col = j;
        }
        if (current_val > thread_local_max_val) {
            thread_local_max_val = current_val;
            thread_local_max_row = i;
            thread_local_max_col = j;
        }
    }
}
```

---

### Q1.3: Explain the purpose of the critical section at the end of the parallel region.

**Answer:**
The `#pragma omp critical` ensures that only one thread at a time can execute the code block that updates global min/max values. This prevents race conditions where multiple threads might simultaneously read and write the global variables, which could lead to:
- Lost updates
- Inconsistent state (e.g., min_val from one thread but min_row from another)

**Code Snippet (matrixSum-openmp.c:98-110):**
```c
#pragma omp critical
{
    if (thread_local_min_val < global_min_val) {
        global_min_val = thread_local_min_val;
        global_min_row = thread_local_min_row;
        global_min_col = thread_local_min_col;
    }
    if (thread_local_max_val > global_max_val) {
        global_max_val = thread_local_max_val;
        global_max_row = thread_local_max_row;
        global_max_col = thread_local_max_col;
    }
}
```

---

### Q1.4: What is the difference between `omp_set_num_threads()` and the `OMP_NUM_THREADS` environment variable?

**Answer:**
- **`omp_set_num_threads(n)`**: Programmatically sets the number of threads at runtime from within the code. It overrides the environment variable.
- **`OMP_NUM_THREADS`**: Environment variable set before running the program. Provides default thread count if not overridden by code.

Priority: `omp_set_num_threads()` > `OMP_NUM_THREADS` > system default

**Code Snippet (matrixSum-openmp.c:53):**
```c
omp_set_num_threads(numWorkers);
```

**Test Commands:**
```bash
# Using environment variable
export OMP_NUM_THREADS=2
./matrixSum-openmp 1000

# Or specify as argument (code uses omp_set_num_threads internally)
./matrixSum-openmp 1000 4
```

---

## Question 2: Quicksort - OpenMP Tasks and Recursive Parallelism

### Q2.1: Why use OpenMP tasks instead of parallel for loops for quicksort?

**Answer:**
Quicksort has **irregular parallelism** - the amount of work in each recursive call varies greatly depending on where the pivot splits the array. OpenMP tasks are better suited because:
1. **Dynamic work creation**: Tasks can be created recursively at runtime
2. **Load balancing**: The OpenMP runtime can dynamically assign tasks to threads
3. **Nested parallelism**: Each recursive call can spawn new tasks
4. **No predetermined iteration count**: Unlike for loops, we don't know how many recursive calls will be needed

**Code Snippet (quicksort_openmp.c:73-87):**
```c
void quicksort_omp(int* array, int low, int high) {
    if (low < high) {
        int pi = partition(array, low, high);

        // Create two tasks for left and right subarrays
        #pragma omp task
        {
            quicksort_omp(array, low, pi - 1);
        }

        #pragma omp task
        {
            quicksort_omp(array, pi + 1, high);
        }
    }
}
```

**Test Command:**
```bash
cd Question_2
gcc -o quicksort_openmp quicksort_openmp.c -fopenmp
export OMP_NUM_THREADS=4
./quicksort_openmp 100000
```

---

### Q2.2: Explain the purpose of `#pragma omp single nowait` in the quicksort implementation.

**Answer:**
- **`single`**: Ensures only one thread executes the enclosed code block (creates the initial task)
- **`nowait`**: Removes the implicit barrier at the end of the single construct

Without `nowait`, all threads would wait at the end of the single block before starting task execution. With `nowait`, other threads can immediately start picking up tasks from the task queue.

**Code Snippet (quicksort_openmp.c:123-130):**
```c
#pragma omp parallel
{
    // Only one thread creates the initial task
    #pragma omp single nowait
    {
        quicksort_omp(array, 0, n - 1);
    }
} // Implicit barrier here ensures all tasks complete
```

---

### Q2.3: What happens at the implicit barrier at the end of the parallel region?

**Answer:**
The implicit barrier at the end of the parallel region (line 130) ensures that:
1. All spawned tasks have completed execution
2. All threads have finished their work
3. The array is fully sorted before the program continues

Without this barrier, the verification code would start before sorting is complete, leading to incorrect results.

**Test Command to verify sorting:**
```bash
./quicksort_openmp 1000000
# Output should show "Array successfully sorted."
```

---

### Q2.4: Why is task-based parallelism suitable for divide-and-conquer algorithms?

**Answer:**
Task-based parallelism naturally maps to divide-and-conquer because:
1. **Recursive structure**: Each subproblem can be a separate task
2. **Independent subproblems**: Left and right partitions can be sorted independently
3. **Dynamic load balancing**: Smaller partitions complete faster, threads can pick up new tasks
4. **Exploits available parallelism**: Creates as many tasks as needed without over-subscribing

The work tree naturally expands and contracts as the algorithm progresses.

---

## Question 3: Palindromes - Hash Sets and Dynamic Scheduling

### Q3.1: Why is a hash set used instead of linear search for word lookups?

**Answer:**
**Time Complexity:**
- Hash set lookup: O(1) average case
- Linear search: O(n) where n is dictionary size

For a dictionary with ~25,000 words, checking if a reversed word exists:
- Hash set: ~1 operation
- Linear search: up to 25,000 comparisons

With parallel processing checking thousands of words, hash sets provide massive performance improvement.

**Code Snippet (palindromes.c:70-78):**
```c
int hash_set_contains(HashSet* set, const char* key) {
    unsigned long index = hash_function(key) % set->size;
    Node* current = set->buckets[index];
    while (current) {
        if (strcmp(current->key, key) == 0) return 1;
        current = current->next;
    }
    return 0;
}
```

**Test Command:**
```bash
cd Question_3
gcc -o palindromes palindromes.c -fopenmp
export OMP_NUM_THREADS=4
./palindromes /usr/share/dict/words results.txt
```

---

### Q3.2: Explain the purpose of per-thread data structures (per_thread_palindromes, per_thread_semordnilaps).

**Answer:**
Per-thread lists avoid **synchronization overhead** during the parallel loop:
1. **No locking needed**: Each thread writes only to its own list
2. **Better cache locality**: Thread-local data stays in the thread's cache
3. **Prevents false sharing**: No cache line bouncing between cores
4. **Sequential merge**: Results are merged after parallel section (acceptable cost)

Alternative (bad): Using a single shared list with locks would serialize access and eliminate parallelism benefits.

**Code Snippet (palindromes.c:187-204):**
```c
#pragma omp parallel
{
    int tid = omp_get_thread_num();
    list_init(&per_thread_palindromes[tid], 100);
    list_init(&per_thread_semordnilaps[tid], 100);

    #pragma omp for schedule(dynamic, 100)
    for (int i = 0; i < all_words_count; i++) {
        char reversed_word[MAX_WORD_LEN];
        reverse_string(all_words[i], reversed_word);

        if (strcmp(all_words[i], reversed_word) == 0) {
            list_add(&per_thread_palindromes[tid], all_words[i]);
        } else if (strcmp(all_words[i], reversed_word) < 0 &&
                   hash_set_contains(word_set, reversed_word)) {
            list_add(&per_thread_semordnilaps[tid], all_words[i]);
        }
    }
}
```

---

### Q3.3: What is the purpose of `schedule(dynamic, 100)` in the parallel for loop?

**Answer:**
**Dynamic scheduling** assigns loop iterations to threads at runtime:
- **Chunk size 100**: Each thread gets 100 iterations at a time
- **Load balancing**: If one thread finishes early, it gets more work
- **Useful when**: Work per iteration varies (some words are longer, hash collisions vary)

**Alternative schedules:**
- `schedule(static)`: Divides iterations evenly upfront (may cause load imbalance)
- `schedule(guided)`: Starts with large chunks, decreases over time (good for decreasing work)

**Code Snippet (palindromes.c:193):**
```c
#pragma omp for schedule(dynamic, 100)
for (int i = 0; i < all_words_count; i++) {
    // Process each word
}
```

---

### Q3.4: Why does the code use `strcmp(all_words[i], reversed_word) < 0` when checking for semordnilaps?

**Answer:**
This prevents **counting the same pair twice**. For example:
- "draw" and "ward" are the same semordnilap pair
- Without the check, we'd record both "draw→ward" and "ward→draw"
- `strcmp < 0` ensures we only record the lexicographically smaller word

This effectively halves the output for semordnilaps while maintaining correctness.

**Code Snippet (palindromes.c:200):**
```c
else if (strcmp(all_words[i], reversed_word) < 0 &&
         hash_set_contains(word_set, reversed_word)) {
    list_add(&per_thread_semordnilaps[tid], all_words[i]);
}
```

---

## Question 4: 8-Queens - Task-Based Backtracking

### Q4.1: Explain the parallelization strategy for the 8-Queens problem.

**Answer:**
The code uses **top-level parallelism**:
1. Creates 8 tasks, one for each possible placement of the first queen (rows 0-7 in column 0)
2. Each task solves its subproblem **sequentially** using backtracking
3. This divides the search space into 8 roughly equal parts
4. Better load balancing than creating tasks at every recursive level (which would create millions of tiny tasks)

**Code Snippet (8_queens.c:99-116):**
```c
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
                memset(board, -1, N * sizeof(int));
                board[0] = i;  // Place first queen at row i
                solve_queens_recursive(board, 1, &solution_count);
            }
        }
    }
} // Implicit barrier waits for all tasks
```

**Test Command:**
```bash
cd Question_4
gcc -o 8_queens 8_queens.c -fopenmp
export OMP_NUM_THREADS=4
./8_queens
# Should output: Found 92 solutions.
```

---

### Q4.2: Why use `#pragma omp atomic` for incrementing solution_count?

**Answer:**
**Atomic operations** ensure thread-safe increment without the overhead of critical sections:
- Multiple tasks may find solutions simultaneously
- `atomic` guarantees the increment operation is indivisible
- Much faster than critical sections for simple operations
- Prevents race conditions where two threads might read the same value, increment, and write back

**Code Snippet (8_queens.c:67-69):**
```c
if (col >= N) {
    #pragma omp atomic
    (*solution_count)++;
    return;
}
```

**What could go wrong without atomic:**
```
Thread 1 reads solution_count = 10
Thread 2 reads solution_count = 10
Thread 1 writes solution_count = 11
Thread 2 writes solution_count = 11  // Lost update! Should be 12
```

---

### Q4.3: Explain the purpose of the `firstprivate(i)` clause in the task directive.

**Answer:**
`firstprivate(i)` ensures each task gets its own **private copy** of `i`, initialized with the value of `i` at task creation time.

Without `firstprivate`:
- All tasks would share the same `i` variable
- By the time a task executes, `i` might have changed (loop continues)
- Tasks would place the first queen in the wrong row

**Code Snippet (8_queens.c:106-114):**
```c
for (int i = 0; i < N; i++) {
    #pragma omp task firstprivate(i)  // Each task gets its own copy of i
    {
        int board[N];
        memset(board, -1, N * sizeof(int));
        board[0] = i;  // Use the task's private copy of i
        solve_queens_recursive(board, 1, &solution_count);
    }
}
```

---

### Q4.4: Why is the backtracking performed sequentially within each task?

**Answer:**
Creating tasks at every recursive level would cause:
1. **Task overhead explosion**: Millions of tiny tasks created
2. **Overhead > parallelism benefit**: Task creation cost exceeds computation time
3. **Memory overhead**: Each task needs stack space

**Better approach** (used in code):
- Create 8 coarse-grained tasks (top level)
- Each task has substantial work (exploring ~1/8 of search space)
- Task granularity balances parallelism vs overhead

**Performance trade-off:**
- Too fine-grained: overhead dominates
- Too coarse-grained: insufficient parallelism
- 8 tasks for 8 queens: good balance for 4-8 threads

---

## General OpenMP Concepts

### Q5.1: What is the difference between `#pragma omp parallel for` and splitting it into `#pragma omp parallel` + `#pragma omp for`?

**Answer:**
**Combined directive** `#pragma omp parallel for`:
```c
#pragma omp parallel for
for (int i = 0; i < n; i++) { /* work */ }
```
- Creates a parallel region AND distributes loop iterations in one directive
- Simpler, more concise

**Split directives**:
```c
#pragma omp parallel
{
    #pragma omp for
    for (int i = 0; i < n; i++) { /* work */ }
}
```
- Allows additional code in the parallel region before/after the loop
- Useful when you need thread-local initialization (like in palindromes.c)

**When to split:** When you need thread-specific setup/cleanup or multiple work-sharing constructs in one parallel region.

---

### Q5.2: Explain the difference between `task` and `for` work-sharing constructs.

**Answer:**

| Feature | `#pragma omp for` | `#pragma omp task` |
|---------|-------------------|-------------------|
| **Use case** | Regular loops with known iteration count | Irregular/recursive parallelism |
| **Work distribution** | Static/dynamic predetermined | Dynamic task queue |
| **Creation time** | All iterations known upfront | Tasks created dynamically |
| **Synchronization** | Implicit barrier at end | Requires explicit taskwait or region end |
| **Best for** | Data parallelism | Task parallelism |

**Examples:**
- Matrix operations, array processing → `for`
- Quicksort, tree traversal, graph algorithms → `task`

---

### Q5.3: What is the purpose of `omp_get_wtime()` and why is it preferred for parallel programs?

**Answer:**
`omp_get_wtime()` returns wall-clock time in seconds (floating-point):
- **Thread-safe**: Safe to call from multiple threads
- **High precision**: Typically microsecond or better resolution
- **Portable**: Works across different OpenMP implementations
- **Measures real elapsed time**: Unlike CPU time, which sums all threads

**Code Snippet:**
```c
double start_time = omp_get_wtime();
// ... parallel work ...
double end_time = omp_get_wtime();
printf("Execution time: %f seconds\n", end_time - start_time);
```

**Why not `clock()`?**
- `clock()` measures CPU time (sum of all threads)
- For 4 threads doing 2 seconds of work each, `clock()` reports ~8 seconds
- `omp_get_wtime()` correctly reports ~2 seconds wall time

---

### Q5.4: What are race conditions and how does OpenMP help prevent them?

**Answer:**
**Race condition**: Multiple threads access shared data concurrently, and at least one access is a write, leading to unpredictable results.

**OpenMP synchronization mechanisms:**

1. **Critical sections**: `#pragma omp critical`
   - Only one thread at a time
   - Used in matrixSum for min/max updates

2. **Atomic operations**: `#pragma omp atomic`
   - Indivisible read-modify-write
   - Used in 8-queens for counter

3. **Reduction**: `reduction(op:var)`
   - Automatic thread-local copies + combination
   - Used in matrixSum for sum

4. **Locks**: `omp_lock_t` (not used in homework)
   - Explicit lock/unlock

**Example race condition:**
```c
// BAD - race condition
int sum = 0;
#pragma omp parallel for
for (int i = 0; i < n; i++) {
    sum += array[i];  // Multiple threads write to sum simultaneously
}

// GOOD - use reduction
int sum = 0;
#pragma omp parallel for reduction(+:sum)
for (int i = 0; i < n; i++) {
    sum += array[i];  // Each thread has private sum, combined at end
}
```

---

## Performance Analysis Questions

### Q6.1: What factors can limit speedup in parallel programs?

**Answer:**

1. **Amdahl's Law**: Sequential portions limit maximum speedup
   - If 10% is sequential, max speedup ≈ 10× regardless of threads

2. **Overhead**:
   - Thread creation/destruction
   - Task creation and scheduling
   - Synchronization (locks, barriers, atomics)

3. **Load imbalance**: Some threads finish before others
   - Static scheduling with varying work
   - Solution: dynamic scheduling

4. **False sharing**: Threads modify adjacent data on same cache line
   - Causes cache invalidation across cores
   - Solution: padding, per-thread data structures

5. **Memory bandwidth**: All cores compete for RAM access
   - Matrix operations often memory-bound

6. **Granularity**: Task size vs overhead trade-off
   - Too small: overhead dominates
   - Too large: insufficient parallelism

**Test to observe:**
```bash
# Run with different thread counts
for threads in 1 2 4 8; do
    export OMP_NUM_THREADS=$threads
    echo "Threads: $threads"
    ./matrixSum-openmp 2000
done
```

---

### Q6.2: How would you measure speedup and efficiency?

**Answer:**

**Speedup**: S(p) = T(1) / T(p)
- T(1) = execution time with 1 thread
- T(p) = execution time with p threads
- Ideal (linear) speedup: S(p) = p

**Efficiency**: E(p) = S(p) / p
- Percentage of ideal speedup achieved
- E(p) = 1.0 means perfect scaling (100%)
- E(p) < 1.0 indicates overhead/imbalance

**Measurement process:**
1. Run with 1 thread (sequential baseline)
2. Run with 2, 4, 8... threads
3. Run each configuration 5+ times
4. Use median time (reduces noise)
5. Calculate speedup and efficiency

**Example:**
```
T(1) = 10.0s
T(4) = 3.0s
S(4) = 10.0 / 3.0 = 3.33
E(4) = 3.33 / 4 = 0.83 (83% efficiency)
```

---

## Testing Commands Summary

```bash
# Question 1: Matrix Sum
cd Question_1
gcc -O -fopenmp -o matrixSum-openmp matrixSum-openmp.c
./matrixSum-openmp 1000 4
export OMP_NUM_THREADS=2
./matrixSum-openmp 2000

# Question 2: Quicksort
cd ../Question_2
gcc -o quicksort_openmp quicksort_openmp.c -fopenmp
export OMP_NUM_THREADS=4
./quicksort_openmp 100000

# Question 3: Palindromes
cd ../Question_3
gcc -o palindromes palindromes.c -fopenmp
export OMP_NUM_THREADS=4
./palindromes /usr/share/dict/words results.txt
cat results.txt | head -30

# Question 4: 8-Queens
cd ../Question_4
gcc -o 8_queens 8_queens.c -fopenmp
export OMP_NUM_THREADS=4
./8_queens

# Performance testing (run 5 times, use median)
cd ../Question_1
for i in {1..5}; do
    ./matrixSum-openmp 3000 4 | grep "took"
done
```

---

## Key Takeaways

1. **Use `reduction` for simple aggregations** (sum, max, min values only)
2. **Use `critical` for complex updates** (e.g., updating multiple related variables)
3. **Use `atomic` for simple increment/decrement** operations
4. **Use `tasks` for irregular/recursive** parallelism
5. **Use `parallel for` for regular loops** with known bounds
6. **Use per-thread data structures** to avoid synchronization in hot paths
7. **Use `dynamic` scheduling** when work per iteration varies
8. **Measure only parallel sections** for accurate speedup analysis
9. **Balance task granularity** - too fine = overhead, too coarse = poor parallelism
10. **Always verify correctness** before measuring performance
