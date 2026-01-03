# Question 4 Analysis: Parallel Implementation of Linux tee Command

## Summary of the Question/Task

Develop a parallel multithreaded program (C/Pthreads or Java) implementing the Linux `tee` command with the following specifications:

**Command**: `tee filename`

**Functionality**:
- Read from standard input (stdin)
- Write simultaneously to:
  1. Standard output (stdout)
  2. File `filename`

**Architecture**: Exactly three threads:
1. **Reader Thread**: Reads from stdin
2. **Writer Thread 1**: Writes to stdout
3. **Writer Thread 2**: Writes to file

**Key Requirement**: Pipeline architecture with producer-consumer pattern

## Current Implementation Status

**Status**: NOT IMPLEMENTED

The Question_4 directory contains only a README.md file. No code implementation exists.

## Analysis of Required Implementation Approach

### Original tee Command Behavior

```bash
$ echo "Hello World" | tee output.txt
Hello World              # Appears on stdout
$ cat output.txt
Hello World              # Saved to file
```

**Sequential Logic**:
```c
while (read(stdin, buffer, size) > 0) {
    write(stdout, buffer, size);
    write(file, buffer, size);
}
```

### Parallel Pipeline Architecture

```
[stdin] → Reader Thread → [Buffer Queue] → Writer Thread (stdout)
                              ↓
                         Writer Thread (file)
```

**Data Flow**:
1. Reader produces data from stdin into shared buffer queue
2. Two consumers (writers) take data from queue
3. Each writer independently writes to its destination

### Implementation Components

#### 1. Shared Buffer Queue

```c
#define BUFFER_SIZE 1024
#define QUEUE_SIZE 10

typedef struct {
    char data[BUFFER_SIZE];
    int size;
    int eof;
} Buffer;

Buffer buffer_queue[QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;
int queue_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
```

#### 2. Reader Thread

```c
void* reader_thread(void* arg) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0) {
        pthread_mutex_lock(&queue_mutex);

        // Wait if queue is full
        while (queue_count == QUEUE_SIZE) {
            pthread_cond_wait(&queue_not_full, &queue_mutex);
        }

        // Add buffer to queue
        memcpy(buffer_queue[queue_tail].data, buffer, bytes_read);
        buffer_queue[queue_tail].size = bytes_read;
        buffer_queue[queue_tail].eof = 0;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;

        pthread_cond_broadcast(&queue_not_empty);
        pthread_mutex_unlock(&queue_mutex);
    }

    // Signal EOF to both writers
    pthread_mutex_lock(&queue_mutex);
    while (queue_count == QUEUE_SIZE) {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
    }
    buffer_queue[queue_tail].eof = 1;
    buffer_queue[queue_tail].size = 0;
    queue_tail = (queue_tail + 1) % QUEUE_SIZE;
    queue_count++;
    pthread_cond_broadcast(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    return NULL;
}
```

#### 3. Writer Threads

```c
typedef struct {
    int fd;  // File descriptor (STDOUT_FILENO or file fd)
    const char* name;  // For debugging
} WriterArgs;

void* writer_thread(void* arg) {
    WriterArgs* args = (WriterArgs*)arg;
    int eof_received = 0;

    while (!eof_received) {
        pthread_mutex_lock(&queue_mutex);

        // Wait for data
        while (queue_count == 0) {
            pthread_cond_wait(&queue_not_empty, &queue_mutex);
        }

        // Dequeue buffer
        Buffer buf = buffer_queue[queue_head];
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        queue_count--;

        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_mutex);

        // Write data outside critical section
        if (buf.eof) {
            eof_received = 1;
        } else {
            write(args->fd, buf.data, buf.size);
        }
    }

    return NULL;
}
```

### Alternative Architecture: Broadcast Pattern

Instead of queue with two consumers, use **two separate queues**:

```
[stdin] → Reader Thread → [Queue 1] → Writer Thread (stdout)
                      ↓
                      → [Queue 2] → Writer Thread (file)
```

Reader broadcasts to both queues (requires duplicate buffering).

## Correctness Assessment

### Functional Correctness

**Requirement**: Every byte from stdin must appear exactly once in both stdout and file

**Potential Issues**:

1. **Lost Data**: If writer crashes, some data might be lost
   - **Mitigation**: Error checking on write operations
   - **Still a problem**: If stdout blocks indefinitely, file might be incomplete

2. **Duplicate Data**: If queue logic is wrong, might write same buffer twice
   - **Prevention**: Proper queue head/tail management with modulo arithmetic

3. **Partial Writes**: `write()` may write fewer bytes than requested
   - **Fix**: Loop until all bytes written:
   ```c
   ssize_t total_written = 0;
   while (total_written < buf.size) {
       ssize_t n = write(fd, buf.data + total_written, buf.size - total_written);
       if (n < 0) {
           perror("write");
           break;
       }
       total_written += n;
   }
   ```

4. **EOF Handling**: Both writers must receive EOF signal
   - **Current approach**: Single EOF buffer in queue
   - **Problem**: First writer consumes it, second never sees it
   - **Fix**: Separate EOF flags or broadcast mechanism

### Critical Correctness Issue: EOF Broadcasting

**Problem with Current Design**:

```c
// Reader sends one EOF
buffer_queue[queue_tail].eof = 1;

// Writer 1 dequeues EOF, exits
// Writer 2 waits forever - no more data coming!
```

**Solution 1: Reference Counting**:
```c
typedef struct {
    char data[BUFFER_SIZE];
    int size;
    int eof;
    int ref_count;  // Number of writers that need to consume this
} Buffer;

// Reader sets ref_count = 2 for each buffer
// Writers decrement ref_count, only advance head when ref_count == 0
```

**Solution 2: Separate EOF Flags**:
```c
int stdout_eof = 0;
int file_eof = 0;

// Reader sets both flags when done
pthread_mutex_lock(&queue_mutex);
stdout_eof = 1;
file_eof = 1;
pthread_cond_broadcast(&queue_not_empty);
pthread_mutex_unlock(&queue_mutex);

// Each writer checks its own flag
while (!my_eof_flag) {
    // Process data
}
```

**Solution 3: Separate Queues** (Recommended):
```c
Buffer stdout_queue[QUEUE_SIZE];
Buffer file_queue[QUEUE_SIZE];

// Reader copies to both queues
// Each writer has independent queue
```

## Performance Considerations

### Sequential vs Parallel tee

**Sequential tee** (original):
```c
// Simplified
while (read(stdin, buf, SIZE) > 0) {
    write(stdout, buf, SIZE);  // May block
    write(file, buf, SIZE);    // May block
}
```

**Blocking Behavior**:
- If stdout is a pipe to slow consumer → read blocks
- If file I/O is slow (network mount) → read blocks
- **Result**: Input rate limited by slowest output

**Parallel tee**:
- Reader continues reading regardless of writer speed (up to queue size)
- If stdout blocks, file writing continues
- If file writing slow, stdout continues
- **Buffering**: Queue absorbs speed differences

### When Parallel Helps

**Scenario 1: Asymmetric Output Speeds**
```bash
$ ./tee /nfs/slow_network_mount.txt | fast_consumer
```
- File write is slow (network latency)
- stdout is fast (pipe to local process)
- **Benefit**: stdout writer not blocked by slow file I/O

**Scenario 2: Bursty Input**
```bash
$ cat large_file | ./tee output.txt | grep pattern
```
- Input comes in bursts
- Queue buffers bursts, writers drain steadily
- **Benefit**: Smoother throughput

**Scenario 3: CPU-bound Writers**
- If writers perform processing (compression, encryption) before writing
- Parallel writers utilize multiple cores
- **Benefit**: Higher throughput on multicore systems

### When Parallel Doesn't Help

**Scenario 1: Balanced I/O**
- stdin, stdout, file all similar speeds
- **Result**: No bottleneck to parallelize, overhead wasted

**Scenario 2: Small Data**
```bash
$ echo "small" | ./tee output.txt
```
- Thread creation overhead > execution time
- **Result**: Slower than sequential

**Scenario 3: Memory-bound**
- Copying data between queues adds overhead
- Cache thrashing if multiple threads access shared data
- **Result**: Potential slowdown

### Performance Metrics

**Throughput**: Bytes per second through the pipeline

**Latency**: Time from stdin read to stdout/file write

**Trade-off**:
- Large queue → Higher throughput (more buffering), higher latency
- Small queue → Lower latency, may not absorb speed differences

**Optimal Queue Size**: Depends on:
- Buffer size (page size = 4KB typically optimal)
- Expected speed differences
- Memory constraints

**Recommendation**: 8-16 buffers of 4KB each (32-64KB total queue)

## Synchronization and Concurrency Issues

### Issue 1: Producer-Consumer Synchronization

**Classic Problem**: Avoid busy waiting while ensuring prompt response

**Requirements**:
1. Reader must wait if queue full (prevent overflow)
2. Writers must wait if queue empty (prevent underflow)
3. Writers must wake when data available
4. Reader must wake when space available

**Correct Pattern** (Already shown above):
```c
// Producer (Reader)
pthread_mutex_lock(&queue_mutex);
while (queue_count == QUEUE_SIZE) {
    pthread_cond_wait(&queue_not_full, &queue_mutex);
}
// Add to queue
pthread_cond_broadcast(&queue_not_empty);
pthread_mutex_unlock(&queue_mutex);

// Consumer (Writer)
pthread_mutex_lock(&queue_mutex);
while (queue_count == 0) {
    pthread_cond_wait(&queue_not_empty, &queue_mutex);
}
// Remove from queue
pthread_cond_signal(&queue_not_full);
pthread_mutex_unlock(&queue_mutex);
```

**Key Points**:
- `while` loop (not `if`) to handle spurious wakeups (Mesa semantics)
- Broadcast to wake all waiting writers
- Signal (not broadcast) to wake reader (only one producer)

### Issue 2: Multiple Consumers on Single Queue

**Problem**: Two writers competing for same queue elements

**Current Design Flaw**:
```c
// Writer 1 reads queue_head, gets index 0
Buffer buf = buffer_queue[queue_head];
// Writer 2 reads queue_head, also gets index 0
queue_head = (queue_head + 1) % QUEUE_SIZE;
// Both writers process same buffer!
```

**Fix**: Dequeue operation must be atomic:
```c
pthread_mutex_lock(&queue_mutex);
while (queue_count == 0) {
    pthread_cond_wait(&queue_not_empty, &queue_mutex);
}
Buffer buf = buffer_queue[queue_head];  // Read
queue_head = (queue_head + 1) % QUEUE_SIZE;  // Modify
queue_count--;  // Update count
pthread_mutex_unlock(&queue_mutex);  // All within critical section
```

**Result**: Each buffer consumed by only one writer
**Problem**: Need both writers to get same data!

**Solution**: Must use broadcast/duplication approach (see Issue 3)

### Issue 3: Data Duplication for Multiple Consumers

**Requirement**: Same data to both stdout and file

**Approach A: Reference Counting** (Complex)
```c
typedef struct {
    char data[BUFFER_SIZE];
    int size;
    int readers_remaining;  // Initially 2
    pthread_mutex_t ref_mutex;
} SharedBuffer;

// Writer after processing:
pthread_mutex_lock(&buf->ref_mutex);
buf->readers_remaining--;
if (buf->readers_remaining == 0) {
    // Last writer marks buffer as free
    advance_queue_head();
}
pthread_mutex_unlock(&buf->ref_mutex);
```

**Complexity**: Each buffer needs its own mutex, complex coordination

**Approach B: Separate Queues** (Recommended)
```c
Buffer stdout_queue[QUEUE_SIZE];
Buffer file_queue[QUEUE_SIZE];

// Reader copies to both
pthread_mutex_lock(&stdout_queue_mutex);
// Add to stdout_queue
pthread_mutex_unlock(&stdout_queue_mutex);

pthread_mutex_lock(&file_queue_mutex);
// Add to file_queue
pthread_mutex_unlock(&file_queue_mutex);
```

**Pros**: Simple, independent queues, no contention
**Cons**: Double memory usage (2x queue storage)

**Approach C: Broadcast with Acknowledged Read** (Medium complexity)
```c
// Reader places buffer, waits for both writers to acknowledge
Buffer shared_buffer;
int stdout_ready = 0;
int file_ready = 0;

// Reader:
shared_buffer = new_data;
pthread_cond_broadcast(&data_available);
while (!(stdout_ready && file_ready)) {
    pthread_cond_wait(&both_acknowledged, &mutex);
}
stdout_ready = file_ready = 0;

// Writers:
memcpy(local_buf, shared_buffer.data, shared_buffer.size);
if (I_am_stdout_writer) stdout_ready = 1;
if (I_am_file_writer) file_ready = 1;
if (stdout_ready && file_ready) pthread_cond_signal(&both_acknowledged);
```

**Pros**: Single buffer (minimal memory)
**Cons**: Complex coordination, reader blocks on slow writer

### Issue 4: Deadlock Potential

**Scenario**: None in current design (single mutex, no circular wait)

**Proof**:
- Only one mutex: queue_mutex
- Lock ordering: Always acquire queue_mutex (no second mutex)
- Hold-and-wait: Not possible (release before waiting on condition)
- **Result**: Deadlock impossible

**If using separate queues**: Two mutexes
- **Deadlock risk**: If reader locks stdout_queue_mutex, then file_queue_mutex, while another thread does reverse
- **Prevention**: Always lock in same order (stdout before file)

### Issue 5: Signal vs Broadcast

**Current code uses broadcast**:
```c
pthread_cond_broadcast(&queue_not_empty);
```

**Question**: Should we use `signal` instead?

**Analysis**:
- Two writers waiting on queue_not_empty
- If reader signals, only one writer wakes
- That writer consumes buffer, other sleeps forever

**But**: We want both writers to process same data!

**Contradiction**: This reveals fundamental design issue
- If both wake and dequeue, they might get different buffers
- If only one wakes, other doesn't get data

**Resolution**: Confirms need for separate queues or reference counting

## Recommendations and Improvements

### Recommended Implementation: Separate Queues

```c
#define BUFFER_SIZE 4096
#define QUEUE_SIZE 8

typedef struct {
    char data[BUFFER_SIZE];
    size_t size;
    int eof;
} Buffer;

typedef struct {
    Buffer queue[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} BufferQueue;

BufferQueue stdout_queue;
BufferQueue file_queue;

void enqueue(BufferQueue* q, Buffer* buf) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == QUEUE_SIZE) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->queue[q->tail] = *buf;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

Buffer dequeue(BufferQueue* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    Buffer buf = q->queue[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return buf;
}

void* reader_thread(void* arg) {
    Buffer buf;
    while (1) {
        ssize_t n = read(STDIN_FILENO, buf.data, BUFFER_SIZE);
        if (n <= 0) {
            buf.eof = 1;
            buf.size = 0;
            enqueue(&stdout_queue, &buf);
            enqueue(&file_queue, &buf);
            break;
        }
        buf.size = n;
        buf.eof = 0;
        enqueue(&stdout_queue, &buf);
        enqueue(&file_queue, &buf);
    }
    return NULL;
}

void* writer_thread(void* arg) {
    int fd = *(int*)arg;
    while (1) {
        Buffer buf = dequeue((fd == STDOUT_FILENO) ? &stdout_queue : &file_queue);
        if (buf.eof) break;

        size_t total = 0;
        while (total < buf.size) {
            ssize_t written = write(fd, buf.data + total, buf.size - total);
            if (written < 0) {
                perror("write");
                return NULL;
            }
            total += written;
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return 1;
    }

    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("open");
        return 1;
    }

    // Initialize queues
    // ... (init mutexes and conds)

    pthread_t reader, stdout_writer, file_writer;

    pthread_create(&reader, NULL, reader_thread, NULL);

    int stdout_fd = STDOUT_FILENO;
    pthread_create(&stdout_writer, NULL, writer_thread, &stdout_fd);
    pthread_create(&file_writer, NULL, writer_thread, &file_fd);

    pthread_join(reader, NULL);
    pthread_join(stdout_writer, NULL);
    pthread_join(file_writer, NULL);

    close(file_fd);
    return 0;
}
```

### Error Handling

1. **File Open Failure**: Check and report
2. **Write Errors**: Handle partial writes and errors
3. **Read Errors**: Detect and propagate
4. **Memory Allocation**: If using dynamic buffers, check malloc()
5. **Thread Creation Failure**: Check pthread_create() return

### Testing Strategy

1. **Basic Functionality**:
   ```bash
   echo "test" | ./tee output.txt
   diff <(echo "test") output.txt
   ```

2. **Large Data**:
   ```bash
   dd if=/dev/urandom bs=1M count=100 | ./tee output.bin | sha256sum
   sha256sum output.bin
   # Both should match
   ```

3. **Slow Consumer**:
   ```bash
   cat large_file | ./tee output.txt | (sleep 5; cat) > /dev/null
   # Verify output.txt complete before stdout consumer finishes
   ```

4. **Binary Data**:
   ```bash
   cat /bin/ls | ./tee ls_copy | wc -c
   diff /bin/ls ls_copy
   ```

5. **EOF Handling**:
   ```bash
   ./tee output.txt < /dev/null
   [ -f output.txt ] && echo "File created" || echo "FAIL"
   ```

### Performance Testing

```bash
# Generate 1GB test data
dd if=/dev/zero of=test.dat bs=1M count=1024

# Test sequential tee
time cat test.dat | tee output1.txt > /dev/null

# Test parallel tee
time cat test.dat | ./parallel_tee output2.txt > /dev/null

# Compare throughput
```

**Expected**: Parallel may be faster if output asymmetric, similar if balanced

## Conclusion

**Task**: Implement parallel tee with three threads (reader, 2 writers)

**Current Status**: Not implemented

**Key Challenge**: Broadcasting same data to multiple consumers

**Recommended Solution**: Separate queues for each writer

**Critical Issues**:
1. EOF must reach both writers
2. Each buffer must be consumed by both writers
3. Producer-consumer synchronization requires condition variables
4. Partial write handling essential for correctness

**Complexity**: Medium
- Producer-consumer pattern is standard
- Multiple consumers on broadcast data requires careful design
- EOF handling needs special attention

**Performance Expectation**:
- May improve throughput with asymmetric I/O
- Overhead not justified for small data or balanced I/O
- Demonstrates pipeline parallelism concept

**Learning Objectives**:
1. Producer-consumer pattern with condition variables
2. Multiple consumer synchronization
3. Pipeline parallelism architecture
4. I/O buffering and flow control
5. EOF signaling in parallel pipelines

**Alternative Design**: Could use POSIX message queues (mq_send/mq_receive) instead of custom queue implementation - simpler but less educational.
