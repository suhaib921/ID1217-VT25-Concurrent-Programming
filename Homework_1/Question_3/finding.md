# Question 3 Analysis: Compute Pi Using Adaptive Quadrature

## Summary of the Question/Task

Develop a multithreaded program (C/Pthreads or Java) to compute pi using **adaptive quadrature** of the function f(x) = sqrt(1-x²), which defines a unit circle. The program should:

1. Compute the area of the upper-right quadrant of a unit circle
2. Multiply the result by 4 to get pi
3. Use adaptive quadrature with a given epsilon (error tolerance)
4. Parallelize across np threads (command-line argument)
5. Measure and print execution time (excluding initialization and thread termination)

**Mathematical Foundation**:
- Circle area: πr² = π (for unit circle with r=1)
- Quadrant area: π/4 = ∫₀¹ √(1-x²) dx
- Therefore: π = 4 × ∫₀¹ √(1-x²) dx

## Current Implementation Status

**Status**: NOT IMPLEMENTED

The Question_3 directory contains only the template matrixSum.c file (likely copied by mistake) and a README.md. The actual adaptive quadrature implementation for computing pi does not exist.

## Analysis of Required Implementation Approach

### Adaptive Quadrature Algorithm

Adaptive quadrature recursively subdivides intervals where error is too large:

```c
double adaptive_quad(double (*f)(double), double a, double b, double epsilon) {
    double c = (a + b) / 2.0;
    double h = b - a;

    // Three-point Simpson's rule approximations
    double s_ab = (h / 6.0) * (f(a) + 4*f(c) + f(b));

    double d = (a + c) / 2.0;
    double e = (c + b) / 2.0;
    double s_ac = (h / 12.0) * (f(a) + 4*f(d) + f(c));
    double s_cb = (h / 12.0) * (f(c) + 4*f(e) + f(b));
    double s_acb = s_ac + s_cb;

    double error = fabs(s_acb - s_ab);

    if (error < 15.0 * epsilon) {
        return s_acb + (s_acb - s_ab) / 15.0;  // Richardson extrapolation
    } else {
        return adaptive_quad(f, a, c, epsilon/2.0) +
               adaptive_quad(f, c, b, epsilon/2.0);
    }
}
```

**Properties**:
- Recursive subdivision
- Error estimation via comparing coarse vs fine approximations
- Dynamic work generation (unpredictable recursion depth)

### Circle Function

```c
double circle(double x) {
    return sqrt(1.0 - x*x);
}

double compute_pi(double epsilon, int num_threads) {
    double quadrant_area = adaptive_quad(circle, 0.0, 1.0, epsilon);
    return 4.0 * quadrant_area;
}
```

### Parallelization Challenges

**Challenge 1: Dynamic Work Generation**

- Cannot predict recursion depth or number of intervals
- Work is data-dependent (varies with function smoothness)
- Static partitioning (divide [0,1] into np equal parts) loses adaptivity

**Challenge 2: Irregular Task Granularity**

- Some intervals converge quickly (smooth regions)
- Others subdivide deeply (high curvature near x=1)
- Load imbalance if using static partitioning

**Challenge 3: Load Balancing**

- Near x=0: f(x) ≈ 1 (flat, coarse intervals sufficient)
- Near x=1: f(x) → 0 with infinite derivative (needs fine subdivision)
- Right side of domain requires much more work

### Parallelization Strategies

**Strategy 1: Static Domain Decomposition (Simple but Inefficient)**

```c
typedef struct {
    int thread_id;
    int num_threads;
    double epsilon;
    double result;
} ThreadArgs;

void* worker(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;

    double interval_size = 1.0 / args->num_threads;
    double a = args->thread_id * interval_size;
    double b = a + interval_size;

    args->result = adaptive_quad(circle, a, b, args->epsilon);
    return NULL;
}
```

**Pros**: Simple implementation, no synchronization during computation
**Cons**: Severe load imbalance (thread processing near x=1 does much more work)

**Strategy 2: Work Queue with Task Splitting**

```c
typedef struct {
    double a, b;
    double epsilon;
} Task;

Task task_queue[MAX_TASKS];
int queue_size = 0;
pthread_mutex_t queue_mutex;
pthread_cond_t tasks_available;
int work_done = 0;

void enqueue_task(double a, double b, double epsilon) {
    pthread_mutex_lock(&queue_mutex);
    task_queue[queue_size++] = (Task){a, b, epsilon};
    pthread_cond_signal(&tasks_available);
    pthread_mutex_unlock(&queue_mutex);
}

Task dequeue_task() {
    pthread_mutex_lock(&queue_mutex);
    while (queue_size == 0 && !work_done) {
        pthread_cond_wait(&tasks_available, &queue_mutex);
    }
    Task task = task_queue[--queue_size];
    pthread_mutex_unlock(&queue_mutex);
    return task;
}

void* worker_queue(void* arg) {
    double local_sum = 0.0;
    while (1) {
        Task task = dequeue_task();
        if (work_done && queue_size == 0) break;

        // Try coarse estimate
        double c = (task.a + task.b) / 2.0;
        double h = task.b - task.a;
        double s_ab = (h / 6.0) * (circle(task.a) + 4*circle(c) + circle(task.b));

        double d = (task.a + c) / 2.0;
        double e = (c + task.b) / 2.0;
        double s_ac = (h / 12.0) * (circle(task.a) + 4*circle(d) + circle(c));
        double s_cb = (h / 12.0) * (circle(c) + 4*circle(e) + circle(task.b));
        double s_acb = s_ac + s_cb;

        if (fabs(s_acb - s_ab) < 15.0 * task.epsilon) {
            local_sum += s_acb + (s_acb - s_ab) / 15.0;
        } else {
            // Split into two tasks
            enqueue_task(task.a, c, task.epsilon / 2.0);
            enqueue_task(c, task.b, task.epsilon / 2.0);
        }
    }

    pthread_mutex_lock(&result_mutex);
    global_result += local_sum;
    pthread_mutex_unlock(&result_mutex);
    return NULL;
}
```

**Pros**: Perfect load balancing, dynamic task distribution
**Cons**: Complex implementation, synchronization overhead, potential queue contention

**Strategy 3: Hybrid - Static Initial Split + Sequential Adaptive**

```c
void* worker_hybrid(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;

    // Each thread gets one initial interval
    double interval_size = 1.0 / args->num_threads;
    double a = args->thread_id * interval_size;
    double b = a + interval_size;

    // Run sequential adaptive quadrature on this interval
    args->result = adaptive_quad_sequential(circle, a, b, args->epsilon);
    return NULL;
}
```

**Pros**: Simple, no synchronization during computation
**Cons**: Load imbalance (but better than nothing)

**Strategy 4: Initial Coarse Split + Work Stealing**

Pre-compute many small intervals, then distribute:

```c
// Phase 1: Main thread generates initial task pool
Task initial_tasks[1000];
int task_count = generate_initial_tasks(circle, 0.0, 1.0, epsilon, initial_tasks);

// Phase 2: Workers pick tasks from shared pool
pthread_mutex_t task_index_mutex;
int next_task_index = 0;

void* worker_steal(void* arg) {
    double local_sum = 0.0;
    while (1) {
        int my_task;
        pthread_mutex_lock(&task_index_mutex);
        if (next_task_index >= task_count) {
            pthread_mutex_unlock(&task_index_mutex);
            break;
        }
        my_task = next_task_index++;
        pthread_mutex_unlock(&task_index_mutex);

        Task t = initial_tasks[my_task];
        local_sum += adaptive_quad_sequential(circle, t.a, t.b, t.epsilon);
    }

    pthread_mutex_lock(&result_mutex);
    global_result += local_sum;
    pthread_mutex_unlock(&result_mutex);
    return NULL;
}
```

**Pros**: Good load balancing, moderate complexity
**Cons**: Requires good initial task generation

## Correctness Assessment

### Mathematical Correctness

**Formula**: π = 4 × ∫₀¹ √(1-x²) dx

**Verification**:
- Analytical result: ∫₀¹ √(1-x²) dx = π/4
- Numerical result should be within epsilon of π ≈ 3.14159265359

### Numerical Issues

**Issue 1: Near x=1**

```c
double circle(double x) {
    return sqrt(1.0 - x*x);  // sqrt(ε) when x ≈ 1
}
```

**Problem**:
- At x=1.0, 1-x² = 0, sqrt(0) = 0 (OK)
- At x=0.999999, sqrt(1-x²) involves small numbers → precision loss
- Derivative: f'(x) = -x/√(1-x²) → ∞ as x→1

**Impact**: Needs many subdivisions near x=1, but numerically stable

**Issue 2: Floating-Point Accumulation**

```c
global_result += local_sum;  // Multiple threads adding to global
```

**Problem**: Floating-point addition is not associative
- (a + b) + c ≠ a + (b + c) in floating point
- Different thread orderings produce slightly different results
- **Not a bug**: All results within epsilon tolerance

**Issue 3: Epsilon Distribution**

In recursive calls: `adaptive_quad(f, a, c, epsilon/2.0)`

**Reason**: Error budget split between left and right subintervals
**Correctness**: Total error remains bounded by original epsilon

### Parallel Correctness

**Requirement 1: No Data Races**

- Each thread computes on disjoint intervals: [a,b] regions don't overlap
- Only shared write is final result accumulation (must be mutex-protected)

**Requirement 2: Result Consistency**

- Sum of partial results must equal total integral
- ∫₀¹ f(x)dx = ∫₀^⁰·⁵ f(x)dx + ∫₀.₅¹ f(x)dx  (linearity of integration)

**Requirement 3: Termination**

- Each recursive call reduces interval size
- Error eventually becomes < epsilon
- Guaranteed termination (barring floating-point issues)

## Performance Considerations

### Theoretical Performance

**Sequential Complexity**:
- Unknown a priori (depends on function and epsilon)
- For smooth functions: O(1/epsilon) intervals
- For circle function: O(1/epsilon²) near x=1 (steep derivative)

**Parallel Speedup**:
- **Ideal**: S = np (number of threads)
- **Realistic**: S ≈ 0.6 × np (due to load imbalance and overhead)
- **Amdahl's Law**: Limited by initial interval creation (sequential phase)

### Performance Analysis by Strategy

**Strategy 1: Static Decomposition**

- Interval [0, 0.125]: ~10 subdivisions (smooth)
- Interval [0.875, 1.0]: ~10000 subdivisions (steep)
- Speedup: Limited by thread handling rightmost interval
- **Expected**: S ≈ 1.5-2.0 with 4 threads (severe imbalance)

**Strategy 2: Work Queue**

- Perfect load balancing
- Overhead: mutex lock/unlock per task
- Queue contention when many threads
- **Expected**: S ≈ 3.0-3.5 with 4 threads (overhead reduces efficiency)

**Strategy 3: Hybrid**

- Better than static, worse than work queue
- Minimal synchronization
- **Expected**: S ≈ 2.5-3.0 with 4 threads

**Strategy 4: Initial Task Pool**

- Very good load balancing
- Low synchronization overhead (only task index increment)
- **Expected**: S ≈ 3.2-3.8 with 4 threads

### Optimization Opportunities

1. **Function Memoization**: Cache sqrt(1-x²) values (unlikely to help - few repeated x values)

2. **SIMD Vectorization**: Compute f(a), f(c), f(b) in parallel using SSE/AVX

3. **Reduce Synchronization**: Use atomic operations for task index:
   ```c
   int my_task = __atomic_fetch_add(&next_task_index, 1, __ATOMIC_SEQ_CST);
   ```

4. **Batch Results**: Accumulate many local results before locking global sum:
   ```c
   // Update global result every 100 intervals instead of every interval
   ```

5. **Better Error Estimation**: Use higher-order quadrature rules to reduce subdivisions

6. **Epsilon Scaling**: Distribute epsilon based on interval size or expected difficulty

### Expected Execution Time

**Parameters**: epsilon = 10⁻⁶, np = 4

**Sequential**:
- Estimated intervals: ~1000-10000
- Time: ~10-50 ms (depends on function evaluation cost)

**Parallel (Strategy 4)**:
- Speedup: ~3x
- Time: ~5-20 ms

**Measurement**: Should show diminishing returns beyond 4-8 threads due to overhead

## Synchronization and Concurrency Issues

### Issue 1: Result Accumulation Race Condition

**Problem**: Multiple threads updating global result

```c
global_result += local_sum;  // NOT THREAD-SAFE
```

**Fix**: Mutex protection
```c
pthread_mutex_lock(&result_mutex);
global_result += local_sum;
pthread_mutex_unlock(&result_mutex);
```

**Alternative**: Each thread stores in array, main sums sequentially
```c
double results[MAX_THREADS];
// Workers write to results[thread_id]
// Main thread sums after joining
```

### Issue 2: Work Queue Synchronization

**Scenario**: Multiple threads dequeue simultaneously

**Required Locks**:
1. Queue access (enqueue/dequeue) - mutex
2. Queue empty condition - condition variable
3. Work completion signal - flag + condition variable

**Deadlock Risk**: None (single mutex, no circular dependencies)

**Starvation Risk**: Possible if producer (task splitter) slower than consumers
- **Solution**: Threads should generate tasks, not just consume

### Issue 3: Task Queue Overflow

**Problem**: Recursive splitting generates unlimited tasks

```c
Task task_queue[MAX_TASKS];  // Fixed size
```

**If queue full**:
- Option 1: Block until space available (adds synchronization)
- Option 2: Fall back to sequential processing (hybrid approach)
- Option 3: Dynamic allocation (malloc overhead)

**Recommended**: Option 2 (process sequentially if can't enqueue)

### Issue 4: Termination Detection

**Challenge**: How to know when all work is done?

**Naive approach** (WRONG):
```c
while (queue_size > 0) {
    // Process tasks
}
// Threads may exit while others are still generating tasks!
```

**Correct approach**: Reference counting
```c
int active_tasks = 1;  // Initially one task [0,1]
pthread_mutex_t task_counter_mutex;

void enqueue_task(...) {
    pthread_mutex_lock(&task_counter_mutex);
    task_queue[queue_size++] = task;
    active_tasks++;
    pthread_mutex_unlock(&task_counter_mutex);
}

void finish_task() {
    pthread_mutex_lock(&task_counter_mutex);
    active_tasks--;
    if (active_tasks == 0) {
        work_done = 1;
        pthread_cond_broadcast(&tasks_available);
    }
    pthread_mutex_unlock(&task_counter_mutex);
}
```

### Issue 5: Floating-Point Non-Determinism

**Observation**: Running same program twice may give slightly different results

**Cause**: Thread scheduling affects summation order
- Run 1: result = r1 + r2 + r3 + r4
- Run 2: result = r2 + r1 + r4 + r3
- Due to floating-point rounding, these may differ in last bits

**Is this a bug?**: NO
- Both results are within epsilon tolerance
- Expected behavior in parallel numerical computation
- Can make deterministic by sorting partial results before summing (overkill)

## Recommendations and Improvements

### Recommended Implementation (Strategy 4 - Task Pool)

```c
#define INITIAL_TASKS 1000

int main(int argc, char* argv[]) {
    double epsilon = (argc > 1) ? atof(argv[1]) : 1e-6;
    int num_threads = (argc > 2) ? atoi(argv[2]) : 4;

    // Phase 1: Generate initial task pool (sequential)
    Task* tasks = malloc(INITIAL_TASKS * sizeof(Task));
    int task_count = generate_coarse_tasks(circle, 0.0, 1.0, epsilon, tasks, INITIAL_TASKS);

    // Phase 2: Parallel processing
    pthread_t threads[num_threads];
    double results[num_threads];

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (int i = 0; i < num_threads; i++) {
        ThreadArgs* args = malloc(sizeof(ThreadArgs));
        args->thread_id = i;
        args->tasks = tasks;
        args->task_count = task_count;
        args->result = &results[i];
        pthread_create(&threads[i], NULL, worker, args);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&end, NULL);

    // Phase 3: Sum results
    double pi_approx = 0.0;
    for (int i = 0; i < num_threads; i++) {
        pi_approx += results[i];
    }
    pi_approx *= 4.0;

    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Pi approximation: %.15f\n", pi_approx);
    printf("Error: %.2e\n", fabs(pi_approx - M_PI));
    printf("Execution time: %g sec\n", elapsed);
    printf("Epsilon: %.2e\n", epsilon);
    printf("Threads: %d\n", num_threads);

    free(tasks);
    return 0;
}
```

### Initial Task Generation

```c
int generate_coarse_tasks(double (*f)(double), double a, double b, double epsilon, Task* tasks, int max_tasks) {
    int count = 0;
    double step = (b - a) / max_tasks;

    for (int i = 0; i < max_tasks; i++) {
        tasks[count].a = a + i * step;
        tasks[count].b = a + (i + 1) * step;
        tasks[count].epsilon = epsilon;  // May want to adjust based on interval
        count++;
    }

    return count;
}
```

### Worker Function

```c
typedef struct {
    int thread_id;
    Task* tasks;
    int task_count;
    double* result;
} ThreadArgs;

pthread_mutex_t task_index_mutex = PTHREAD_MUTEX_INITIALIZER;
int next_task_index = 0;

void* worker(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    double local_sum = 0.0;

    while (1) {
        int my_task;
        pthread_mutex_lock(&task_index_mutex);
        if (next_task_index >= args->task_count) {
            pthread_mutex_unlock(&task_index_mutex);
            break;
        }
        my_task = next_task_index++;
        pthread_mutex_unlock(&task_index_mutex);

        Task t = args->tasks[my_task];
        local_sum += adaptive_quad_sequential(circle, t.a, t.b, t.epsilon);
    }

    *(args->result) = local_sum;
    free(args);
    return NULL;
}
```

### Testing and Validation

1. **Correctness Tests**:
   ```c
   assert(fabs(pi_approx - M_PI) < epsilon);
   ```

2. **Convergence Test**: Vary epsilon and verify error decreases
   - epsilon = 10⁻³ → error ~ 10⁻³
   - epsilon = 10⁻⁶ → error ~ 10⁻⁶

3. **Thread Scalability Test**: Measure speedup vs number of threads
   - 1 thread: baseline
   - 2 threads: should be ~1.8x faster
   - 4 threads: should be ~3.0x faster
   - 8 threads: diminishing returns

4. **Reproducibility**: Run multiple times, verify results within tolerance

### Advanced Considerations

1. **Adaptive Epsilon Distribution**: Give more error budget to steep regions
2. **Alternative Quadrature**: Gauss-Kronrod rules for better accuracy
3. **Comparison**: Implement both static and dynamic strategies, compare performance
4. **Monte Carlo Alternative**: Compare adaptive quadrature with parallel Monte Carlo integration

## Conclusion

**Task**: Compute pi using parallel adaptive quadrature of circle function

**Current Status**: Not implemented - only template matrixSum.c exists

**Key Challenges**:
1. Dynamic, irregular workload (adaptive refinement)
2. Severe load imbalance with naive static partitioning
3. Task generation and distribution synchronization
4. Floating-point accumulation across threads

**Recommended Approach**:
- Initial coarse task pool (1000 intervals)
- Worker threads fetch tasks dynamically
- Minimal synchronization (atomic task index)
- Per-thread result accumulation

**Expected Learning**:
1. Dynamic load balancing techniques
2. Trade-offs between synchronization overhead and load balance
3. Numerical integration parallelization
4. Work distribution patterns (static vs dynamic)

**Difficulty**: Medium-High
- Requires understanding of adaptive quadrature algorithm
- Need to implement effective load balancing
- Must handle floating-point precision carefully
- Performance analysis reveals Amdahl's Law limitations

**Estimated Speedup**: 2.5-3.5x with 4 threads (epsilon = 10⁻⁶)
