# Question 4: Welfare Crook Problem (Common Values in Three Arrays) - Analysis

## Summary of the Task
Implement a distributed program to find common elements across three arrays:
- **3 distributed processes**: F, G, H - each with a local array of strings/integers
- **Arrays**: f[1:n], g[1:n], h[1:n] (e.g., IBM employees, Columbia students, NYC welfare recipients)
- **Goal**: Each process determines all names appearing in ALL three lists
- **Constraints**:
  - Use message passing only (no shared memory)
  - Messages contain only ONE value at a time (plus process IDs if needed)
  - Cannot send entire arrays (arrays may be huge)
- **Output**: Each process prints all common values it discovered

## Implementation Status
**NOT IMPLEMENTED** - Only the problem description exists. No code files are present in this directory.

## Required Implementation Approach

### Architecture
This problem requires a **peer-to-peer distributed architecture** using MPI:
- 3 processes (ranks 0, 1, 2) representing F, G, H
- Each has a local array (sorted or unsorted initially)
- Communication via point-to-point messages
- Total MPI processes: 3

### Key Challenge
**Problem**: How to find intersection of three distributed arrays without sending entire arrays?

**Constraint**: Messages can contain only ONE value at a time.

### Algorithm Design Options

#### Algorithm 1: Pairwise Intersection with Coordinator
```
Process F (coordinator):
1. For each value v in f[]:
   a. Send v to process G
   b. Wait for response: IN_G or NOT_IN_G
   c. If IN_G:
      - Send v to process H
      - Wait for response: IN_H or NOT_IN_H
      - If IN_H:
        * Add v to common_values
        * Send v to G and H (inform them it's common)

Process G:
1. Loop:
   a. Receive value v from F
   b. Search for v in g[]
   c. Send response: IN_G or NOT_IN_G
   d. If later receive confirmation from F:
      - Add v to common_values

Process H: (similar to G)
```

**Complexity**: O(|f| × |g| × |h|) comparisons in worst case

#### Algorithm 2: Sorted Array Three-Way Merge
```
Prerequisite: Each process sorts its local array

Process F maintains: index_f, index_g, index_h = 0, 0, 0
               current_f = f[0], current_g = ?, current_h = ?

Algorithm:
1. F requests current values from G and H
2. Three-way comparison:
   a. If current_f == current_g == current_h:
      - Found common value!
      - Broadcast to G and H
      - All advance their indices
   b. Else:
      - Process with smallest value advances
      - Request new value from that process
3. Continue until any process exhausted

Process G/H:
- Maintain local index
- Respond to requests with next value
- Advance index when instructed
```

**Complexity**: O(n) messages (each array element sent at most once)

#### Algorithm 3: Symmetric Distributed Algorithm
```
Each process independently:
1. Sort local array
2. For each local value v:
   a. Send v to other two processes
   b. Wait for their responses
   c. If both respond "YES":
      - Add v to common_values
3. Receive values from other processes:
   a. Search in local array
   b. Respond YES/NO

Challenges:
- Deadlock: All three waiting for responses
- Solution: Use non-blocking communication or tag-based routing
```

**Complexity**: O(3n) messages (each process sends all values)

### Recommended Algorithm: Sorted Three-Way Merge

This is optimal in terms of messages (O(n)) and natural for distributed setting.

## Correctness Considerations

### Synchronization Requirements
1. **Consistency**: All three processes must agree on common values
2. **Completeness**: Must find ALL common values
3. **Correctness**: No false positives (reported value must be in all three arrays)
4. **Termination**: Algorithm must detect when all values examined

### Potential Race Conditions
1. **Message Ordering**: MPI guarantees FIFO between process pairs
2. **Concurrent Requests**: Three-way coordination requires careful sequencing
3. **Early Termination**: One process finishing early must notify others

### Edge Cases
1. **No common values**: Algorithm should detect and report empty set
2. **All values common**: Should report all n values
3. **Duplicates in arrays**: Need to handle duplicate values correctly
4. **Arrays of different sizes**: Generalize to f[1:nf], g[1:ng], h[1:nh]

## Performance Considerations

### Message Complexity Analysis

#### Algorithm 1 (Sequential Scanning):
- Messages: 2 × |f| × (1 + 1) = 4|f| in best case
- Worst case if all values checked: 2|f| messages to G, 2|common| messages to H
- Total: O(|f|) messages

#### Algorithm 2 (Sorted Merge - RECOMMENDED):
- Initial value requests: 2 messages (F asks G and H)
- Value updates: Each array element sent at most once
- Total: O(|f| + |g| + |h|) = O(3n) messages
- **Optimal for this problem**

#### Algorithm 3 (Symmetric):
- Each process sends all values to other two: 2n × 3 = 6n sends
- Each process receives and responds: 6n receives
- Total: O(n) messages per process, but more total communication

### Time Complexity

#### Without Sorting:
- Linear scan: O(|f| × |g| × |h|) comparisons
- With hash tables: O(|f| + |g| + |h|) local work

#### With Sorting:
- Local sort: O(n log n) per process
- Merge: O(n) comparisons
- **Total: O(n log n) dominated by sorting**

### Space Complexity
- Local arrays: O(n) per process
- Result set: O(min(|f|, |g|, |h|)) worst case O(n)
- Message buffers: O(1) since one value per message

## Implementation Recommendations

### 1. Data Structures

```c
typedef struct {
    char** values;      // Array of strings (or int* for integers)
    int size;           // Current array size
    int capacity;       // Allocated capacity
    int current_index;  // For iteration
} Array;

typedef struct {
    char* value;
    int from_process;
} Message;

// For tracking current values in three-way merge
typedef struct {
    char* value_f;
    char* value_g;
    char* value_h;
    int index_f;
    int index_g;
    int index_h;
} MergeState;
```

### 2. Message Protocol

```c
// Message tags
#define REQUEST_VALUE     1  // Request next value from a process
#define SEND_VALUE        2  // Send value in response
#define ADVANCE_INDEX     3  // Instruction to move to next value
#define COMMON_VALUE      4  // Inform about common value found
#define FINISHED          5  // No more values to send
#define TERMINATE         6  // Algorithm complete
```

### 3. Helper Functions

```c
// Comparison for sorting (for strings)
int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// Sort local array
void sort_local_array(Array* arr) {
    qsort(arr->values, arr->size, sizeof(char*), compare_strings);
}

// Get next value from local array
char* get_next_value(Array* arr) {
    if (arr->current_index >= arr->size) {
        return NULL;  // Exhausted
    }
    return arr->values[arr->current_index++];
}

// Find minimum among three values
int find_min(char* v1, char* v2, char* v3, bool* v1_valid, bool* v2_valid, bool* v3_valid) {
    // Returns 0, 1, or 2 indicating which value is smallest
    // Handles NULL (invalid) values
}
```

### 4. Core Algorithm Implementation (Coordinator-based)

```c
void process_f(Array* local_array) {
    sort_local_array(local_array);

    char* value_g = NULL;
    char* value_h = NULL;
    bool g_active = true, h_active = true;

    // Request initial values
    send_request(PROCESS_G);
    send_request(PROCESS_H);
    receive_value(&value_g, PROCESS_G);
    receive_value(&value_h, PROCESS_H);

    for (int i = 0; i < local_array->size && g_active && h_active; ) {
        char* value_f = local_array->values[i];

        int cmp_fg = strcmp(value_f, value_g);
        int cmp_fh = strcmp(value_f, value_h);
        int cmp_gh = strcmp(value_g, value_h);

        if (cmp_fg == 0 && cmp_fh == 0) {
            // All three equal - common value!
            printf("Common value: %s\n", value_f);
            send_common_value(value_f, PROCESS_G);
            send_common_value(value_f, PROCESS_H);

            i++;
            g_active = request_next_value(&value_g, PROCESS_G);
            h_active = request_next_value(&value_h, PROCESS_H);
        }
        else {
            // Find minimum and advance that one
            if (cmp_fg <= 0 && cmp_fh <= 0) {
                i++;  // F is minimum
            }
            else if (cmp_gh <= 0 && strcmp(value_g, value_f) < 0) {
                g_active = request_next_value(&value_g, PROCESS_G);
            }
            else {
                h_active = request_next_value(&value_h, PROCESS_H);
            }
        }
    }

    send_terminate(PROCESS_G);
    send_terminate(PROCESS_H);
}
```

### 5. Worker Process Implementation

```c
void process_g(Array* local_array) {
    sort_local_array(local_array);

    char* current_value = local_array->values[0];
    int index = 0;

    while (true) {
        MPI_Status status;
        char buffer[MAX_STRING_LENGTH];

        MPI_Recv(buffer, MAX_STRING_LENGTH, MPI_CHAR,
                 PROCESS_F, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        switch (status.MPI_TAG) {
            case REQUEST_VALUE:
                if (index < local_array->size) {
                    MPI_Send(local_array->values[index], strlen(...), MPI_CHAR,
                             PROCESS_F, SEND_VALUE, MPI_COMM_WORLD);
                } else {
                    MPI_Send("", 0, MPI_CHAR,
                             PROCESS_F, FINISHED, MPI_COMM_WORLD);
                }
                break;

            case ADVANCE_INDEX:
                index++;
                break;

            case COMMON_VALUE:
                printf("Process G: Common value found: %s\n", buffer);
                break;

            case TERMINATE:
                return;
        }
    }
}
```

### 6. Compilation and Execution

```bash
# Compilation
mpicc -o welfare_crook welfare_crook.c

# Execution with 3 processes
mpirun -np 3 ./welfare_crook

# With input files
mpirun -np 3 ./welfare_crook ibm_employees.txt columbia_students.txt nyc_welfare.txt
```

## Concurrency and Distribution Issues

### MPI-Specific Considerations

1. **String Handling in MPI**:
```c
// Sending variable-length strings
int length = strlen(str);
MPI_Send(&length, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
MPI_Send(str, length, MPI_CHAR, dest, tag, MPI_COMM_WORLD);

// Receiving
int length;
MPI_Recv(&length, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
char* str = malloc(length + 1);
MPI_Recv(str, length, MPI_CHAR, src, tag, MPI_COMM_WORLD, &status);
str[length] = '\0';
```

2. **Deadlock Avoidance**:
- Use non-blocking sends if processes send to each other simultaneously
- Or use buffered sends: MPI_Bsend
- Or carefully order send/receive operations

### Distributed Algorithm Challenges

1. **Global Termination**:
   - Process F (coordinator) knows when done
   - Must notify G and H explicitly
   - G and H wait for TERMINATE message

2. **Duplicate Handling**:
   - If arrays contain duplicates, need to handle carefully
   - Option 1: Remove duplicates during preprocessing
   - Option 2: Skip consecutive duplicates during merge

3. **Array Size Differences**:
   - Generalize to different sizes
   - Smaller arrays finish first
   - Must handle FINISHED message gracefully

## Testing and Validation

### 1. Test Cases

```c
// Test 1: Simple intersection
F: [1, 3, 5, 7, 9]
G: [2, 3, 5, 8, 9]
H: [3, 5, 9, 11]
Expected: [3, 5, 9]

// Test 2: No common values
F: [1, 2, 3]
G: [4, 5, 6]
H: [7, 8, 9]
Expected: []

// Test 3: All same
F: [1, 2, 3]
G: [1, 2, 3]
H: [1, 2, 3]
Expected: [1, 2, 3]

// Test 4: One common value
F: [1, 5, 10]
G: [2, 5, 20]
H: [3, 5, 30]
Expected: [5]

// Test 5: Duplicates
F: [1, 1, 2, 2, 3]
G: [1, 2, 2, 3]
H: [1, 2, 3, 3]
Expected: [1, 2, 3] (or handle duplicates appropriately)
```

### 2. Validation Strategy
```c
// Each process should print the same common values
// Verify by comparing outputs

void print_common_values(char** common, int count, int rank) {
    printf("Process %d found %d common values:\n", rank, count);
    for (int i = 0; i < count; i++) {
        printf("  %s\n", common[i]);
    }
}
```

### 3. Message Counting
```c
// Track messages sent for performance analysis
int messages_sent = 0;
int messages_received = 0;

#define SEND_COUNTED(buf, count, type, dest, tag, comm) \
    do { \
        MPI_Send(buf, count, type, dest, tag, comm); \
        messages_sent++; \
    } while(0)
```

## Advanced Optimizations

### 1. Hash-Based Approach
```c
// Alternative for unsorted arrays
// Process F: Send all values to G and H
// G and H: Build hash sets, respond with intersection
// Trade-off: More messages but no sorting overhead
```

### 2. Bloom Filters
```c
// Optimization: Use Bloom filters for preliminary filtering
// Each process creates Bloom filter of its array
// Exchange filters (fixed size) first
// Then only query values that pass all three filters
// Reduces false positives in queries
```

### 3. Parallel Local Operations
```c
// If arrays are very large, could parallelize local sorting
// Use OpenMP within each MPI process
#pragma omp parallel for
for (int i = 0; i < chunks; i++) {
    sort_chunk(i);
}
merge_chunks();
```

## Comparison with Alternative Designs

| Design | Messages | Time | Pros | Cons |
|--------|----------|------|------|------|
| Sequential scan | O(n²) | O(n²) | Simple | Inefficient |
| Sorted merge | O(n) | O(n log n) | Optimal messages | Requires sorting |
| Hash-based | O(n) | O(n) | No sorting | More messages |
| Symmetric P2P | O(n) | O(n log n) | Balanced load | Complex coordination |

## Expected Learning Outcomes
1. Distributed set intersection algorithms
2. Handling large distributed datasets
3. Message-passing constraints (one value per message)
4. Sorted merge algorithms in distributed settings
5. Coordinator vs peer-to-peer patterns
6. Trade-offs between local computation and communication
7. Handling variable-length data in MPI
8. Termination detection in distributed algorithms
