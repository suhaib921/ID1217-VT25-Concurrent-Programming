# Question 2: Parallel Quicksort with OpenMP Tasks - Analysis

## Summary of Task
Develop a parallel multithreaded quicksort implementation using OpenMP tasks with recursive parallelism. The program should:
- Sort an array of n values using the quicksort algorithm
- Use OpenMP task-based parallelism for recursive decomposition
- Be benchmarked on different thread counts (up to at least 4 threads)
- Be tested with at least 3 different array sizes
- Report speedup relative to sequential execution
- Use median of at least 5 runs for timing
- Use `omp_get_wtime()` for timing measurements

## Analysis of Current Implementation

### Implementation Status: NOT IMPLEMENTED

The Question_2 directory contains only a README.md file with the problem description. **No code implementation exists.**

## Required Implementation Approach

### Algorithm Overview: Quicksort

**Sequential Quicksort Algorithm:**
1. Select a pivot element from the array
2. Partition the array: elements < pivot go left, elements >= pivot go right
3. Recursively sort the left subarray
4. Recursively sort the right subarray
5. Base case: arrays of size 0 or 1 are already sorted

**Parallelization Strategy with OpenMP Tasks:**
The natural parallelism in quicksort comes from the fact that after partitioning, the two subarrays can be sorted independently. OpenMP tasks are ideal for this recursive parallelism pattern.

### Recommended Implementation Structure

```c
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define THRESHOLD 1000  // Switch to sequential below this size

// Sequential partition function
int partition(int *arr, int low, int high) {
    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            // Swap arr[i] and arr[j]
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    // Swap arr[i+1] and arr[high] (pivot)
    int temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;

    return i + 1;
}

// Sequential quicksort for small subarrays
void quicksort_sequential(int *arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quicksort_sequential(arr, low, pi - 1);
        quicksort_sequential(arr, pi + 1, high);
    }
}

// Parallel quicksort using OpenMP tasks
void quicksort_parallel(int *arr, int low, int high) {
    if (low < high) {
        // Use sequential for small subarrays to avoid task overhead
        if (high - low < THRESHOLD) {
            quicksort_sequential(arr, low, high);
            return;
        }

        int pi = partition(arr, low, high);

        // Create tasks for recursive calls
        #pragma omp task shared(arr) firstprivate(low, pi)
        {
            quicksort_parallel(arr, low, pi - 1);
        }

        #pragma omp task shared(arr) firstprivate(pi, high)
        {
            quicksort_parallel(arr, pi + 1, high);
        }

        // Wait for both tasks to complete
        #pragma omp taskwait
    }
}

int main(int argc, char *argv[]) {
    int n = (argc > 1) ? atoi(argv[1]) : 100000;
    int numThreads = (argc > 2) ? atoi(argv[2]) : 4;

    int *arr = (int*)malloc(n * sizeof(int));

    // Initialize with random values
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        arr[i] = rand() % 1000000;
    }

    omp_set_num_threads(numThreads);

    double start = omp_get_wtime();

    // Must create initial parallel region for tasks
    #pragma omp parallel
    {
        #pragma omp single
        {
            quicksort_parallel(arr, 0, n - 1);
        }
    }

    double end = omp_get_wtime();

    printf("Sorted %d elements in %g seconds using %d threads\n",
           n, end - start, numThreads);

    // Verify correctness
    for (int i = 0; i < n - 1; i++) {
        if (arr[i] > arr[i + 1]) {
            printf("ERROR: Array not sorted at index %d\n", i);
            break;
        }
    }

    free(arr);
    return 0;
}
```

## Correctness Considerations

### Critical Synchronization Requirements

1. **Task Dependencies:**
   - `#pragma omp taskwait` is ESSENTIAL after creating subtasks
   - Without taskwait, parent function may return before children complete
   - This would lead to incorrect results or crashes

2. **Data Sharing Clauses:**
   - Array must be `shared(arr)` - all tasks operate on same array
   - Indices must be `firstprivate(low, high, pi)` - each task needs its own copy
   - Using just `private` would leave indices uninitialized

3. **Parallel Region Requirements:**
   - Tasks MUST be created within a parallel region
   - Use `#pragma omp single` to ensure only one thread creates initial tasks
   - Without `single`, all threads would create duplicate tasks

4. **Partition Function Thread Safety:**
   - Partition operates on non-overlapping array regions after initial call
   - No race conditions as each task works on distinct subarray
   - No need for locks or atomic operations

### Common Pitfalls to Avoid

1. **Missing taskwait:**
   ```c
   // INCORRECT - Race condition!
   #pragma omp task
   quicksort_parallel(arr, low, pi - 1);
   #pragma omp task
   quicksort_parallel(arr, pi + 1, high);
   // Missing taskwait - function returns before tasks complete!
   ```

2. **Wrong data sharing:**
   ```c
   // INCORRECT - pi is shared, may be overwritten
   #pragma omp task shared(arr, pi)  // pi should be firstprivate!
   quicksort_parallel(arr, low, pi - 1);
   ```

3. **Creating tasks outside parallel region:**
   ```c
   // INCORRECT - Tasks require parallel region
   void main() {
       #pragma omp task  // ERROR: no parallel region!
       quicksort_parallel(arr, 0, n-1);
   }
   ```

4. **Excessive task creation:**
   - Creating tasks for very small subarrays causes overhead > benefit
   - Must use threshold to switch to sequential execution
   - Typical threshold: 1000-10000 elements depending on system

## Performance Considerations

### Expected Performance Characteristics

**Speedup Analysis:**

1. **Small arrays (< 10,000 elements):**
   - Limited speedup: 1.2-1.5x on 4 cores
   - Task creation overhead dominates
   - Sequential may actually be faster
   - Cache effects significant

2. **Medium arrays (100,000 - 1,000,000):**
   - Good speedup: 2.5-3.2x on 4 cores
   - Task overhead amortized over computation
   - Load balancing generally good

3. **Large arrays (> 10,000,000):**
   - Best speedup: 3.0-3.5x on 4 cores
   - Approaches optimal parallel efficiency
   - Memory bandwidth may become bottleneck

**Why Not Linear Speedup?**

1. **Amdahl's Law:**
   - Sequential partitioning at each level
   - Can't parallelize the partition step effectively
   - Initial partition completely sequential

2. **Load Imbalance:**
   - Partition may create unequal subarrays
   - Poor pivot selection exacerbates this
   - Some tasks finish much faster than others

3. **Task Scheduling Overhead:**
   - OpenMP task queue management
   - Work stealing between threads
   - Synchronization at taskwait points

4. **Cache Effects:**
   - Different threads accessing different array regions
   - Cache line transfers between cores
   - Reduced cache hit rate vs sequential

### Optimization Strategies

1. **Threshold Tuning:**
   - Experiment with different thresholds (500, 1000, 5000, 10000)
   - Optimal value depends on system and array size
   - Too low: excessive overhead; Too high: insufficient parallelism

2. **Pivot Selection:**
   - Better pivot = more balanced partitions = better load balancing
   - Median-of-three: choose median of first, middle, last elements
   - Random pivot: prevents worst-case O(n²) on sorted input

3. **Task Cutoff Depth:**
   - Limit task creation to first K levels of recursion
   - After depth K, use sequential quicksort
   - Reduces total number of tasks created

4. **If Clause:**
   ```c
   #pragma omp task if(high - low > THRESHOLD)
   quicksort_parallel(arr, low, pi - 1);
   ```
   - Conditionally create task only for large subarrays
   - Smaller subarrays execute sequentially in parent task

### Scalability Limitations

1. **Recursion Depth:**
   - Average depth: O(log n)
   - Best parallelism at top levels (few large tasks)
   - Bottom levels: many small tasks (overhead heavy)

2. **Work Stealing:**
   - OpenMP runtime uses work stealing for load balancing
   - Overhead for queue management
   - May help with load imbalance from poor pivots

3. **Memory Bandwidth:**
   - Quicksort is memory-intensive (random access patterns)
   - Multiple threads competing for memory bandwidth
   - May saturate memory subsystem before CPU saturation

## Benchmarking Requirements

### Test Configuration

**Array Sizes (at least 3):**
- Small: 10,000 elements
- Medium: 1,000,000 elements
- Large: 10,000,000 elements

**Thread Counts:**
- 1 thread (baseline for speedup calculation)
- 2 threads
- 4 threads
- 8 threads (if available)

**Methodology:**
- Run each configuration 5+ times
- Report median time (more robust than mean for timing)
- Calculate speedup: T_sequential / T_parallel
- Verify correctness after each run

### Expected Results Example

```
Array Size: 1,000,000 elements

Threads | Time (s) | Speedup | Efficiency
--------|----------|---------|------------
   1    |  0.150   |  1.00   |  100%
   2    |  0.085   |  1.76   |   88%
   4    |  0.051   |  2.94   |   74%
   8    |  0.043   |  3.49   |   44%
```

**Observations:**
- Efficiency decreases with more threads (Amdahl's law)
- Diminishing returns beyond 4 threads on 4-core system
- Hyperthreading provides limited benefit

## Testing and Validation

### Correctness Verification

1. **Sorted Order Check:**
   ```c
   for (int i = 0; i < n - 1; i++) {
       assert(arr[i] <= arr[i + 1]);
   }
   ```

2. **Element Preservation:**
   - Copy array before sorting
   - Sort both with parallel and sequential
   - Compare results element-by-element

3. **Edge Cases:**
   - Empty array (n = 0)
   - Single element (n = 1)
   - Already sorted array
   - Reverse sorted array
   - All identical elements
   - Array with duplicates

### Debugging Techniques

1. **Print task creation:**
   ```c
   #pragma omp task
   {
       printf("Task created for range [%d, %d] by thread %d\n",
              low, high, omp_get_thread_num());
       quicksort_parallel(arr, low, high);
   }
   ```

2. **Task limiting for debugging:**
   - Use small threshold to see task behavior
   - Check for deadlocks or missing taskwaits

3. **Compare with sequential:**
   - Run same input through both versions
   - Verify identical output

## Recommendations

### Implementation Priority

1. **Start with sequential quicksort:**
   - Implement and test thoroughly
   - This becomes the baseline and fallback

2. **Add basic task parallelism:**
   - Simple task creation with taskwait
   - Verify correctness on small arrays

3. **Add threshold optimization:**
   - Implement cutoff for small subarrays
   - Tune threshold experimentally

4. **Improve pivot selection:**
   - Implement median-of-three or random pivot
   - Test on various input distributions

5. **Complete benchmarking:**
   - Systematic testing across sizes and thread counts
   - Analyze and explain results

### Code Quality

1. **Error handling:**
   - Check malloc return value
   - Validate command-line arguments
   - Handle invalid inputs gracefully

2. **Compilation:**
   ```bash
   gcc -O3 -fopenmp -o quicksort quicksort.c
   ```
   - Use -O3 for optimization
   - Ensure -fopenmp is specified

3. **Documentation:**
   - Comment task creation strategy
   - Explain threshold choice
   - Document expected performance

## Conclusion

**Implementation Status:** NOT IMPLEMENTED - requires complete development from scratch.

**Algorithm Suitability:** Quicksort is well-suited for task-based parallelism due to its divide-and-conquer recursive structure. OpenMP tasks are an excellent fit for this pattern.

**Expected Challenges:**
1. Achieving good speedup requires careful threshold tuning
2. Load balancing depends on pivot quality
3. Task overhead can dominate for small problem sizes

**Performance Expectations:** With proper implementation, expect 2.5-3.5x speedup on 4 cores for medium-to-large arrays (100K+ elements), with efficiency of 60-85%.

**Critical Success Factors:**
- Correct data sharing clauses (firstprivate for indices)
- Proper taskwait placement
- Appropriate threshold for task creation
- Robust correctness verification
