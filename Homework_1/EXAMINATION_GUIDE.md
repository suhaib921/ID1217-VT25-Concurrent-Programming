# Homework 1 - Oral Examination Guide (10 minutes per student)

**Format:** Show output → Explain key concepts → Answer 2-3 questions about code/concepts

---

## Question 1: Matrix Sum, Min, and Max (20 points)

### Commands to Test

```bash
cd Homework_1/Question_1

# Test part (a) - Sum, Min, Max with random values
gcc -pthread -o matrixSum_a matrixSum_a.c -lm
./matrixSum_a

# Test part (b) - Main thread prints results, no barrier
gcc -pthread -o matrixSum_b matrixSum_b.c -lm
./matrixSum_b

# Test part (c) - Bag of tasks approach
gcc -pthread -o matrixSum_c matrixSum_c.c -lm
./matrixSum_c
```

### Expected Output
- Sum, minimum value with position (row, col), maximum value with position
- Execution time should be displayed
- Random matrix values (not all ones)

### Key Concepts to Explain
1. **Thread synchronization using mutexes** - Protecting shared data (min, max, sum)
2. **Barrier vs no barrier** - Understanding thread coordination
3. **Bag of tasks pattern** - Dynamic work distribution using shared counter with mutex

### Examination Questions
1. **Q1:** Explain why you need a mutex when updating the global min/max values. What would happen without it?
**What to verify:** Student should explain that without mutex, multiple threads could simultaneously read and write global_min/global_max, causing race conditions where updates are lost (read-modify-write problem).

2. **Q2:** In part (c), explain how the bag of tasks pattern works. Why might it provide better load balancing than static assignment?
**What to verify:** Student should explain that workers dynamically fetch work instead of static assignment. If one worker finishes early (uneven workload), it can grab more tasks. Compare to part (a) where each worker gets stripSize rows regardless of completion speed.

3. **Q3:** What's the difference between using a barrier (part a) vs joining threads in main (part b)?
**What to verify:** Student should explain:
- **Barrier:** Workers synchronize with each other; Worker 0 continues and prints
- **Join:** Main thread waits for all workers to complete, then main prints
- Barrier allows workers to coordinate mid-execution; join is for final synchronization

---

## Question 2: Parallel Quicksort (20 points)

### Commands to Test

```bash
cd Homework_1/Question_2

# Compile and run
gcc -pthread -o quicksort quicksort.c -lm
./quicksort

# Test with different array sizes
./quicksort 10000
./quicksort 100000
```

### Expected Output
- Sorted array (verify first/last elements)
- Execution time
- Confirmation that array is sorted correctly

### Key Concepts to Explain
1. **Recursive parallelism** - Creating threads recursively for each partition
2. **Thread creation overhead** - Why we might limit thread creation depth
3. **Quicksort partitioning** - Dividing work around pivot element

### Examination Questions
1. **Q1:** Explain how you implement recursive parallelism in quicksort. When do you create new threads vs sequential sort?
**What to verify:** Student explains that after partitioning, if subarray is large enough (> THREAD_THRESHOLD), create new thread for right partition while current thread handles left. Otherwise, both partitions run sequentially to avoid thread overhead.

2. **Q2:** What is the potential problem with creating unlimited threads in recursive quicksort? How did you solve it?
**What to verify:** Student should explain:
- **Problem:** Creating threads for every recursive call causes thread explosion (exponential growth), high memory usage, excessive context switching
- **Solution:** THREAD_THRESHOLD ensures threads only created for large subarrays (> 10000 elements)
- **Graceful degradation:** If pthread_create fails, falls back to sequential sorting

3. **Q3:** How do you ensure that the parent thread waits for child threads to complete before combining results?
**What to verify:** Student explains that pthread_join() blocks the parent thread until the child thread (right_thread) completes. This ensures both left (done by parent) and right (done by child) partitions are fully sorted before the parent returns.

---

## Question 3: Computing Pi using Adaptive Quadrature (20 points)

### Commands to Test

```bash
cd Homework_1/Question_3

# Compile and run with different thread counts
gcc -pthread -o compute_pi matrixSum.c -lm
./compute_pi 1
./compute_pi 2
./compute_pi 4
./compute_pi 8
```

### Expected Output
- Computed value of pi (should be close to 3.14159...)
- Epsilon value used
- Number of threads
- Execution time

### Key Concepts to Explain
1. **Adaptive quadrature algorithm** - Recursive area calculation with error tolerance
2. **Work distribution** - Dividing integration ranges among threads
3. **Numerical integration** - Approximating area under curve f(x) = sqrt(1-x²)

### Examination Questions
1. **Q1:** Explain the adaptive quadrature algorithm. How does epsilon control accuracy?
**What to verify:** Student explains:
- f(x) = sqrt(1-x²) defines upper-right quadrant of unit circle
- Divide [0,1] into total_num_steps intervals
- Use midpoint rule: sample f(x) at center of each interval
- Sum all f(x) values, multiply by dx to get area
- Multiply by 4 (four quadrants) to get pi

2. **Q2:** How did you divide the work among multiple threads? What does each thread compute?
**What to verify:** Student explains:
- Integration steps divided equally among workers
- Each worker gets steps_per_worker steps (last worker handles remainder)
- Each worker computes partial area for its range
- Main thread sums all partial areas after threads finish
- No synchronization needed during computation (embarrassingly parallel)


3. **Q3:** Does execution time decrease linearly with number of threads? Why or why not?
**What to verify:** Student should discuss:
- **Theoretical:** Computation is embarrassingly parallel, should scale linearly
- **Practical limitations:**
  - Thread creation/destruction overhead (lines 79-86)
  - Memory bandwidth bottleneck (reading/writing partial_sums)
  - Speedup limited by number of CPU cores (Amdahl's Law)
  - Cache effects and false sharing
- **Observation:** May see diminishing returns beyond number of physical cores

---

## Question 4: Parallel tee Command (20 points)

### Commands to Test

```bash
cd Homework_1/Question_4

# Compile
gcc -pthread -o tee tee.c

# Test 1: Simple input
echo "Hello World" | ./tee output.txt
cat output.txt

# Test 2: Multiple lines
printf "Line 1\nLine 2\nLine 3\n" | ./tee output.txt
cat output.txt

# Test 3: Piping from a file
cat /etc/hosts | ./tee output.txt
```

### Expected Output
- Input should appear on standard output
- Same input should be written to output.txt
- Both outputs should be identical

### Key Concepts to Explain
1. **Producer-consumer pattern** - Reader thread produces, writer threads consume
2. **Thread coordination** - Using condition variables and shared buffer
3. **Three-thread architecture** - Reader, stdout writer, file writer

### Examination Questions
1. **Q1:** Explain the three threads in your program and what each one does. How do they communicate?
**What to verify:** Student explains:
- **Reader thread:** Reads from stdin (line 129), places copies in both queues (lines 131-132)
- **Stdout writer:** Dequeues from stdout_queue, writes to STDOUT_FILENO
- **File writer:** Dequeues from file_queue, writes to file descriptor
- Communication via two separate thread-safe queues (producer-consumer pattern)

2. **Q2:** How do you coordinate between the reader thread and the two writer threads? What synchronization primitives do you use?
**What to verify:** Student explains:
- **Mutex** protects queue data structure
- **Condition variables:**
  - `not_full`: Woken when space available (reader waits on this if queue full)
  - `not_empty`: Woken when data available (writers wait on this if queue empty)
- Prevents busy-waiting and coordinates producer-consumer relationship

3. **Q3:** How do you handle the end of input? How do writer threads know when to terminate?
**What to verify:** Student explains that when stdin closes, reader sends zero-size buffer to both queues. Writers check buffer size and exit when they receive zero-size buffer. The eof flag ensures writers don't wait forever if queue is empty.

---

## Question 5: Parallel diff Command (20 points)

### Commands to Test

```bash
cd Homework_1/Question_5

# Create test files
echo -e "line1\nline2\nline3" > file1.txt
echo -e "line1\nDIFFERENT\nline3\nextra" > file2.txt

# Compile and run
gcc -pthread -o diff diff.c
./diff file1.txt file2.txt

# Test with identical files
echo -e "same\nlines" > file3.txt
echo -e "same\nlines" > file4.txt
./diff file3.txt file4.txt

# Test with different length files
echo -e "short" > file5.txt
echo -e "short\nextra1\nextra2" > file6.txt
./diff file5.txt file6.txt
```

### Expected Output
- Only different lines should be printed
- Extra lines in longer file should be printed
- Format should show both versions of differing lines

### Key Concepts to Explain
1. **Thread coordination** - Two reader threads, one comparator thread
2. **Synchronization** - Ensuring lines are compared in correct order
3. **Producer-consumer with two producers** - Both file readers feeding comparator

### Examination Questions
1. **Q1:** Explain your threading model. How many threads and what does each do?
**What to verify:** Student explains:
- **2 Reader threads:** Each reads one file (fgets at line 147), pushes lines to its queue
- **Main thread (comparator):** Pops one line from each queue (lines 192-193), compares them (line 201)
- Pattern: Two producers (file readers), one consumer (main thread)

2. **Q2:** How do you ensure that corresponding lines from both files are compared correctly?
**What to verify:** Student explains:
- Each file has its own queue, preserving line order within each file
- Main thread alternates: pop from q1, pop from q2, compare
- Blocking behavior ensures we wait for next line before comparing
- Line counter (line 190, 221) tracks which line number we're comparing

3. **Q3:** How do you handle files of different lengths? What happens when one file ends before the other?
*What to verify:** Student explains:
- When reader finishes file, it calls queue_signal_eof() (line 151)
- queue_pop() returns NULL when EOF reached and queue empty (lines 115-117)
- Main thread handles three cases: both have lines, only file1 has line, only file2 has line
- Continues printing extra lines until both return NULL

---

## Question 6: Find Palindromes and Semordnilaps (40 points)

### Commands to Test

```bash
cd Homework_1/Question_6

# Compile and run with different worker counts
gcc -pthread -o palindrome matrixSum.c -lm
./palindrome 1
./palindrome 2
./palindrome 4
./palindrome 8

# Check results file
head -20 results.txt
tail -20 results.txt
wc -l results.txt
```

### Expected Output
- Total palindromes found
- Total semordnilaps found (should be even number - pairs!)
- Count per worker
- Results written to output file
- Execution time

### Key Concepts to Explain
1. **Work partitioning** - Dividing dictionary among W workers
2. **String reversal and lookup** - Efficient palindrome/semordnilap detection
3. **Per-worker counting** - Each worker tracks its own finds

### Examination Questions
1. **Q1:** Explain how you divide the dictionary among workers. How does each worker know which words to process?
*What to verify:** Student explains:
- **Static partitioning:** Dictionary divided into equal chunks
- Each worker gets words_per_worker words (last worker takes remainder)
- Worker myid processes words from `[myid * words_per_worker, end_index)`
- Input phase is sequential (line 185), only computation is parallel
- No synchronization needed during computation (workers process disjoint sets)

2. **Q2:** How do you distinguish between a palindrome and a semordnilap? Why is the count of semordnilaps always even?
*What to verify:** Student explains:
- **Palindrome:** Word reads same forwards and backwards (line 58 comparison)
- **Semordnilap:** Reversed word is different but also in dictionary
- Uses `bsearch()` on sorted dictionary for O(log n) lookup (line 304)
- **Even count:** Every semordnilap pair appears twice (e.g., "draw" and "ward")
  - When processing "draw", we find "ward" in dictionary → count "draw"
  - When processing "ward", we find "draw" in dictionary → count "ward"
  - Both get counted, so total is always even
3. **Q3:** How do you handle the results from multiple workers? Do you need synchronization when writing results?
*What to verify:** Student explains:
- **No synchronization needed during computation:**
  - Each worker has its own WorkerResult struct (line 281)
  - Workers write to disjoint memory locations (worker_results[myid])
  - No shared data structures during parallel phase
- **Three phases:**
  1. **Input (sequential):** Load dictionary (lines 183-190)
  2. **Compute (parallel):** Find palindromes/semordnilaps (lines 210-217)
  3. **Output (sequential):** Write results to file (lines 222-246)
- Sequential output phase aggregates all worker results without conflicts

---

## Question 7: The 8-Queens Problem (40 points)

### Commands to Test

```bash
cd Homework_1/Question_7

# Compile and run
gcc -pthread -o nqueens matrixSum.c -lm
./nqueens

# If already compiled
./nqueens

# Verify 92 solutions
./nqueens | grep -i "solution" | wc -l
```

### Expected Output
- All 92 solutions to 8-queens problem
- Each solution showing queen positions
- Execution time
- Confirmation of 92 total solutions

### Key Concepts to Explain
1. **Pipeline pattern** - Main thread generates, workers validate
2. **Backtracking algorithm** - Recursive queen placement
3. **Conflict detection** - Checking rows, columns, diagonals

### Examination Questions
1. **Q1:** Explain the pipeline architecture in your solution. What does the main thread do vs worker threads?
- **Divide-and-conquer approach:** Each worker explores solutions with first queen in different column
- Worker 0: first queen in column 0, Worker 1: column 1, etc.
- Each worker does independent backtracking search from row 1 onwards
- **Work division:** numWorkers ≤ N (limited to N starting positions)
- No communication between workers during search (embarrassingly parallel)

2. **Q2:** How do you check if a queen placement is valid? Explain the conflict detection for diagonals.
**What to verify:** Student explains:
- **No row conflicts:** Since we place one queen per row (incrementing row), can't have row conflict
- **Column conflict:** `board[i] == col` checks if any previous queen in same column
- **Diagonal conflict:** `abs(board[i] - col) == abs(i - row)` checks both diagonals
  - Left diagonal (↙↗): row-col is constant
  - Right diagonal (↘↖): row+col is constant
  - Combined check: if column difference equals row difference, they're on same diagonal
- Only checks rows 0 to row-1 (previously placed queens)

3. **Q3:** How do worker threads communicate with the main thread? How are valid solutions collected?
**What to verify:** Student explains:
- **Shared counter:** `totalSolutions` accessed by all workers
- **Critical section:** Incrementing totalSolutions (read-modify-write)
- **Mutex protection:** Lock before increment, unlock after (lines 56-58)
- **Race condition prevention:** Without mutex, two threads could read same value, increment, and write back → lost update
- Main thread reads totalSolutions after pthread_join (no lock needed, workers finished)

---

## General Questions (Can ask for any solution)

1. **Performance:** Did you measure execution time? How does it scale with number of threads?
2. **Race conditions:** Where are the critical sections in your code? How did you protect them?
3. **Thread synchronization:** What Pthread primitives did you use? (mutexes, condition variables, barriers, join)
4. **Debugging:** What was the most difficult bug you encountered and how did you fix it?
5. **Load balancing:** Is the work evenly distributed among threads? Why or why not?

---

## Quick Compilation/Test Script

```bash
#!/bin/bash
# Run this to quickly test all solutions

echo "=== Question 1a ==="
cd Homework_1/Question_1 && gcc -pthread -o matrixSum_a matrixSum_a.c -lm && ./matrixSum_a

echo "=== Question 2 ==="
cd ../Question_2 && gcc -pthread -o quicksort quicksort.c -lm && ./quicksort 1000

echo "=== Question 3 ==="
cd ../Question_3 && gcc -pthread -o compute_pi matrixSum.c -lm && ./compute_pi 4

echo "=== Question 4 ==="
cd ../Question_4 && gcc -pthread -o tee tee.c && echo "Test input" | ./tee output.txt && cat output.txt

echo "=== Question 5 ==="
cd ../Question_5 && gcc -pthread -o diff diff.c && echo -e "a\nb" > t1.txt && echo -e "a\nc" > t2.txt && ./diff t1.txt t2.txt

echo "=== Question 6 ==="
cd ../Question_6 && gcc -pthread -o palindrome matrixSum.c -lm && ./palindrome 4 && head -10 results.txt

echo "=== Question 7 ==="
cd ../Question_7 && gcc -pthread -o nqueens matrixSum.c -lm && ./nqueens | head -20
```

---

## Grading Rubric (10-minute format)

- **Output demonstration (2 min):** Program runs correctly, shows expected output
- **Concept explanation (4 min):** Student clearly explains 1-2 key concepts
- **Technical questions (4 min):** Student answers 2-3 questions about implementation
- **Deductions:** Non-working code, poor synchronization, race conditions, memory leaks

**Key things to verify:**
- Correct use of pthread primitives (mutex, condition variables, barriers)
- No race conditions in critical sections
- Proper thread creation and joining
- Performance measurement included
- Code compiles without warnings (use -Wall flag)
