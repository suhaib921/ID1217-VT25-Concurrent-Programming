# Question 2 Analysis: Parallel Quicksort with Recursive Parallelism

## Summary of the Question/Task

Develop a parallel multithreaded program (C/Pthreads or Java) implementing the quicksort algorithm with **recursive parallelism**. The program should:

1. Sort an array of n values using the quicksort algorithm
2. Implement parallel recursive decomposition (spawn threads for subproblems)
3. Measure and print execution time using `times()` or `gettimeofday()`
4. Time only the parallel computation phase (after initialization, before thread termination)

**Key Requirement**: Recursive parallelism - create threads recursively to sort sublists in parallel.

## Current Implementation Status

**Status**: NOT IMPLEMENTED

The Question_2 directory contains only a README.md file with the problem description. No code implementation exists.

## Analysis of Required Implementation Approach

### Sequential Quicksort Algorithm

```c
void quicksort(int arr[], int left, int right) {
    if (left < right) {
        int pivot_index = partition(arr, left, right);
        quicksort(arr, left, pivot_index - 1);   // Left sublist
        quicksort(arr, pivot_index + 1, right);  // Right sublist
    }
}
```

### Parallel Quicksort Strategy

**Approach 1: Unbounded Thread Creation (Naive)**

```c
void* parallel_quicksort(void* arg) {
    QsortArgs* args = (QsortArgs*)arg;
    if (args->left >= args->right) return NULL;

    int pivot_index = partition(args->arr, args->left, args->right);

    pthread_t left_thread, right_thread;

    // Create thread for left sublist
    QsortArgs left_args = {args->arr, args->left, pivot_index - 1};
    pthread_create(&left_thread, NULL, parallel_quicksort, &left_args);

    // Create thread for right sublist
    QsortArgs right_args = {args->arr, pivot_index + 1, args->right};
    pthread_create(&right_thread, NULL, parallel_quicksort, &right_args);

    pthread_join(left_thread, NULL);
    pthread_join(right_thread, NULL);

    return NULL;
}
```

**Problem**: Exponential thread explosion - O(2^depth) threads for recursion depth

**Approach 2: Bounded Thread Creation (Recommended)**

Use a thread pool or depth limit:

```c
#define MAX_DEPTH 10  // Limit recursion depth for thread creation
int active_threads = 0;
int max_threads = 8;
pthread_mutex_t thread_count_mutex;

void parallel_quicksort_limited(int arr[], int left, int right, int depth) {
    if (left >= right) return;

    int pivot_index = partition(arr, left, right);

    int create_threads = 0;
    pthread_mutex_lock(&thread_count_mutex);
    if (active_threads < max_threads && depth < MAX_DEPTH) {
        active_threads++;
        create_threads = 1;
    }
    pthread_mutex_unlock(&thread_count_mutex);

    if (create_threads) {
        pthread_t thread;
        QsortArgs args = {arr, left, pivot_index - 1, depth + 1};
        pthread_create(&thread, NULL, worker_quicksort, &args);

        // Sort right sublist in current thread
        parallel_quicksort_limited(arr, pivot_index + 1, right, depth + 1);

        pthread_join(thread, NULL);

        pthread_mutex_lock(&thread_count_mutex);
        active_threads--;
        pthread_mutex_unlock(&thread_count_mutex);
    } else {
        // Sequential fallback
        quicksort_sequential(arr, left, pivot_index - 1);
        quicksort_sequential(arr, pivot_index + 1, right);
    }
}
```

**Approach 3: Task Queue Pattern**

Use a shared work queue where threads fetch unprocessed sublists:

```c
typedef struct {
    int left;
    int right;
} Task;

Task task_queue[MAX_TASKS];
int queue_head = 0, queue_tail = 0;
pthread_mutex_t queue_mutex;
pthread_cond_t tasks_available;
int done = 0;
```

### Partition Algorithm

**In-place partition** (Hoare or Lomuto scheme):

```c
int partition(int arr[], int left, int right) {
    int pivot = arr[right];  // Choose last element as pivot
    int i = left - 1;

    for (int j = left; j < right; j++) {
        if (arr[j] <= pivot) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[right]);
    return i + 1;
}
```

**Thread Safety**: Partition operates on disjoint array regions - no synchronization needed if regions don't overlap.

## Correctness Assessment

### Algorithm Correctness

**Sequential Quicksort**: Correct if partition maintains invariant:
- All elements arr[left...pivot-1] <= arr[pivot]
- All elements arr[pivot+1...right] > arr[pivot]

**Parallel Correctness**: Additional requirements:
1. **Disjoint regions**: Left and right sublists must not overlap (guaranteed by partition)
2. **Thread coordination**: Parent must join children before returning (ensures sorting completes)
3. **Data races**: No concurrent writes to same array element (guaranteed by disjoint regions)

### Potential Correctness Issues

**Issue 1: Stack-allocated Arguments**

```c
// WRONG - args goes out of scope
QsortArgs left_args = {arr, left, pivot_index - 1};
pthread_create(&left_thread, NULL, parallel_quicksort, &left_args);
// left_args destroyed here, but thread may not have read it yet
```

**Fix**: Heap allocate or use thread-safe copying mechanism

**Issue 2: Thread Creation Failure**

If `pthread_create()` fails but not checked, sublist won't be sorted.

**Fix**: Check return value and fall back to sequential sorting:

```c
if (pthread_create(&thread, NULL, worker, &args) != 0) {
    // Fall back to sequential
    quicksort_sequential(arr, left, pivot_index - 1);
}
```

**Issue 3: Pivot Selection**

Poor pivot selection (e.g., always first/last element) causes worst-case O(n²) on sorted data.

**Fix**: Use median-of-three or random pivot selection

**Issue 4: Small Sublist Overhead**

Creating threads for very small sublists wastes resources.

**Fix**: Switch to sequential quicksort below threshold:

```c
#define SEQUENTIAL_THRESHOLD 1000
if (right - left < SEQUENTIAL_THRESHOLD) {
    quicksort_sequential(arr, left, right);
    return;
}
```

## Performance Considerations

### Theoretical Analysis

**Sequential Quicksort**:
- Average: O(n log n)
- Worst: O(n²) (already sorted with bad pivot)
- Space: O(log n) recursion stack

**Parallel Quicksort**:
- Ideal speedup: O(n log n / p) where p = number of processors
- Work: O(n log n) (same as sequential)
- Span (critical path): O(log² n) with ideal partitioning

**Amdahl's Law**: Speedup limited by:
1. Partition step (sequential within each recursion level)
2. Thread creation overhead
3. Thread synchronization (join operations)

### Performance Challenges

**Challenge 1: Thread Creation Overhead**

- Creating pthread costs ~1-10 microseconds
- Sorting small sublist (<1000 elements) takes similar time
- **Solution**: Limit thread creation depth and use sequential cutoff

**Challenge 2: Load Imbalance**

- Unbalanced partitions lead to unequal work distribution
- One thread may finish quickly while another processes large sublist
- **Solution**: Better pivot selection, work-stealing task queues

**Challenge 3: Memory Hierarchy**

- Sequential quicksort has good cache locality (works on contiguous regions)
- Parallel version fragments cache usage across threads
- **Solution**: Ensure each thread's working set fits in L2/L3 cache

**Challenge 4: Scalability Limits**

With P processors:
- Effective parallelism only in first log₂(P) recursion levels
- After that, each processor works on separate sublist sequentially
- **Expected speedup**: 3-5x on 8 cores (not 8x due to overhead)

### Optimization Strategies

1. **Hybrid Approach**: Parallel for large sublists, sequential for small
2. **Thread Pooling**: Reuse threads instead of creating new ones
3. **Work Stealing**: Idle threads steal tasks from busy threads
4. **NUMA Awareness**: Keep data local to thread's NUMA node
5. **Pivot Optimization**: Median-of-three, random sampling
6. **Insertion Sort Cutoff**: Use insertion sort for very small sublists (<10 elements)

### Expected Performance

**Best Case** (good pivots, 8 threads, large array):
- Speedup: 4-6x
- Efficiency: 50-75%

**Worst Case** (poor pivots, overhead dominates):
- Speedup: 1-2x (may be slower than sequential)
- Efficiency: 12-25%

**Realistic** (random data, n=10⁶, 4 threads):
- Speedup: 2.5-3.5x
- Execution time: Sequential ~100ms, Parallel ~30ms

## Synchronization and Concurrency Issues

### Issue 1: Thread Explosion

**Problem**: Creating 2 threads per recursion level → 2^depth threads

**Calculation**: For array of 10⁶ elements with average log₂(10⁶) ≈ 20 levels:
- Potential threads: 2²⁰ = 1,048,576 threads
- System limit: Usually 1024-10000 threads
- Result: Resource exhaustion, system crash

**Solutions**:

a) **Depth Limiting**:
```c
if (depth >= MAX_DEPTH) {
    quicksort_sequential(arr, left, right);
    return;
}
```

b) **Thread Count Limiting**:
```c
pthread_mutex_lock(&thread_count_mutex);
int can_create = (active_threads < max_threads);
if (can_create) active_threads++;
pthread_mutex_unlock(&thread_count_mutex);
```

c) **Adaptive Strategy**: Only parallelize if sublist is large enough

### Issue 2: Argument Passing Race Condition

**Problem**: Passing stack-allocated struct to pthread_create()

```c
QsortArgs args = {arr, left, right};  // Stack allocated
pthread_create(&thread, NULL, worker, &args);
// args may be overwritten before thread reads it
```

**Solutions**:

a) **Heap Allocation**:
```c
QsortArgs* args = malloc(sizeof(QsortArgs));
args->arr = arr; args->left = left; args->right = right;
pthread_create(&thread, NULL, worker, args);
// Worker must free(args)
```

b) **Wait for Thread to Copy**:
```c
pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
// Use condition variable to signal args copied
```

c) **Encode in Pointer** (for small indices):
```c
// Pack left and right into void* (only works for small values)
void* packed = (void*)(((long)left << 32) | right);
pthread_create(&thread, NULL, worker, packed);
```

### Issue 3: Thread Join Overhead

**Problem**: Calling pthread_join() is blocking - parent thread idles

**Impact**: If left sublist finishes quickly, thread waits idle for right sublist

**Solution**: Parent processes one sublist while thread handles other:
```c
pthread_create(&thread, NULL, worker, &left_args);
parallel_quicksort(arr, pivot_index + 1, right);  // Process right in current thread
pthread_join(thread, NULL);
```

### Issue 4: False Sharing (Unlikely in Quicksort)

**Scenario**: If partition boundary happens to align with cache line boundary

**Impact**: Minimal - threads work on well-separated regions

**Prevention**: Ensure partition index not shared between threads' working sets

### Issue 5: Thread Safety of Partition

**Analysis**: Partition only modifies arr[left...right]

**Guarantee**: If recursive calls have disjoint [left, right] ranges, no race condition

**Verification**:
- Left child: [left, pivot-1]
- Right child: [pivot+1, right]
- Disjoint: max(left child) = pivot-1 < pivot+1 = min(right child) ✓

## Recommendations and Improvements

### Essential Implementation Elements

1. **Command-line Arguments**:
   ```c
   int main(int argc, char* argv[]) {
       int n = (argc > 1) ? atoi(argv[1]) : 100000;
       int num_threads = (argc > 2) ? atoi(argv[2]) : 4;
   ```

2. **Array Initialization**:
   ```c
   int* arr = malloc(n * sizeof(int));
   srand(time(NULL));
   for (int i = 0; i < n; i++) {
       arr[i] = rand() % 1000000;
   }
   ```

3. **Timing**:
   ```c
   struct timeval start, end;
   gettimeofday(&start, NULL);
   parallel_quicksort(arr, 0, n-1);
   gettimeofday(&end, NULL);
   double elapsed = (end.tv_sec - start.tv_sec) +
                    (end.tv_usec - start.tv_usec) / 1e6;
   ```

4. **Verification**:
   ```c
   int is_sorted(int arr[], int n) {
       for (int i = 1; i < n; i++) {
           if (arr[i] < arr[i-1]) return 0;
       }
       return 1;
   }
   ```

### Advanced Optimizations

1. **Three-Way Partitioning**: Handle duplicate keys efficiently
2. **Introspective Sort**: Switch to heapsort if recursion too deep
3. **Sample-Based Pivot**: Choose pivot from random sample for better balance
4. **SIMD Partition**: Use vector instructions for comparison phase

### Testing Strategy

1. **Correctness Tests**:
   - Empty array: n=0
   - Single element: n=1
   - Already sorted: [1,2,3,...,n]
   - Reverse sorted: [n,n-1,...,1]
   - All duplicates: [5,5,5,...]
   - Random data: various sizes

2. **Performance Tests**:
   - Vary array size: 10³, 10⁴, 10⁵, 10⁶
   - Vary thread count: 1, 2, 4, 8, 16
   - Measure speedup and efficiency
   - Compare with sequential version

3. **Stress Tests**:
   - Very large arrays (10⁸ elements)
   - Many threads (16, 32)
   - Monitor system resources

### Common Mistakes to Avoid

1. **Not handling thread creation failure**: Always check pthread_create() return value
2. **Memory leaks**: Free heap-allocated argument structs in worker threads
3. **Not validating input**: Check n > 0, num_threads > 0
4. **Incorrect timing**: Including initialization in measured time
5. **Integer overflow**: Use size_t for array indices with large arrays

## Conclusion

**Task**: Implement parallel quicksort with recursive parallelism using Pthreads

**Current Status**: Not implemented - directory only contains README

**Key Challenges**:
1. Controlling thread explosion (depth/count limits)
2. Managing thread lifecycle (creation, joining, cleanup)
3. Balancing parallelism overhead vs speedup benefit
4. Ensuring correctness with stack-allocated arguments

**Expected Outcome**:
- Working parallel quicksort implementation
- Timing measurements showing speedup
- Demonstration of Amdahl's Law in practice

**Learning Objectives**:
1. Recursive parallelism pattern
2. Dynamic thread creation and management
3. Performance analysis of parallel algorithms
4. Trade-offs between parallelism and overhead
5. Thread synchronization through join operations

**Estimated Effort**: Medium difficulty - requires understanding of:
- Quicksort algorithm
- Pthread thread creation and joining
- Performance measurement
- Resource management (preventing thread explosion)
