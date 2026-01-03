# Question 6: Exchanging Values (Three Algorithms) - Analysis

## Summary of the Task
Implement three different algorithms for exchanging values among distributed peers:
- **Reference**: Lecture 15, slides 28-34 (three program outlines for value exchange)
- **Implementation**: Each algorithm in C using MPI
- **Rounds**: Execute R rounds of exchanges
- **Output**: Trace of interesting events (not too verbose)
- **Performance Evaluation**: Compare execution time as function of:
  - Number of processes: 2, 4, 6, 8
  - Number of rounds: 1, 2, 3
- **Deliverable**: Plot showing total execution time for each algorithm

## Implementation Status
**NOT IMPLEMENTED** - Only the problem description exists. No code files are present in this directory.

## Required Implementation Approach

### Context: Value Exchange Problem
**Scenario**: Each process has a local value and wants to share/exchange values with other processes.

**Common patterns**:
1. **Ring-based exchange**: Pass values around a ring
2. **All-to-all exchange**: Every process sends to every other process
3. **Butterfly/hypercube exchange**: Structured hierarchical exchange

**Goal of R rounds**: After R rounds, each process has accumulated values from multiple peers.

## Three Algorithm Patterns (Based on Typical Lecture Content)

### Algorithm 1: Sequential Ring Exchange
**Description**: Processes arranged in a logical ring. Each round, values circulate around the ring.

#### Algorithm:
```
Process with rank i (out of n processes):
Initialize: my_value = initial_value[i]
           collected_values = [my_value]

For round = 1 to R:
    left_neighbor = (i - 1 + n) % n
    right_neighbor = (i + 1) % n

    Send my_value to right_neighbor
    Receive new_value from left_neighbor
    my_value = new_value
    Add new_value to collected_values

After R rounds:
    Process i has values from processes [i, i-1, i-2, ..., i-R] (mod n)
```

#### Characteristics:
- **Messages per round**: n messages (each process sends once)
- **Total messages**: R × n
- **Latency per round**: 1 message hop
- **Total time**: O(R) rounds
- **Concurrency**: All processes send/receive simultaneously
- **Coverage**: After n-1 rounds, every process has all values

#### MPI Implementation Considerations:
```c
// Use MPI_Sendrecv to avoid deadlock
MPI_Sendrecv(&send_value, 1, MPI_INT, right_neighbor, TAG,
             &recv_value, 1, MPI_INT, left_neighbor, TAG,
             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```

### Algorithm 2: All-to-All Broadcast
**Description**: Each round, every process sends its value to all other processes.

#### Algorithm:
```
Process with rank i (out of n processes):
Initialize: my_values[i] = initial_value
           round_values = array[n]

For round = 1 to R:
    // Send my value to all others
    For j = 0 to n-1:
        If j != i:
            Send my_values[i] to process j

    // Receive values from all others
    For j = 0 to n-1:
        If j != i:
            Receive round_values[j] from process j

    // Update: Could aggregate, replace, or process values
    For j = 0 to n-1:
        my_values[j] = round_values[j]

After R rounds:
    All processes have the same set of values (full synchronization)
```

#### Characteristics:
- **Messages per round**: n × (n-1) messages (each process sends to n-1 others)
- **Total messages**: R × n × (n-1) = O(R × n²)
- **Latency per round**: 1 message hop
- **Total time**: O(R) rounds
- **Bandwidth**: High - O(n²) messages per round
- **Coverage**: After 1 round, every process has all values (full exchange)

#### MPI Implementation Considerations:
```c
// Use MPI_Allgather for efficient all-to-all
MPI_Allgather(&my_value, 1, MPI_INT,
              all_values, 1, MPI_INT,
              MPI_COMM_WORLD);

// Or MPI_Alltoall for personalized exchange
```

### Algorithm 3: Butterfly/Hypercube Exchange
**Description**: Processes exchange values in a hierarchical pattern, doubling reach each round.

#### Algorithm:
```
Process with rank i (out of n processes):
Initialize: my_values = [initial_value]
           distance = 1

For round = 1 to R:
    partner = i XOR distance  // XOR for hypercube topology

    Send my_values to partner
    Receive partner_values from partner

    Merge my_values and partner_values
    distance = distance * 2  // Or shift left: distance << 1

After R rounds:
    Each process has values from 2^R processes (exponential growth)
```

#### Characteristics:
- **Messages per round**: n/2 pairs × 2 = n messages
- **Total messages**: R × n
- **Latency per round**: 1 message hop
- **Total time**: O(R) rounds
- **Coverage**: After log₂(n) rounds, every process has all values
- **Efficiency**: Logarithmic convergence - optimal for dissemination

#### MPI Implementation Considerations:
```c
// Hypercube exchange pattern
int partner = rank ^ (1 << round);  // XOR with 2^round

if (partner < size) {
    MPI_Sendrecv(send_buf, send_count, MPI_INT, partner, TAG,
                 recv_buf, recv_count, MPI_INT, partner, TAG,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
```

## Correctness Considerations

### Synchronization Requirements
1. **Round Synchronization**: All processes must complete round r before starting round r+1
2. **Deadlock Avoidance**: Send/receive must be coordinated
3. **Message Ordering**: MPI guarantees FIFO between process pairs
4. **Barrier Synchronization**: Explicit barriers may be needed between rounds

### Deadlock Prevention Strategies

#### Problem: Circular Waiting
If all processes send before receiving, buffers may fill (deadlock).
If all processes receive before sending, nobody sends (deadlock).

#### Solutions:
```c
// Solution 1: MPI_Sendrecv (recommended)
MPI_Sendrecv(&send_buf, count, MPI_INT, dest, tag,
             &recv_buf, count, MPI_INT, src, tag,
             comm, &status);

// Solution 2: Odd-even ordering
if (rank % 2 == 0) {
    MPI_Send(...);
    MPI_Recv(...);
} else {
    MPI_Recv(...);
    MPI_Send(...);
}

// Solution 3: Non-blocking communication
MPI_Isend(&send_buf, count, MPI_INT, dest, tag, comm, &request);
MPI_Recv(&recv_buf, count, MPI_INT, src, tag, comm, &status);
MPI_Wait(&request, &status);

// Solution 4: Buffered send
MPI_Bsend(...);  // Returns immediately if buffer available
```

### Edge Cases
1. **n = 1**: Single process (no exchange needed)
2. **n not power of 2**: Hypercube algorithm needs adjustment
3. **R = 0**: No exchange, trivial case
4. **R > n**: May have redundant rounds in some algorithms

## Performance Considerations

### Message Complexity Comparison

| Algorithm | Messages/Round | Total Messages | Rounds to Full Coverage |
|-----------|----------------|----------------|------------------------|
| Ring      | n              | R × n          | n - 1                  |
| All-to-All| n²             | R × n²         | 1                      |
| Butterfly | n              | R × n          | log₂(n)                |

### Time Complexity Analysis

**Assumptions**:
- α = latency (time to initiate message)
- β = bandwidth cost per byte
- Message size: m bytes
- Number of processes: n
- Number of rounds: R

#### Algorithm 1 (Ring):
```
Time per round: α + β × m (one hop)
Total time: R × (α + β × m)
```

#### Algorithm 2 (All-to-All):
```
Time per round: α + (n-1) × β × m (n-1 sends, can overlap)
With collective: α × log(n) + β × m × (n-1) (using MPI_Allgather)
Total time: R × [α × log(n) + β × m × (n-1)]
```

#### Algorithm 3 (Butterfly):
```
Time per round: α + β × (growing_size)
Round 1: α + β × m
Round 2: α + β × 2m (twice as much data)
Round k: α + β × 2^(k-1) × m
Total time: R × α + β × m × (2^R - 1)
```

### Scalability Comparison

| Factor | Ring | All-to-All | Butterfly |
|--------|------|------------|-----------|
| Network load | Low | High | Low |
| Latency | Linear | Constant | Logarithmic |
| Bandwidth | Linear | Quadratic | Linear |
| Scalability | Good | Poor | Excellent |

### Expected Performance Results

For **small n** (2-8 processes):
- All-to-All may be fastest (low latency, high bandwidth acceptable)
- Ring similar to Butterfly
- Difference increases with R

For **large n** (>16 processes):
- Butterfly fastest (logarithmic convergence)
- Ring moderate
- All-to-All slowest (quadratic messages)

## Implementation Recommendations

### 1. Common Framework Structure

```c
typedef struct {
    int rank;
    int size;
    int rounds;
    int *values;      // Collected values
    int value_count;
    double start_time;
    double end_time;
} ExchangeContext;

// Main structure
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            printf("Usage: %s <algorithm> <rounds>\n", argv[0]);
            printf("Algorithms: 1=Ring, 2=AllToAll, 3=Butterfly\n");
        }
        MPI_Finalize();
        return 1;
    }

    int algorithm = atoi(argv[1]);
    int rounds = atoi(argv[2]);

    ExchangeContext ctx;
    initialize_context(&ctx, rank, size, rounds);

    switch (algorithm) {
        case 1:
            ring_exchange(&ctx);
            break;
        case 2:
            alltoall_exchange(&ctx);
            break;
        case 3:
            butterfly_exchange(&ctx);
            break;
        default:
            fprintf(stderr, "Unknown algorithm %d\n", algorithm);
    }

    finalize_context(&ctx);
    MPI_Finalize();
    return 0;
}
```

### 2. Algorithm 1: Ring Exchange Implementation

```c
void ring_exchange(ExchangeContext *ctx) {
    int my_value = ctx->rank;  // Initial value is rank
    int recv_value;

    int left = (ctx->rank - 1 + ctx->size) % ctx->size;
    int right = (ctx->rank + 1) % ctx->size;

    if (ctx->rank == 0) {
        printf("Starting Ring Exchange: %d rounds, %d processes\n",
               ctx->rounds, ctx->size);
    }

    MPI_Barrier(MPI_COMM_WORLD);  // Synchronize start
    ctx->start_time = MPI_Wtime();

    for (int round = 0; round < ctx->rounds; round++) {
        // Simultaneously send to right, receive from left
        MPI_Sendrecv(&my_value, 1, MPI_INT, right, 0,
                     &recv_value, 1, MPI_INT, left, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("[Round %d] Process %d: sent %d to %d, received %d from %d\n",
               round, ctx->rank, my_value, right, recv_value, left);

        my_value = recv_value;  // Pass along received value
    }

    ctx->end_time = MPI_Wtime();

    if (ctx->rank == 0) {
        printf("Ring Exchange completed in %.6f seconds\n",
               ctx->end_time - ctx->start_time);
    }
}
```

### 3. Algorithm 2: All-to-All Exchange Implementation

```c
void alltoall_exchange(ExchangeContext *ctx) {
    int *my_values = malloc(ctx->size * sizeof(int));
    int *all_values = malloc(ctx->size * sizeof(int));

    // Initialize with rank
    for (int i = 0; i < ctx->size; i++) {
        my_values[i] = (i == ctx->rank) ? ctx->rank : -1;
    }

    if (ctx->rank == 0) {
        printf("Starting All-to-All Exchange: %d rounds, %d processes\n",
               ctx->rounds, ctx->size);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ctx->start_time = MPI_Wtime();

    for (int round = 0; round < ctx->rounds; round++) {
        // Use MPI_Allgather for efficient all-to-all
        MPI_Allgather(&my_values[ctx->rank], 1, MPI_INT,
                      all_values, 1, MPI_INT,
                      MPI_COMM_WORLD);

        if (round < 3 || round == ctx->rounds - 1) {  // Limit verbosity
            printf("[Round %d] Process %d gathered values: ", round, ctx->rank);
            for (int i = 0; i < ctx->size; i++) {
                printf("%d ", all_values[i]);
            }
            printf("\n");
        }

        // Update my_values with gathered data
        memcpy(my_values, all_values, ctx->size * sizeof(int));
    }

    ctx->end_time = MPI_Wtime();

    if (ctx->rank == 0) {
        printf("All-to-All Exchange completed in %.6f seconds\n",
               ctx->end_time - ctx->start_time);
    }

    free(my_values);
    free(all_values);
}
```

### 4. Algorithm 3: Butterfly Exchange Implementation

```c
void butterfly_exchange(ExchangeContext *ctx) {
    int *my_values = malloc(ctx->size * sizeof(int));
    int *partner_values = malloc(ctx->size * sizeof(int));
    int my_count = 1;
    int partner_count;

    my_values[0] = ctx->rank;

    if (ctx->rank == 0) {
        printf("Starting Butterfly Exchange: %d rounds, %d processes\n",
               ctx->rounds, ctx->size);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    ctx->start_time = MPI_Wtime();

    for (int round = 0; round < ctx->rounds; round++) {
        int distance = 1 << round;  // 2^round
        int partner = ctx->rank ^ distance;

        if (partner >= ctx->size) {
            // No partner in this round (size not power of 2)
            printf("[Round %d] Process %d: no partner (would be %d)\n",
                   round, ctx->rank, partner);
            continue;
        }

        // Exchange value counts first
        MPI_Sendrecv(&my_count, 1, MPI_INT, partner, 0,
                     &partner_count, 1, MPI_INT, partner, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Exchange values
        MPI_Sendrecv(my_values, my_count, MPI_INT, partner, 1,
                     partner_values, partner_count, MPI_INT, partner, 1,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("[Round %d] Process %d: exchanged with %d (%d values)\n",
               round, ctx->rank, partner, partner_count);

        // Merge received values (remove duplicates)
        int new_count = merge_unique(my_values, my_count,
                                     partner_values, partner_count);
        my_count = new_count;
    }

    ctx->end_time = MPI_Wtime();

    if (ctx->rank == 0) {
        printf("Butterfly Exchange completed in %.6f seconds\n",
               ctx->end_time - ctx->start_time);
    }

    printf("Process %d final value count: %d\n", ctx->rank, my_count);

    free(my_values);
    free(partner_values);
}

// Helper: Merge two sorted arrays, removing duplicates
int merge_unique(int *arr1, int count1, int *arr2, int count2) {
    int *temp = malloc((count1 + count2) * sizeof(int));
    int i = 0, j = 0, k = 0;

    // Simple merge
    while (i < count1) temp[k++] = arr1[i++];
    while (j < count2) {
        bool duplicate = false;
        for (int x = 0; x < count1; x++) {
            if (arr2[j] == arr1[x]) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) temp[k++] = arr2[j];
        j++;
    }

    memcpy(arr1, temp, k * sizeof(int));
    free(temp);
    return k;
}
```

### 5. Performance Measurement

```c
// Add timing collection
void collect_timing_data(double local_time, int rank, int size) {
    double *all_times = NULL;
    if (rank == 0) {
        all_times = malloc(size * sizeof(double));
    }

    MPI_Gather(&local_time, 1, MPI_DOUBLE,
               all_times, 1, MPI_DOUBLE,
               0, MPI_COMM_WORLD);

    if (rank == 0) {
        double total = 0, max_time = 0, min_time = all_times[0];
        for (int i = 0; i < size; i++) {
            total += all_times[i];
            if (all_times[i] > max_time) max_time = all_times[i];
            if (all_times[i] < min_time) min_time = all_times[i];
        }
        printf("Timing: Avg=%.6f, Min=%.6f, Max=%.6f\n",
               total/size, min_time, max_time);
        free(all_times);
    }
}
```

### 6. Automated Performance Evaluation Script

```bash
#!/bin/bash
# run_experiments.sh

OUTPUT_FILE="results.csv"
echo "Algorithm,Processes,Rounds,Time" > $OUTPUT_FILE

for alg in 1 2 3; do
    for procs in 2 4 6 8; do
        for rounds in 1 2 3; do
            echo "Running: Alg=$alg, Procs=$procs, Rounds=$rounds"
            time_output=$(mpirun -np $procs ./exchange $alg $rounds 2>&1 | grep "completed in")
            time=$(echo $time_output | awk '{print $4}')
            echo "$alg,$procs,$rounds,$time" >> $OUTPUT_FILE
        done
    done
done

echo "Results saved to $OUTPUT_FILE"
```

### 7. Plotting Results (Python)

```python
# plot_results.py
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('results.csv')

algorithms = {1: 'Ring', 2: 'All-to-All', 3: 'Butterfly'}

for rounds in [1, 2, 3]:
    plt.figure(figsize=(10, 6))
    for alg in [1, 2, 3]:
        data = df[(df['Algorithm'] == alg) & (df['Rounds'] == rounds)]
        plt.plot(data['Processes'], data['Time'],
                marker='o', label=algorithms[alg])

    plt.xlabel('Number of Processes')
    plt.ylabel('Execution Time (seconds)')
    plt.title(f'Performance Comparison - {rounds} Rounds')
    plt.legend()
    plt.grid(True)
    plt.savefig(f'performance_rounds_{rounds}.png')
    plt.close()

print("Plots saved")
```

## Testing and Validation

### 1. Correctness Tests
```c
void verify_exchange_correctness(int *values, int count, int expected_count) {
    if (count != expected_count) {
        printf("ERROR: Expected %d values, got %d\n", expected_count, count);
    }

    // Check for duplicates
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (values[i] == values[j]) {
                printf("ERROR: Duplicate value %d\n", values[i]);
            }
        }
    }
}
```

### 2. Expected Results

**After 1 round**:
- Ring: Each process has 2 values (own + neighbor's)
- All-to-All: Each process has n values (full exchange)
- Butterfly: Each process has 2 values (own + partner's)

**After log₂(n) rounds** (for n=8, 3 rounds):
- Ring: Each process has 4 values
- All-to-All: Each process has n values (same as round 1)
- Butterfly: Each process has n values (full coverage)

## Expected Learning Outcomes
1. Different distributed exchange patterns and their trade-offs
2. Message complexity analysis in distributed algorithms
3. Performance measurement and profiling of MPI programs
4. Deadlock prevention strategies in peer-to-peer communication
5. Use of MPI collective operations (MPI_Allgather)
6. Scalability analysis: bandwidth vs latency trade-offs
7. Experimental evaluation and visualization of parallel algorithms
8. Understanding logarithmic dissemination (hypercube pattern)
