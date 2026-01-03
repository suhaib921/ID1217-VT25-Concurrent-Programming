# Question 6 Analysis: Find Palindromes and Semordnilaps

## Summary of the Question/Task

Develop a parallel multithreaded program (C/Pthreads or Java) to find palindromes and semordnilaps in a dictionary file:

**Definitions**:
- **Palindrome**: Word that reads the same forward and backward (e.g., "noon", "radar")
- **Semordnilap**: Word whose reverse is a different valid word in dictionary (e.g., "draw" ↔ "ward")

**Requirements**:
1. Read dictionary from `/usr/dict/words` or provided `words` file (25,143 words)
2. Use W worker threads (command-line argument)
3. Workers handle only the **compute phase** (finding palindromes/semordnilaps)
4. Input and output phases are **sequential**
5. Each worker counts palindromes and semordnilaps it finds
6. Output:
   - Write results to file
   - Print total count of palindromes and semordnilaps
   - Print per-worker counts
7. Measure execution time (parallel phase only)

**Point Value**: 40 points (double the previous problems - more complex)

## Current Implementation Status

**Status**: NOT IMPLEMENTED

The Question_6 directory contains:
- `README.md`: Problem description
- `matrixSum.c`: Template file (unrelated to this problem)
- `words`: Dictionary file with 25,143 words

No actual palindrome/semordnilap detection implementation exists.

## Analysis of Required Implementation Approach

### Algorithm Design

#### Phase 1: Input (Sequential)

```c
#define MAX_WORDS 30000
#define MAX_WORD_LENGTH 50

char dictionary[MAX_WORDS][MAX_WORD_LENGTH];
int word_count = 0;

// Read all words into memory
FILE* f = fopen("words", "r");
while (fgets(dictionary[word_count], MAX_WORD_LENGTH, f) != NULL) {
    // Remove newline
    dictionary[word_count][strcspn(dictionary[word_count], "\n")] = '\0';
    word_count++;
}
fclose(f);
```

#### Phase 2: Compute (Parallel)

**Palindrome Detection**:
```c
int is_palindrome(const char* word) {
    int len = strlen(word);
    for (int i = 0; i < len / 2; i++) {
        if (word[i] != word[len - 1 - i]) {
            return 0;
        }
    }
    return 1;
}
```

**Semordnilap Detection**:
```c
// Reverse a word
void reverse_word(const char* word, char* reversed) {
    int len = strlen(word);
    for (int i = 0; i < len; i++) {
        reversed[i] = word[len - 1 - i];
    }
    reversed[len] = '\0';
}

// Check if word is in dictionary using binary search
int in_dictionary(const char* word) {
    int left = 0, right = word_count - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = strcmp(word, dictionary[mid]);
        if (cmp == 0) return 1;
        if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
    return 0;
}

int is_semordnilap(const char* word) {
    char reversed[MAX_WORD_LENGTH];
    reverse_word(word, reversed);

    // Must be different from original and exist in dictionary
    if (strcmp(word, reversed) == 0) return 0;  // It's a palindrome, not semordnilap
    return in_dictionary(reversed);
}
```

#### Phase 3: Output (Sequential)

```c
// After workers finish, main thread writes results
FILE* output = fopen("results.txt", "w");
for (int i = 0; i < palindrome_count; i++) {
    fprintf(output, "Palindrome: %s\n", palindromes[i]);
}
for (int i = 0; i < semordnilap_count; i++) {
    fprintf(output, "Semordnilap: %s ↔ %s\n", semordnilaps[i].word, semordnilaps[i].reverse);
}
fclose(output);

printf("Total palindromes: %d\n", palindrome_count);
printf("Total semordnilaps: %d\n", semordnilap_count);
for (int i = 0; i < num_workers; i++) {
    printf("Worker %d found %d palindromes and %d semordnilaps\n",
           i, worker_stats[i].palindromes, worker_stats[i].semordnilaps);
}
```

### Parallelization Strategies

#### Strategy 1: Static Partitioning (Simple)

```c
typedef struct {
    int thread_id;
    int start_index;
    int end_index;
} WorkerArgs;

void* worker(void* arg) {
    WorkerArgs* args = (WorkerArgs*)arg;
    int my_palindromes = 0;
    int my_semordnilaps = 0;

    for (int i = args->start_index; i < args->end_index; i++) {
        if (is_palindrome(dictionary[i])) {
            // Store palindrome
            pthread_mutex_lock(&result_mutex);
            strcpy(palindromes[palindrome_count++], dictionary[i]);
            pthread_mutex_unlock(&result_mutex);
            my_palindromes++;
        } else if (is_semordnilap(dictionary[i])) {
            // Store semordnilap
            pthread_mutex_lock(&result_mutex);
            strcpy(semordnilaps[semordnilap_count++].word, dictionary[i]);
            pthread_mutex_unlock(&result_mutex);
            my_semordnilaps++;
        }
    }

    worker_stats[args->thread_id].palindromes = my_palindromes;
    worker_stats[args->thread_id].semordnilaps = my_semordnilaps;

    return NULL;
}

// Main thread creates workers
int words_per_worker = word_count / num_workers;
for (int i = 0; i < num_workers; i++) {
    args[i].thread_id = i;
    args[i].start_index = i * words_per_worker;
    args[i].end_index = (i == num_workers - 1) ? word_count : (i + 1) * words_per_worker;
    pthread_create(&threads[i], NULL, worker, &args[i]);
}
```

**Pros**: Simple, no dynamic synchronization
**Cons**: Results stored in shared array (requires mutex), no local buffering

#### Strategy 2: Per-Worker Result Buffers (Better)

```c
typedef struct {
    char palindromes[1000][MAX_WORD_LENGTH];
    int palindrome_count;
    char semordnilaps[1000][MAX_WORD_LENGTH];
    int semordnilap_count;
} WorkerResults;

WorkerResults worker_results[MAX_WORKERS];

void* worker(void* arg) {
    WorkerArgs* args = (WorkerArgs*)arg;
    WorkerResults* my_results = &worker_results[args->thread_id];
    my_results->palindrome_count = 0;
    my_results->semordnilap_count = 0;

    for (int i = args->start_index; i < args->end_index; i++) {
        if (is_palindrome(dictionary[i])) {
            strcpy(my_results->palindromes[my_results->palindrome_count++], dictionary[i]);
        } else if (is_semordnilap(dictionary[i])) {
            strcpy(my_results->semordnilaps[my_results->semordnilap_count++], dictionary[i]);
        }
    }

    return NULL;
}

// Main thread aggregates after joining
for (int i = 0; i < num_workers; i++) {
    pthread_join(threads[i], NULL);
}

// Aggregate results (sequential)
for (int i = 0; i < num_workers; i++) {
    for (int j = 0; j < worker_results[i].palindrome_count; j++) {
        strcpy(palindromes[total_palindromes++], worker_results[i].palindromes[j]);
    }
    for (int j = 0; j < worker_results[i].semordnilap_count; j++) {
        strcpy(semordnilaps[total_semordnilaps++], worker_results[i].semordnilaps[j]);
    }
}
```

**Pros**: No synchronization during computation, better cache locality
**Cons**: More memory usage (per-worker buffers)

**Recommended**: Strategy 2 for this problem

#### Strategy 3: Bag of Tasks (Unnecessary for Uniform Work)

Since checking each word takes similar time, static partitioning is fine. Bag of tasks adds overhead without benefit.

### Dictionary Lookup Optimization

**Problem**: Each semordnilap check requires dictionary lookup
- 25,143 words to check
- Each needs lookup → O(log n) with binary search
- Total: O(n log n)

**Optimization 1: Hash Table**

```c
#include <search.h>  // POSIX hash table

// Build hash table after loading dictionary
hcreate(word_count * 2);  // Create hash table
for (int i = 0; i < word_count; i++) {
    ENTRY e = {dictionary[i], (void*)1};
    hsearch(e, ENTER);
}

// Lookup in O(1) average
int in_dictionary(const char* word) {
    ENTRY e = {(char*)word, NULL};
    ENTRY* ep = hsearch(e, FIND);
    return (ep != NULL);
}
```

**Speedup**: O(n log n) → O(n)

**Optimization 2: Sorted Dictionary + Binary Search**

```c
// Sort dictionary after loading (qsort)
qsort(dictionary, word_count, sizeof(dictionary[0]), strcmp_wrapper);

// Binary search (already shown above)
```

**Speedup**: O(n²) linear search → O(n log n) binary search

### Avoiding Duplicate Semordnilaps

**Problem**: If "draw" and "ward" both in dictionary:
- Worker processing "draw" finds it's a semordnilap
- Worker processing "ward" also finds it's a semordnilap
- Result: Both "draw↔ward" and "ward↔draw" reported (duplicate pair)

**Solution 1: Only Report if word < reverse (Lexicographic)**

```c
if (is_semordnilap(dictionary[i]) && strcmp(dictionary[i], reversed) < 0) {
    // Only report if this word is lexicographically smaller
    store_semordnilap(dictionary[i]);
}
```

**Solution 2: Post-process to Remove Duplicates**

After aggregation, scan for duplicates and remove.

**Recommended**: Solution 1 (simpler, done during computation)

## Correctness Assessment

### Algorithmic Correctness

**Palindrome Detection**: Correct if word == reverse(word)
- "noon": n-o-o-n → n-o-o-n ✓
- Empty string: ✓ (edge case)
- Single char: ✓ (always palindrome)

**Semordnilap Detection**: Correct if:
1. reverse(word) ≠ word (not a palindrome)
2. reverse(word) exists in dictionary
3. No duplicate pairs reported

### Edge Cases

**Case 1: Single Character Words**
- "a", "I"
- Are palindromes (read same forward/backward)
- Not semordnilaps (reverse = original)

**Case 2: Empty Lines**
- If dictionary has blank lines
- `strlen("")` = 0, is_palindrome("") returns 1
- Should skip or handle

**Case 3: Case Sensitivity**
- If dictionary has "Noon" and "noon"
- Are they different words or same?
- **Assumption**: Dictionary is lowercase (standard for /usr/dict/words)

**Case 4: Non-alphabetic Characters**
- Apostrophes: "can't"
- Hyphens: "re-enter"
- Handle as-is (compare as strings)

### Correctness Issues

**Issue 1: Race Condition in Shared Result Array (Strategy 1)**

```c
// Thread 1
palindromes[palindrome_count++] = ...;  // Read-modify-write

// Thread 2
palindromes[palindrome_count++] = ...;  // Same index!
```

**Fix**: Use mutex or per-worker buffers (Strategy 2)

**Issue 2: Dictionary Modification During Parallel Access**

If dictionary is modified while workers read → data race

**Solution**: Dictionary is read-only after loading → safe to access without synchronization

**Issue 3: Binary Search Requires Sorted Array**

```c
int in_dictionary(const char* word) {
    // Assumes dictionary is sorted!
    // Will fail if not sorted
}
```

**Fix**: Sort after loading:
```c
qsort(dictionary, word_count, sizeof(dictionary[0]), strcmp_wrapper);
```

**Issue 4: Buffer Overflow in Worker Results**

```c
char palindromes[1000][MAX_WORD_LENGTH];  // Fixed size
// If worker finds >1000 palindromes → buffer overflow
```

**Fix**: Either:
- Dynamic allocation (realloc as needed)
- Verify dictionary has <1000 palindromes (sanity check)
- Use linked list or dynamic array

## Performance Considerations

### Workload Analysis

**Per-word Work**:
1. Palindrome check: O(L) where L = word length
2. If not palindrome, semordnilap check:
   - Reverse word: O(L)
   - Dictionary lookup: O(log n) or O(1) with hash table
3. Store result: O(L) for string copy

**Total Sequential Work**: O(n × L × log n) where n = 25,143

**Parallelizable**: Yes, each word independent

### Expected Speedup

**Parameters**: 25,143 words, 4 workers

**Ideal Speedup**: 4x (perfect parallelism)

**Realistic Speedup**: 3.5-3.8x

**Limiting Factors**:
1. **Amdahl's Law**: Input/output phases sequential
   - Input: ~10ms (reading file)
   - Compute: ~100ms (checking words)
   - Output: ~5ms (writing results)
   - Parallel fraction: 100/(10+100+5) ≈ 87%
   - Max speedup: 1 / (0.13 + 0.87/4) ≈ 3.4x

2. **Cache Contention**: Dictionary shared (read-only, good for cache)

3. **False Sharing**: If worker_stats array elements on same cache line
   ```c
   // BAD: All in same cache line
   int worker_palindrome_counts[MAX_WORKERS];

   // GOOD: Pad to separate cache lines
   typedef struct {
       int palindromes;
       int semordnilaps;
       char padding[64 - 2*sizeof(int)];
   } WorkerStats __attribute__((aligned(64)));
   ```

### Performance Measurements

**Expected Results** (4 workers on quad-core):

| Metric | Sequential | Parallel (4 workers) |
|--------|-----------|---------------------|
| Input time | 10 ms | 10 ms (sequential) |
| Compute time | 100 ms | 28 ms (3.5x speedup) |
| Output time | 5 ms | 5 ms (sequential) |
| **Total** | **115 ms** | **43 ms (2.7x overall)** |

**Efficiency**: 2.7/4 = 67.5% (good, considering sequential phases)

### Optimization Opportunities

1. **Hash Table for Dictionary**: O(log n) → O(1) lookup
2. **String Length Cache**: Store lengths to avoid repeated strlen()
3. **Early Exit in Palindrome Check**: Return false on first mismatch
4. **SIMD for String Comparison**: Vectorize character comparisons
5. **Reduce String Copying**: Use pointers instead of strcpy where possible

## Synchronization and Concurrency Issues

### Issue 1: Shared Dictionary (Read-Only)

**Analysis**:
- Multiple threads read dictionary simultaneously
- No writes to dictionary during parallel phase
- **Thread-safe**: Read-only data needs no synchronization

**Caveat**: Must ensure dictionary fully initialized before parallel phase starts

### Issue 2: Result Storage Synchronization

**Approach A: Shared Result Array with Mutex**

```c
pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_lock(&result_mutex);
palindromes[palindrome_count++] = ...;
pthread_mutex_unlock(&result_mutex);
```

**Bottleneck**: All threads serialize on result_mutex
- If finding result takes 1μs but mutex wait is 0.5μs → 50% overhead
- With 4 threads competing → worse

**Approach B: Per-Worker Buffers (Recommended)**

No synchronization during compute phase:
```c
worker_results[thread_id].palindromes[local_count++] = ...;
```

**Aggregation in Sequential Phase**: No contention

**Winner**: Approach B significantly faster

### Issue 3: Worker Statistics

**Shared Array**:
```c
typedef struct {
    int palindromes;
    int semordnilaps;
} WorkerStats;

WorkerStats worker_stats[MAX_WORKERS];

// In worker thread
worker_stats[thread_id].palindromes++;  // Write to own slot
```

**Thread-Safety**:
- Each worker writes only to `worker_stats[thread_id]`
- No overlap between workers
- **Safe without mutex** (different memory locations)

**False Sharing Risk**:
- If `worker_stats[0]` and `worker_stats[1]` on same cache line
- Thread 0 writes → invalidates cache line for Thread 1
- Performance degradation, not correctness issue

**Fix**: Padding or alignment
```c
typedef struct {
    int palindromes;
    int semordnilaps;
    char padding[56];  // Total 64 bytes (cache line)
} WorkerStats __attribute__((aligned(64)));
```

### Issue 4: Thread Creation and Joining

**Correct Pattern**:
```c
// Start timing AFTER loading dictionary, BEFORE creating threads
gettimeofday(&start_time, NULL);

for (int i = 0; i < num_workers; i++) {
    pthread_create(&threads[i], NULL, worker, &args[i]);
}

for (int i = 0; i < num_workers; i++) {
    pthread_join(threads[i], NULL);
}

gettimeofday(&end_time, NULL);
// Only parallel computation timed, not I/O
```

**Wrong**: Including file I/O in timing
```c
gettimeofday(&start_time, NULL);
// Load dictionary... (sequential I/O)
// Create threads...
```

### Issue 5: qsort During Parallel Phase

**If sorting dictionary for binary search**:

```c
// WRONG: Sort after threads created
pthread_create(...);
qsort(dictionary, ...);  // Modifying data while threads reading!
```

**Correct**: Sort before creating threads
```c
// Load dictionary
qsort(dictionary, word_count, ...);  // Sort first
// NOW create threads
pthread_create(...);
```

## Recommendations and Improvements

### Recommended Implementation

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_WORDS 30000
#define MAX_WORD_LENGTH 50
#define MAX_WORKERS 16
#define MAX_RESULTS_PER_WORKER 2000

// Global dictionary (read-only during parallel phase)
char dictionary[MAX_WORDS][MAX_WORD_LENGTH];
int word_count = 0;

// Per-worker result buffers
typedef struct {
    char palindromes[MAX_RESULTS_PER_WORKER][MAX_WORD_LENGTH];
    int palindrome_count;
    char semordnilaps[MAX_RESULTS_PER_WORKER][MAX_WORD_LENGTH];
    int semordnilap_count;
    char padding[64];  // Prevent false sharing
} WorkerResults __attribute__((aligned(64)));

WorkerResults worker_results[MAX_WORKERS];

typedef struct {
    int thread_id;
    int start_index;
    int end_index;
} WorkerArgs;

int is_palindrome(const char* word) {
    int len = strlen(word);
    for (int i = 0; i < len / 2; i++) {
        if (word[i] != word[len - 1 - i]) {
            return 0;
        }
    }
    return 1;
}

void reverse_word(const char* word, char* reversed) {
    int len = strlen(word);
    for (int i = 0; i < len; i++) {
        reversed[i] = word[len - 1 - i];
    }
    reversed[len] = '\0';
}

int in_dictionary(const char* word) {
    int left = 0, right = word_count - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = strcmp(word, dictionary[mid]);
        if (cmp == 0) return 1;
        if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
    return 0;
}

void* worker(void* arg) {
    WorkerArgs* args = (WorkerArgs*)arg;
    WorkerResults* results = &worker_results[args->thread_id];
    results->palindrome_count = 0;
    results->semordnilap_count = 0;

    for (int i = args->start_index; i < args->end_index; i++) {
        const char* word = dictionary[i];

        if (is_palindrome(word)) {
            strcpy(results->palindromes[results->palindrome_count++], word);
        } else {
            char reversed[MAX_WORD_LENGTH];
            reverse_word(word, reversed);

            // Only report if word < reversed (avoid duplicates)
            if (in_dictionary(reversed) && strcmp(word, reversed) < 0) {
                strcpy(results->semordnilaps[results->semordnilap_count++], word);
            }
        }
    }

    return NULL;
}

int strcmp_wrapper(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s num_workers\n", argv[0]);
        return 1;
    }

    int num_workers = atoi(argv[1]);
    if (num_workers < 1 || num_workers > MAX_WORKERS) {
        fprintf(stderr, "Invalid number of workers\n");
        return 1;
    }

    // PHASE 1: INPUT (Sequential)
    FILE* f = fopen("words", "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    while (fgets(dictionary[word_count], MAX_WORD_LENGTH, f) != NULL) {
        dictionary[word_count][strcspn(dictionary[word_count], "\n")] = '\0';
        if (strlen(dictionary[word_count]) > 0) {  // Skip empty lines
            word_count++;
        }
    }
    fclose(f);

    // Sort for binary search
    qsort(dictionary, word_count, sizeof(dictionary[0]), strcmp_wrapper);

    // PHASE 2: COMPUTE (Parallel)
    pthread_t threads[MAX_WORKERS];
    WorkerArgs args[MAX_WORKERS];

    struct timeval start, end;
    gettimeofday(&start, NULL);

    int words_per_worker = word_count / num_workers;
    for (int i = 0; i < num_workers; i++) {
        args[i].thread_id = i;
        args[i].start_index = i * words_per_worker;
        args[i].end_index = (i == num_workers - 1) ? word_count : (i + 1) * words_per_worker;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < num_workers; i++) {
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    // PHASE 3: OUTPUT (Sequential)
    FILE* output = fopen("results.txt", "w");

    int total_palindromes = 0;
    int total_semordnilaps = 0;

    // Write results
    fprintf(output, "PALINDROMES:\n");
    for (int i = 0; i < num_workers; i++) {
        for (int j = 0; j < worker_results[i].palindrome_count; j++) {
            fprintf(output, "%s\n", worker_results[i].palindromes[j]);
            total_palindromes++;
        }
    }

    fprintf(output, "\nSEMORDNILAPS:\n");
    for (int i = 0; i < num_workers; i++) {
        for (int j = 0; j < worker_results[i].semordnilap_count; j++) {
            fprintf(output, "%s\n", worker_results[i].semordnilaps[j]);
            total_semordnilaps++;
        }
    }

    fclose(output);

    // Print statistics
    printf("Total palindromes: %d\n", total_palindromes);
    printf("Total semordnilaps: %d\n", total_semordnilaps);
    printf("\nPer-worker statistics:\n");
    for (int i = 0; i < num_workers; i++) {
        printf("Worker %d: %d palindromes, %d semordnilaps\n",
               i, worker_results[i].palindrome_count,
               worker_results[i].semordnilap_count);
    }
    printf("\nExecution time: %.6f seconds\n", elapsed);

    return 0;
}
```

### Testing Strategy

1. **Small Dictionary Test**:
   ```bash
   echo -e "noon\nradar\ndraw\nward\nhello" > test_words
   ./palindrome 2
   # Expected: noon, radar (palindromes), draw (semordnilap)
   ```

2. **Edge Cases**:
   ```bash
   echo -e "a\n\nab\nba" > test_words
   # Test single char, empty line, simple semordnilap
   ```

3. **Full Dictionary**:
   ```bash
   ./palindrome 4
   # Verify results.txt contains expected palindromes/semordnilaps
   ```

4. **Scalability Test**:
   ```bash
   for w in 1 2 4 8; do
       echo "Workers: $w"
       time ./palindrome $w
   done
   # Measure speedup
   ```

5. **Verification**:
   ```bash
   # Manual check of sample results
   grep "noon" results.txt  # Should be in palindromes section
   ```

### Known Palindromes in English

Examples to verify (likely in /usr/dict/words):
- noon, radar, level, civic, rotor, kayak, deed, refer
- Single letters: a, i

### Known Semordnilaps

Examples:
- desserts ↔ stressed
- drawer ↔ reward
- evil ↔ live
- star ↔ rats
- stop ↔ pots

## Conclusion

**Task**: Find palindromes and semordnilaps in 25,143-word dictionary using parallel workers

**Current Status**: Not implemented (only template and dictionary file exist)

**Recommended Approach**:
- Static partitioning with per-worker result buffers
- Binary search for dictionary lookup
- Avoid duplicates by lexicographic ordering

**Key Challenges**:
1. Efficient dictionary lookup (binary search requires sorted array)
2. Avoiding duplicate semordnilap pairs
3. Per-worker result aggregation
4. False sharing prevention in statistics

**Complexity**: Medium-High
- Requires string manipulation (reverse, compare)
- Dictionary data structure (sorted array or hash table)
- Result aggregation from multiple workers
- Performance measurement and statistics

**Expected Performance**:
- **Speedup**: 3-3.5x with 4 workers
- **Bottleneck**: Sequential I/O phases
- **Efficiency**: ~75-85%

**Learning Objectives**:
1. Static work partitioning for uniform tasks
2. Read-only shared data (no synchronization needed)
3. Per-worker result buffers to avoid contention
4. False sharing and cache line alignment
5. Amdahl's Law in practice (sequential I/O limits speedup)

**Estimated Execution Time**: 20-50ms parallel phase (sequential: 80-150ms)

**Memory Usage**: ~5MB for dictionary + worker buffers (reasonable)
