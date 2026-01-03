# Question 3: Find Palindromes and Semordnilaps - Analysis

## Summary of Task
Develop a parallel multithreaded program using OpenMP to find:
1. **Palindromes**: Words that read the same forward and backward (e.g., "noon", "radar")
2. **Semordnilaps**: Words whose reverse forms are different valid words (e.g., "draw" ↔ "ward")

**Requirements:**
- Process a dictionary file (/usr/share/dict/words or custom file)
- Use OpenMP for-loop parallelism or tasks
- Number of threads as command-line argument
- Sequential I/O (input/output phases), parallel processing
- Write results to an output file
- Benchmark on different thread counts (up to 4+) and dictionary sizes (at least 3)
- Report speedup using median of 5+ runs
- Use `omp_get_wtime()` for timing

## Analysis of Current Implementation

### Implementation Status: NOT IMPLEMENTED

The Question_3 directory contains only a README.md file. **No code implementation exists.**

## Required Implementation Approach

### Algorithm Design

**High-Level Strategy:**
1. **Sequential Phase**: Read dictionary into memory (array of strings)
2. **Parallel Phase**: For each word, check if it's a palindrome or semordnilap
3. **Sequential Phase**: Write results to file

**Key Decisions:**

1. **Data Structure for Dictionary:**
   - Array of strings for linear scan: O(n) lookup
   - Hash table for fast lookup: O(1) average case
   - **Recommendation**: Hash table (e.g., using separate chaining or open addressing)

2. **Parallelization Strategy:**
   - **Option A**: Parallel for loop over dictionary words
   - **Option B**: Task-based parallelism
   - **Recommendation**: Parallel for loop (simpler, well-suited for uniform work)

3. **Result Collection:**
   - **Challenge**: Multiple threads may find results concurrently
   - **Options**:
     - Critical section (serialization bottleneck)
     - Thread-local lists, merge later (better performance)
     - Preallocated result arrays with atomic counters

### Recommended Implementation Structure

```c
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORD_LEN 100
#define MAX_WORDS 500000
#define HASH_SIZE 1000000  // Prime number for better distribution

// Hash table node
typedef struct HashNode {
    char *word;
    struct HashNode *next;
} HashNode;

// Hash table
HashNode *hash_table[HASH_SIZE];

// Simple hash function (djb2)
unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % HASH_SIZE;
}

// Insert word into hash table (sequential, during loading)
void hash_insert(const char *word) {
    unsigned long idx = hash(word);
    HashNode *node = malloc(sizeof(HashNode));
    node->word = strdup(word);
    node->next = hash_table[idx];
    hash_table[idx] = node;
}

// Check if word exists in hash table (thread-safe for read-only)
int hash_contains(const char *word) {
    unsigned long idx = hash(word);
    HashNode *node = hash_table[idx];
    while (node) {
        if (strcmp(node->word, word) == 0)
            return 1;
        node = node->next;
    }
    return 0;
}

// Reverse a string
void reverse_string(const char *src, char *dst) {
    int len = strlen(src);
    for (int i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
    dst[len] = '\0';
}

// Check if word is palindrome
int is_palindrome(const char *word) {
    int len = strlen(word);
    for (int i = 0; i < len / 2; i++) {
        if (word[i] != word[len - 1 - i])
            return 0;
    }
    return 1;
}

// Check if word is semordnilap
int is_semordnilap(const char *word) {
    char reversed[MAX_WORD_LEN];
    reverse_string(word, reversed);

    // Semordnilap must be different from original and exist in dictionary
    return strcmp(word, reversed) != 0 && hash_contains(reversed);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <dict_file> <num_threads>\n", argv[0]);
        return 1;
    }

    char *dict_file = argv[1];
    int num_threads = atoi(argv[2]);
    omp_set_num_threads(num_threads);

    // PHASE 1: Load dictionary (SEQUENTIAL)
    printf("Loading dictionary...\n");
    FILE *fp = fopen(dict_file, "r");
    if (!fp) {
        perror("Failed to open dictionary");
        return 1;
    }

    char **words = malloc(MAX_WORDS * sizeof(char*));
    int word_count = 0;
    char buffer[MAX_WORD_LEN];

    while (fgets(buffer, MAX_WORD_LEN, fp) && word_count < MAX_WORDS) {
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;

        // Convert to lowercase
        for (int i = 0; buffer[i]; i++)
            buffer[i] = tolower(buffer[i]);

        words[word_count] = strdup(buffer);
        hash_insert(buffer);
        word_count++;
    }
    fclose(fp);
    printf("Loaded %d words\n", word_count);

    // PHASE 2: Find palindromes and semordnilaps (PARALLEL)
    // Use thread-local lists for results
    char ***palindrome_lists = malloc(num_threads * sizeof(char**));
    char ***semordnilap_lists = malloc(num_threads * sizeof(char**));
    int *palindrome_counts = calloc(num_threads, sizeof(int));
    int *semordnilap_counts = calloc(num_threads, sizeof(int));

    for (int t = 0; t < num_threads; t++) {
        palindrome_lists[t] = malloc(MAX_WORDS * sizeof(char*));
        semordnilap_lists[t] = malloc(MAX_WORDS * sizeof(char*));
    }

    printf("Processing words...\n");
    double start = omp_get_wtime();

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();

        #pragma omp for schedule(dynamic, 100)
        for (int i = 0; i < word_count; i++) {
            if (strlen(words[i]) < 2) continue;  // Skip single letters

            if (is_palindrome(words[i])) {
                palindrome_lists[tid][palindrome_counts[tid]++] = words[i];
            }
            else if (is_semordnilap(words[i])) {
                // Avoid duplicates: only include if word < reversed lexicographically
                char reversed[MAX_WORD_LEN];
                reverse_string(words[i], reversed);
                if (strcmp(words[i], reversed) < 0) {
                    semordnilap_lists[tid][semordnilap_counts[tid]++] = words[i];
                }
            }
        }
    }

    double end = omp_get_wtime();
    printf("Processing took %g seconds\n", end - start);

    // PHASE 3: Write results (SEQUENTIAL)
    printf("Writing results...\n");
    FILE *out = fopen("results.txt", "w");
    if (!out) {
        perror("Failed to open output file");
        return 1;
    }

    fprintf(out, "PALINDROMES:\n");
    int total_palindromes = 0;
    for (int t = 0; t < num_threads; t++) {
        for (int i = 0; i < palindrome_counts[t]; i++) {
            fprintf(out, "%s\n", palindrome_lists[t][i]);
            total_palindromes++;
        }
    }

    fprintf(out, "\nSEMORDNILAPS:\n");
    int total_semordnilaps = 0;
    for (int t = 0; t < num_threads; t++) {
        for (int i = 0; i < semordnilap_counts[t]; i++) {
            char reversed[MAX_WORD_LEN];
            reverse_string(semordnilap_lists[t][i], reversed);
            fprintf(out, "%s <-> %s\n", semordnilap_lists[t][i], reversed);
            total_semordnilaps++;
        }
    }
    fclose(out);

    printf("Found %d palindromes and %d semordnilap pairs\n",
           total_palindromes, total_semordnilaps);

    // Cleanup
    for (int i = 0; i < word_count; i++) free(words[i]);
    free(words);
    // ... (free hash table, result lists, etc.)

    return 0;
}
```

## Correctness Considerations

### Critical Synchronization Issues

1. **Hash Table Access:**
   - **Building phase**: Sequential (no issues)
   - **Lookup phase**: Read-only access (thread-safe without locks)
   - **No race conditions** as long as hash table is not modified during parallel phase

2. **Result Collection Race Condition:**

   **INCORRECT Approach (Race Condition):**
   ```c
   // Global arrays
   char *palindromes[MAX_WORDS];
   int palindrome_count = 0;

   #pragma omp parallel for
   for (i = 0; i < word_count; i++) {
       if (is_palindrome(words[i])) {
           palindromes[palindrome_count++] = words[i];  // RACE!
       }
   }
   ```
   - Multiple threads increment `palindrome_count` simultaneously
   - Leads to lost updates and array corruption

   **INCORRECT Approach (Performance Killer):**
   ```c
   #pragma omp parallel for
   for (i = 0; i < word_count; i++) {
       if (is_palindrome(words[i])) {
           #pragma omp critical
           {
               palindromes[palindrome_count++] = words[i];
           }
       }
   }
   ```
   - Serializes all palindrome additions
   - Negates parallelism benefits

   **CORRECT Approach (Thread-Local Lists):**
   - Each thread maintains its own result list
   - No synchronization needed during parallel phase
   - Merge lists sequentially after parallel region

3. **Semordnilap Duplicate Prevention:**
   - "draw" and "ward" are both in dictionary
   - Without deduplication, both pairs would be reported
   - **Solution**: Only report pair where word < reversed lexicographically
   - This ensures each pair appears exactly once

### Edge Cases to Handle

1. **Single-letter words**: Not meaningful palindromes, should skip
2. **Empty strings**: Handle gracefully
3. **Case sensitivity**: Convert to lowercase for comparison
4. **Whitespace**: Trim trailing newlines/spaces
5. **Punctuation**: Decide whether to include (typically exclude)
6. **Self-palindromes**: "noon" is both palindrome and semordnilap (reversed is in dictionary) - classify as palindrome only

## Performance Considerations

### Expected Performance Characteristics

**Computational Complexity:**
- **Total work**: O(n × m) where n = word count, m = average word length
  - For each word: palindrome check O(m), reverse O(m), hash lookup O(1) average
- **Well-suited for parallelization**: Independent iterations

**Performance Bottlenecks:**

1. **Hash Table Lookups:**
   - Critical for semordnilap detection
   - With good hash function: O(1) average
   - Poor hash function → collisions → O(n) worst case
   - Cache effects: hash table may be large, poor locality

2. **String Operations:**
   - `strcmp`, `strlen`, `strcpy` dominate runtime
   - Memory-bound operations
   - Limited by memory bandwidth on many cores

3. **Load Balancing:**
   - Different word lengths → variable work per iteration
   - Dynamic scheduling recommended over static
   - Chunk size tuning important

**Expected Speedup:**

For 25,000 words dictionary:
- **1 thread**: Baseline
- **2 threads**: 1.7-1.9x speedup (85-95% efficiency)
- **4 threads**: 3.0-3.5x speedup (75-87% efficiency)
- **8 threads**: 4.5-6.0x speedup (56-75% efficiency)

**Why Good Scalability?**
- Independent iterations (no dependencies)
- Read-only hash table access (no contention)
- Thread-local result collection (no synchronization)
- Computation > overhead for reasonable dictionary sizes

**Limiting Factors:**
1. **Sequential I/O**: File reading/writing not parallelized (Amdahl's law)
2. **Memory bandwidth**: Multiple threads accessing large hash table
3. **Cache coherence**: Hash table lookups cause cache misses
4. **NUMA effects**: On multi-socket systems, memory locality matters

### Optimization Strategies

1. **Scheduling Policy:**
   ```c
   #pragma omp for schedule(dynamic, 100)
   ```
   - Dynamic: better load balancing for variable word lengths
   - Chunk size 100: balance between overhead and distribution
   - Test: static vs dynamic vs guided

2. **Hash Table Optimization:**
   - Use prime number for hash table size (better distribution)
   - Good hash function (djb2, FNV-1a, MurmurHash)
   - Consider perfect hashing for static dictionary
   - Cache-friendly hash table layout

3. **String Comparison Optimization:**
   - Store word lengths to skip length comparison
   - Early exit in palindrome check
   - SIMD string operations (compiler auto-vectorization)

4. **Memory Allocation:**
   - Preallocate result arrays (avoid malloc in parallel region)
   - Use memory pools for hash table nodes
   - Align data structures for cache lines

5. **Reduce String Copying:**
   - Store pointers to words instead of copying
   - Avoid unnecessary `strdup` operations

### Scalability Analysis

**Amdahl's Law Impact:**

Assume:
- I/O: 10% of total time
- Processing: 90% of total time (parallelizable)

Maximum theoretical speedup on infinite cores:
- Speedup_max = 1 / (0.1 + 0.9/∞) = 10x

Realistic speedup on 4 cores:
- Speedup_4 = 1 / (0.1 + 0.9/4) = 3.08x

Efficiency = 3.08/4 = 77%

**Better for larger dictionaries:**
- I/O overhead becomes smaller percentage
- More work to amortize task overhead
- Better parallel efficiency

## Synchronization and Concurrency Analysis

### Thread Safety Assessment

**Thread-Safe Components:**
1. Hash table lookups (read-only during parallel phase)
2. Input word array (read-only during parallel phase)
3. Thread-local result lists (no sharing)

**Not Thread-Safe (Handled Correctly):**
1. File I/O (sequential phases only)
2. Hash table construction (sequential)
3. Result merging (sequential)

**No Synchronization Primitives Needed:**
- No locks, critical sections, or atomic operations required in parallel region
- Thread-local storage pattern eliminates need for synchronization
- This is the key to good performance

### Data Race Analysis

**Potential Data Races (If Implemented Incorrectly):**

1. **Global counter increment**:
   ```c
   global_count++;  // RACE if accessed by multiple threads
   ```

2. **Shared result array**:
   ```c
   results[index++] = word;  // RACE on both array and index
   ```

3. **Hash table modification during parallel phase**:
   ```c
   hash_insert(word);  // RACE on hash table structure
   ```

**Correct Implementation (No Data Races):**
- Const/read-only data in parallel region
- Thread-local variables for mutable state
- No shared mutable state

## Testing and Validation

### Correctness Verification

1. **Known Results Test:**
   - Small dictionary with known palindromes: "noon", "radar", "level"
   - Known semordnilaps: "draw"↔"ward", "evil"↔"live", "stressed"↔"desserts"
   - Verify all are found

2. **Result Consistency:**
   - Run with 1, 2, 4 threads
   - Results should be identical (same set of words)
   - Order may differ (acceptable)

3. **Duplicate Check:**
   - No duplicate palindromes
   - No duplicate semordnilap pairs
   - Each semordnilap pair appears exactly once

4. **Edge Cases:**
   - Empty dictionary
   - Dictionary with only single letters
   - Dictionary with no palindromes/semordnilaps
   - Very long words (> MAX_WORD_LEN)

### Performance Testing

**Test Configurations:**

Dictionary sizes:
1. Small: 1,000 words (custom subset)
2. Medium: 25,143 words (provided words file)
3. Large: 235,886 words (/usr/share/dict/words)

Thread counts: 1, 2, 4, 8

**Timing Methodology:**
- Exclude I/O from timing (or report separately)
- Run 5+ times per configuration
- Report median time
- Calculate speedup and efficiency

**Expected Observation:**
- Larger dictionaries → better speedup (more work to amortize overhead)
- Diminishing returns beyond physical core count
- Dynamic scheduling performs better than static for variable-length words

## Recommendations

### Implementation Priorities

1. **Phase 1**: Sequential version
   - Load dictionary
   - Build hash table
   - Sequential search for palindromes/semordnilaps
   - Verify correctness on small dataset

2. **Phase 2**: Basic parallelization
   - Add `#pragma omp parallel for`
   - Implement thread-local result lists
   - Merge results
   - Verify results match sequential version

3. **Phase 3**: Optimization
   - Tune scheduling (static/dynamic/guided)
   - Optimize chunk size
   - Improve hash function
   - Add performance measurements

4. **Phase 4**: Benchmarking
   - Systematic testing across configurations
   - Statistical analysis (median, variance)
   - Speedup and efficiency plots
   - Result interpretation

### Code Quality Recommendations

1. **Error Handling:**
   - Check file open failures
   - Validate command-line arguments
   - Handle malloc failures
   - Bounds checking for arrays

2. **Memory Management:**
   - Free all allocated memory
   - Avoid memory leaks
   - Use valgrind for leak detection

3. **Compilation:**
   ```bash
   gcc -O3 -fopenmp -o palindrome palindrome.c
   ```

4. **Documentation:**
   - Comment parallelization strategy
   - Explain deduplication logic
   - Document expected performance

### Alternative Approaches

1. **Bloom Filter Pre-filtering:**
   - Use Bloom filter for fast "definitely not in dictionary" check
   - Reduces hash table lookups
   - Space-time tradeoff

2. **Trie-based Dictionary:**
   - Faster lookups than hash table
   - Cache-friendly prefix sharing
   - More complex implementation

3. **Task-based Parallelism:**
   - Divide dictionary into chunks
   - Create task for each chunk
   - Similar performance to parallel for

## Conclusion

**Implementation Status:** NOT IMPLEMENTED - requires complete development.

**Algorithm Suitability:** This problem is excellently suited for parallelization:
- Independent iterations (embarrassingly parallel)
- Read-only data sharing (no contention)
- Uniform work distribution (with dynamic scheduling)

**Expected Performance:** Very good scalability expected:
- 3-3.5x speedup on 4 cores for medium-to-large dictionaries
- Efficiency of 75-87% achievable
- Limited by memory bandwidth and Amdahl's law (sequential I/O)

**Critical Success Factors:**
1. Correct handling of result collection (thread-local lists)
2. Efficient hash table implementation
3. Proper deduplication of semordnilap pairs
4. Dynamic scheduling for load balancing
5. Comprehensive correctness testing

**Key Advantages of This Problem:**
- No complex synchronization needed
- Good learning example for OpenMP parallel for
- Clear performance benefits from parallelization
- Real-world application (dictionary processing)
