# Question 5 Analysis: Parallel Implementation of Simplified Linux diff Command

## Summary of the Question/Task

Develop a parallel multithreaded program (C/Pthreads or Java) implementing a simplified version of the Linux `diff` command:

**Command**: `diff filename1 filename2`

**Functionality**:
- Compare corresponding lines in two text files
- Print both lines to stdout if they differ
- If one file is longer, print all extra lines from the longer file

**Parallel Requirement**: Use multiple threads for the comparison operation

## Current Implementation Status

**Status**: NOT IMPLEMENTED

The Question_5 directory contains only a README.md file. No code implementation exists.

## Analysis of Required Implementation Approach

### Sequential diff Logic

```c
FILE* f1 = fopen(filename1, "r");
FILE* f2 = fopen(filename2, "r");

char line1[MAX_LINE];
char line2[MAX_LINE];
int line_num = 1;

while (1) {
    char* r1 = fgets(line1, MAX_LINE, f1);
    char* r2 = fgets(line2, MAX_LINE, f2);

    if (r1 == NULL && r2 == NULL) break;  // Both files ended

    if (r1 == NULL) {
        // File 1 ended, print rest of file 2
        printf("Extra in %s: %s", filename2, line2);
        while (fgets(line2, MAX_LINE, f2)) {
            printf("Extra in %s: %s", filename2, line2);
        }
        break;
    }

    if (r2 == NULL) {
        // File 2 ended, print rest of file 1
        printf("Extra in %s: %s", filename1, line1);
        while (fgets(line1, MAX_LINE, f1)) {
            printf("Extra in %s: %s", filename1, line1);
        }
        break;
    }

    if (strcmp(line1, line2) != 0) {
        printf("Line %d differs:\n", line_num);
        printf("  %s: %s", filename1, line1);
        printf("  %s: %s", filename2, line2);
    }

    line_num++;
}
```

### Parallelization Strategies

#### Strategy 1: Parallel File Reading (Pipeline)

```
Reader Thread 1 → [Queue 1] ↘
                              Comparator Thread → stdout
Reader Thread 2 → [Queue 2] ↗
```

**Architecture**:
- Thread 1: Reads file1 line-by-line into queue1
- Thread 2: Reads file2 line-by-line into queue2
- Thread 3: Dequeues pairs of lines, compares, prints differences

**Synchronization**: Two producer-consumer queues

#### Strategy 2: Batch Comparison

```
Main Thread: Load files into memory
    ↓
Create N worker threads, each compares 1/N of lines
    ↓
Workers write differences to per-thread buffers
    ↓
Main thread collects and prints results in order
```

**Suitable for**: Large files that fit in memory

#### Strategy 3: Two Readers + One Writer (Recommended for this problem)

```
[File1] → Reader Thread 1 → [Line Queue 1] ↘
                                             → Comparator → [Output Queue] → Writer Thread → stdout
[File2] → Reader Thread 2 → [Line Queue 2] ↗
```

**Four threads**:
1. Read file1 into queue
2. Read file2 into queue
3. Compare lines from both queues
4. Print differences (maintains order)

#### Strategy 4: Simpler Two-Thread Approach

```
Reader Thread 1: Read both files alternately, compare, enqueue differences
Writer Thread 2: Dequeue and print differences
```

**Simplest parallelization** that still uses multiple threads

## Correctness Assessment

### Functional Requirements

1. **Line-by-line comparison**: Corresponding line numbers must match
   - Line 1 of file1 vs line 1 of file2
   - Line 2 of file1 vs line 2 of file2
   - etc.

2. **Difference reporting**: Print both lines when they differ

3. **Extra lines**: Print all remaining lines from longer file

4. **Ordering**: Output must reflect file order (line 1 diffs before line 2)

### Correctness Challenges

**Challenge 1: Synchronizing Line Numbers**

If threads read independently at different speeds:
```
Reader 1: Read lines 1, 2, 3
Reader 2: Read lines 1, 2 (slower)
Comparator: Compare line 1 vs line 1 ✓
            Compare line 2 vs line 2 ✓
            Compare line 3 vs ??? (line 3 not ready)
```

**Solution**: Comparator must wait for both lines to be available

```c
while (1) {
    // Wait for line from file1
    pthread_mutex_lock(&queue1_mutex);
    while (queue1_empty && !file1_eof) {
        pthread_cond_wait(&queue1_not_empty, &queue1_mutex);
    }
    if (file1_eof && queue1_empty) {
        pthread_mutex_unlock(&queue1_mutex);
        // Handle file1 ended
        break;
    }
    Line line1 = dequeue_line1();
    pthread_mutex_unlock(&queue1_mutex);

    // Wait for line from file2
    pthread_mutex_lock(&queue2_mutex);
    while (queue2_empty && !file2_eof) {
        pthread_cond_wait(&queue2_not_empty, &queue2_mutex);
    }
    if (file2_eof && queue2_empty) {
        pthread_mutex_unlock(&queue2_mutex);
        // Handle file2 ended
        break;
    }
    Line line2 = dequeue_line2();
    pthread_mutex_unlock(&queue2_mutex);

    // Compare
    if (strcmp(line1.data, line2.data) != 0) {
        printf("Line %d differs...", line_num);
    }
}
```

**Challenge 2: EOF Handling**

Three cases:
1. Both files end at same time
2. File1 ends first (file2 has extra lines)
3. File2 ends first (file1 has extra lines)

**Correct Handling**:
```c
if (file1_eof && !file2_eof) {
    // Print all remaining lines from file2
    while (dequeue_line2(&line2)) {
        printf("Extra in file2: %s", line2.data);
    }
}

if (file2_eof && !file1_eof) {
    // Print all remaining lines from file1
    while (dequeue_line1(&line1)) {
        printf("Extra in file1: %s", line1.data);
    }
}
```

**Challenge 3: Output Ordering**

If using parallel comparison of chunks, must preserve line order:

```c
// WRONG: Parallel comparison without ordering
Thread 1 compares lines 1-100
Thread 2 compares lines 101-200
// Thread 2 might print before Thread 1 finishes
```

**Solution**: Either:
- Sequential comparison (comparator is single thread)
- Buffered output with sequence numbers
- Output queue that enforces ordering

### Potential Bugs

**Bug 1: Race Condition in Line Number**

```c
int line_num = 0;  // Shared variable

// Thread reads line
line_num++;  // NOT ATOMIC - race condition
printf("Line %d: ...", line_num);
```

**Fix**: Line numbers managed by comparator thread (single writer)

**Bug 2: Deadlock with Two Queues**

```c
// Comparator thread
pthread_mutex_lock(&queue1_mutex);
// ... interrupted here ...
pthread_mutex_lock(&queue2_mutex);
```

If another thread locks in opposite order → deadlock

**Fix**: Always lock in same order, or use single lock for both queues

**Bug 3: Missed EOF Signal**

```c
// Reader thread
while (fgets(line, MAX_LINE, file)) {
    enqueue(line);
}
// Forgot to signal EOF!
// Comparator waits forever
```

**Fix**: Explicit EOF signaling

```c
file1_eof = 1;
pthread_cond_broadcast(&queue1_not_empty);
```

## Performance Considerations

### When Does Parallelization Help?

**Scenario 1: Large Files**
- Files are millions of lines
- Reading from slow storage (network, HDD)
- **Benefit**: Overlapping I/O - while comparing, next lines being read

**Scenario 2: Slow Comparison**
- If comparison involves complex processing (regex, parsing)
- Multiple cores can process different line pairs
- **Benefit**: Parallel computation

**Scenario 3: Asymmetric I/O**
- file1 on fast SSD, file2 on slow network mount
- **Benefit**: Read from both concurrently, don't wait for slower

### When Parallelization Doesn't Help

**Scenario 1: Small Files**
- Thread creation overhead > execution time
- Example: 10-line files
- **Result**: Slower than sequential

**Scenario 2: I/O Bound**
- Single disk → two readers contend for I/O
- Sequential reading faster (no seek overhead)
- **Result**: No speedup, possible slowdown

**Scenario 3: Simple Comparison**
- strcmp() is extremely fast (nanoseconds)
- I/O dominates (microseconds-milliseconds per read)
- **Result**: Parallelizing comparison adds overhead with no gain

### Performance Analysis

**Sequential Implementation**:
- Time = O(n) where n = max(lines in file1, lines in file2)
- Dominated by I/O time (fgets is slow)

**Parallel Implementation** (2 readers + 1 comparator):
- Reading happens concurrently
- **Best case**: Time ≈ O(n) but with lower constant (overlap I/O)
- **Worst case**: Same as sequential (I/O serialized on single disk)

**Expected Speedup**: 1.0-1.5x (modest improvement)

**Why Limited Speedup?**
1. **Amdahl's Law**: Comparison is tiny fraction of total time
2. **I/O Serialization**: Disk can only serve one request at a time
3. **Synchronization Overhead**: Queue operations add latency

### Optimization Opportunities

1. **Buffered I/O**: Read large blocks instead of line-by-line
   ```c
   setvbuf(file, NULL, _IOFBF, 64*1024);  // 64KB buffer
   ```

2. **Memory Mapping**: Use mmap() for large files
   ```c
   char* file1_data = mmap(...);
   // Scan for newlines in parallel
   ```

3. **Reduce Synchronization**: Use lock-free queues or atomic operations

4. **Batch Processing**: Compare multiple lines per dequeue operation

## Synchronization and Concurrency Issues

### Issue 1: Producer-Consumer Pattern (Two Queues)

**Components**:
- Producer 1: Reader for file1
- Producer 2: Reader for file2
- Consumer: Comparator thread

**Synchronization Primitives**:
```c
typedef struct {
    char lines[QUEUE_SIZE][MAX_LINE];
    int head, tail, count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int eof;
} LineQueue;

LineQueue queue1, queue2;
```

**Producer Pattern** (Reader):
```c
void* reader_thread(void* arg) {
    ReaderArgs* args = (ReaderArgs*)arg;
    FILE* file = fopen(args->filename, "r");
    LineQueue* queue = args->queue;

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file) != NULL) {
        pthread_mutex_lock(&queue->mutex);
        while (queue->count == QUEUE_SIZE) {
            pthread_cond_wait(&queue->not_full, &queue->mutex);
        }

        strcpy(queue->lines[queue->tail], line);
        queue->tail = (queue->tail + 1) % QUEUE_SIZE;
        queue->count++;

        pthread_cond_signal(&queue->not_empty);
        pthread_mutex_unlock(&queue->mutex);
    }

    // Signal EOF
    pthread_mutex_lock(&queue->mutex);
    queue->eof = 1;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    fclose(file);
    return NULL;
}
```

**Consumer Pattern** (Comparator):
```c
void* comparator_thread(void* arg) {
    char line1[MAX_LINE], line2[MAX_LINE];
    int line_num = 1;

    while (1) {
        // Dequeue from both
        int got1 = dequeue(&queue1, line1);
        int got2 = dequeue(&queue2, line2);

        if (!got1 && !got2) break;  // Both ended

        if (!got1) {
            printf("Extra in file2 line %d: %s", line_num, line2);
            while (dequeue(&queue2, line2)) {
                printf("Extra in file2 line %d: %s", ++line_num, line2);
            }
            break;
        }

        if (!got2) {
            printf("Extra in file1 line %d: %s", line_num, line1);
            while (dequeue(&queue1, line1)) {
                printf("Extra in file1 line %d: %s", ++line_num, line1);
            }
            break;
        }

        if (strcmp(line1, line2) != 0) {
            printf("Line %d differs:\n", line_num);
            printf("  file1: %s", line1);
            printf("  file2: %s", line2);
        }

        line_num++;
    }

    return NULL;
}

int dequeue(LineQueue* queue, char* line) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0 && !queue->eof) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->count == 0 && queue->eof) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;  // No more data
    }

    strcpy(line, queue->lines[queue->head]);
    queue->head = (queue->head + 1) % QUEUE_SIZE;
    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return 1;  // Got data
}
```

### Issue 2: Deadlock Prevention

**Potential Deadlock**: If comparator locks both queues

```c
pthread_mutex_lock(&queue1.mutex);
pthread_mutex_lock(&queue2.mutex);  // Deadlock if another thread locks in opposite order
```

**Solution 1**: Lock Ordering
- Always lock queue1 before queue2
- Ensure no code path violates this

**Solution 2**: Lock Individually
- Lock queue1, dequeue, unlock
- Lock queue2, dequeue, unlock
- No simultaneous holding

**Recommended**: Solution 2 (shown in code above)

### Issue 3: Condition Variable Spurious Wakeup

**Problem**: pthread_cond_wait() can wake without signal

**Correct Pattern**:
```c
while (queue->count == 0 && !queue->eof) {  // WHILE, not IF
    pthread_cond_wait(&queue->not_empty, &queue->mutex);
}
```

**Why**: Wakeup might be spurious or condition changed before we acquired lock

### Issue 4: EOF Signaling

**Challenge**: Reader must signal EOF to consumer

**Correct Approach**:
```c
// Reader:
queue->eof = 1;
pthread_cond_signal(&queue->not_empty);  // Wake waiting consumer

// Consumer:
while (queue->count == 0 && !queue->eof) {
    pthread_cond_wait(&queue->not_empty, &queue->mutex);
}
if (queue->count == 0 && queue->eof) {
    // File ended
    return 0;
}
```

**Wrong Approach**:
```c
// Reader:
queue->eof = 1;
// Forgot to signal - consumer waits forever!
```

### Issue 5: String Copy Safety

**Problem**: strcpy() vulnerable to buffer overflow

```c
strcpy(queue->lines[queue->tail], line);  // Unsafe if line > MAX_LINE
```

**Fix**: Use strncpy or ensure bounds
```c
strncpy(queue->lines[queue->tail], line, MAX_LINE - 1);
queue->lines[queue->tail][MAX_LINE - 1] = '\0';
```

## Recommendations and Improvements

### Recommended Implementation Structure

```c
#define MAX_LINE 1024
#define QUEUE_SIZE 16

typedef struct {
    char lines[QUEUE_SIZE][MAX_LINE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int eof;
} LineQueue;

typedef struct {
    const char* filename;
    LineQueue* queue;
} ReaderArgs;

void init_queue(LineQueue* q) {
    q->head = q->tail = q->count = q->eof = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s file1 file2\n", argv[0]);
        return 1;
    }

    LineQueue queue1, queue2;
    init_queue(&queue1);
    init_queue(&queue2);

    ReaderArgs args1 = {argv[1], &queue1};
    ReaderArgs args2 = {argv[2], &queue2};

    pthread_t reader1, reader2, comparator;

    pthread_create(&reader1, NULL, reader_thread, &args1);
    pthread_create(&reader2, NULL, reader_thread, &args2);
    pthread_create(&comparator, NULL, comparator_thread, NULL);

    pthread_join(reader1, NULL);
    pthread_join(reader2, NULL);
    pthread_join(comparator, NULL);

    pthread_mutex_destroy(&queue1.mutex);
    pthread_cond_destroy(&queue1.not_empty);
    pthread_cond_destroy(&queue1.not_full);
    pthread_mutex_destroy(&queue2.mutex);
    pthread_cond_destroy(&queue2.not_empty);
    pthread_cond_destroy(&queue2.not_full);

    return 0;
}
```

### Error Handling

1. **File Open Errors**:
   ```c
   FILE* file = fopen(filename, "r");
   if (!file) {
       perror("fopen");
       pthread_exit(NULL);
   }
   ```

2. **Argument Validation**:
   ```c
   if (argc != 3) {
       fprintf(stderr, "Usage: %s file1 file2\n", argv[0]);
       return 1;
   }
   ```

3. **Thread Creation Errors**:
   ```c
   int rc = pthread_create(&reader1, NULL, reader_thread, &args1);
   if (rc != 0) {
       fprintf(stderr, "pthread_create failed: %d\n", rc);
       return 1;
   }
   ```

### Testing Strategy

1. **Identical Files**:
   ```bash
   echo -e "line1\nline2\nline3" > file1.txt
   cp file1.txt file2.txt
   ./diff file1.txt file2.txt
   # Expected: No output (no differences)
   ```

2. **Completely Different**:
   ```bash
   echo -e "aaa\nbbb\nccc" > file1.txt
   echo -e "xxx\nyyy\nzzz" > file2.txt
   ./diff file1.txt file2.txt
   # Expected: All three lines reported as different
   ```

3. **File1 Longer**:
   ```bash
   echo -e "line1\nline2\nline3\nline4" > file1.txt
   echo -e "line1\nline2" > file2.txt
   ./diff file1.txt file2.txt
   # Expected: line3 and line4 reported as extra
   ```

4. **File2 Longer**:
   ```bash
   echo -e "line1\nline2" > file1.txt
   echo -e "line1\nline2\nline3\nline4" > file2.txt
   ./diff file1.txt file2.txt
   # Expected: line3 and line4 reported as extra
   ```

5. **Mixed Differences**:
   ```bash
   echo -e "same\ndiff1\nsame\ndiff2" > file1.txt
   echo -e "same\nDIFF1\nsame\nDIFF2" > file2.txt
   ./diff file1.txt file2.txt
   # Expected: Lines 2 and 4 reported as different
   ```

6. **Large Files**:
   ```bash
   seq 1 1000000 > file1.txt
   seq 1 1000000 > file2.txt
   echo "DIFFERENT" >> file2.txt
   time ./diff file1.txt file2.txt
   # Test performance and memory usage
   ```

7. **Empty Files**:
   ```bash
   touch empty1.txt empty2.txt
   ./diff empty1.txt empty2.txt
   # Expected: No output
   ```

### Optimizations

1. **Larger Buffers**: Increase QUEUE_SIZE for better throughput
   ```c
   #define QUEUE_SIZE 128  // More buffering
   ```

2. **Memory Mapped I/O** (for very large files):
   ```c
   int fd = open(filename, O_RDONLY);
   struct stat sb;
   fstat(fd, &sb);
   char* data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
   // Scan for newlines and compare in parallel
   ```

3. **Lock-Free Queue**: Use atomic operations for queue indices
   - Avoid mutex overhead
   - Requires careful implementation

4. **Output Buffering**: Batch output to reduce printf overhead
   ```c
   char output_buffer[4096];
   int output_pos = 0;
   // Accumulate in buffer, flush when full
   ```

## Conclusion

**Task**: Implement parallel diff comparing two text files line-by-line

**Current Status**: Not implemented

**Recommended Architecture**: 3 threads (2 readers + 1 comparator)

**Key Challenges**:
1. Synchronizing line-by-line comparison
2. Handling EOF for both files correctly
3. Maintaining output order
4. Producer-consumer queue implementation

**Complexity**: Medium
- Producer-consumer pattern implementation
- Two concurrent producers feeding one consumer
- EOF handling for both streams
- Output formatting

**Performance Expectation**:
- **Speedup**: 1.0-1.5x (modest, I/O bound)
- **Best case**: Files on different physical disks
- **Worst case**: Same disk, sequential may be faster

**Educational Value**: High
- Demonstrates producer-consumer with multiple producers
- Shows when parallelization doesn't help (I/O bound problems)
- Teaches proper EOF signaling
- Illustrates condition variable usage

**Correctness Concerns**:
- Line synchronization (must compare line N with line N)
- EOF handling (both files, asymmetric lengths)
- Output ordering (sequential, not random)

**Alternative Approach**: If files fit in memory, load both completely, then parallel comparison of chunks with ordered output aggregation - more complex but better performance for large files.
