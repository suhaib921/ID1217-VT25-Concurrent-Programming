# Question 3: Dining Philosophers (Distributed) - Analysis

## Summary of the Task
Implement a distributed simulation of the classic dining philosophers problem:
- **5 philosopher processes**: Each thinking and eating
- **1 server process**: Represents/manages the table and forks (resources)
- **Simulation**: Random sleep times for eating and thinking
- **Output**: Trace of interesting events (pick up forks, eating, put down forks, thinking)
- **Distribution**: MPI-based distributed application

## Implementation Status
**NOT IMPLEMENTED** - Only the problem description exists. No code files are present in this directory.

## Required Implementation Approach

### Architecture
This problem requires a **client-server distributed architecture** using MPI:
- 1 server process (rank 0) - manages 5 forks
- 5 philosopher processes (ranks 1-5) - compete for forks
- Total MPI processes: 6

### Classic Dining Philosophers Problem
```
Setup:
- 5 philosophers sit at a round table
- 5 forks placed between philosophers
- Philosopher i needs forks i and (i+1) % 5 to eat
- Philosophers alternate: THINKING → HUNGRY → EATING → THINKING

Challenge:
- Prevent deadlock (all grab left fork simultaneously)
- Prevent starvation (some philosopher never eats)
- Maximize concurrency (multiple philosophers eat simultaneously)
```

### Algorithm Design

#### Server (Table Manager) Side:
```
State: fork_available[5] = {true, true, true, true, true}
       waiting_queue[5] = empty queues (optional for fairness)

Loop:
1. Receive message from any philosopher:

   a. REQUEST_FORKS(philosopher_id, left_fork, right_fork):
      - If both forks available:
        * Mark forks as unavailable
        * Send GRANT_FORKS to philosopher
      - Else:
        * Queue request (optional)
        * Don't respond yet

   b. RELEASE_FORKS(philosopher_id, left_fork, right_fork):
      - Mark forks as available
      - Check queue for waiting requests that can now be granted
      - Send GRANT_FORKS to waiting philosophers if possible

2. Continue until termination condition
```

#### Philosopher Side:
```
Loop for N iterations (or indefinite):
1. THINKING state:
   - Sleep random time (simulate thinking)
   - Print "Philosopher X is thinking"

2. HUNGRY state:
   - Calculate left_fork = id - 1
   - Calculate right_fork = id % 5
   - Send REQUEST_FORKS(id, left_fork, right_fork) to server
   - Wait for GRANT_FORKS response
   - Print "Philosopher X picked up forks"

3. EATING state:
   - Sleep random time (simulate eating)
   - Print "Philosopher X is eating"

4. Release forks:
   - Send RELEASE_FORKS(id, left_fork, right_fork) to server
   - Print "Philosopher X put down forks"

5. Return to THINKING
```

## Correctness Considerations

### Synchronization Requirements
1. **Mutual Exclusion on Forks**: Each fork used by at most one philosopher
2. **Atomicity**: Philosopher acquires both forks atomically (no partial acquisition)
3. **Deadlock Freedom**: System must avoid circular waiting
4. **Starvation Freedom**: Every hungry philosopher eventually eats

### Deadlock Prevention Strategies

#### Strategy 1: Resource Ordering (Asymmetric Solution)
```c
// Philosopher requests lower-numbered fork first
int first_fork = min(left_fork, right_fork);
int second_fork = max(left_fork, right_fork);
```
- **Pros**: Simple, guarantees deadlock-free
- **Cons**: Not natural to distributed setting (requires two-phase acquisition)

#### Strategy 2: Server-Side Atomic Allocation
```c
// Server only grants when BOTH forks available
if (fork_available[left] && fork_available[right]) {
    fork_available[left] = false;
    fork_available[right] = false;
    send_grant();
}
```
- **Pros**: Matches problem design, atomic, deadlock-free
- **Cons**: Centralized bottleneck, potential starvation without fairness

#### Strategy 3: Limiting Concurrent Diners
```c
// Allow at most 4 philosophers to attempt eating simultaneously
semaphore room = 4;
```
- **Pros**: Guarantees at least one philosopher can acquire both forks
- **Cons**: Reduces parallelism

### Starvation Prevention
**Problem**: FIFO server processing doesn't guarantee fairness

**Solutions**:
1. **Request Queue**: Server maintains FIFO queue per philosopher
2. **Timestamps**: Priority to longest-waiting philosopher
3. **Fairness Counter**: Track how many times each philosopher ate
4. **Random Selection**: When multiple requests can be granted, choose randomly

### Race Conditions
1. **Concurrent Requests**: Multiple philosophers request simultaneously
   - **Safe**: Server processes sequentially (MPI_Recv is sequential)
2. **Fork Release and Re-request**: Fork released, immediately re-requested
   - **Fair**: Use queue to give others a chance
3. **Message Ordering**: MPI guarantees FIFO between process pairs
   - **Safe**: Each philosopher's messages arrive in order

## Performance Considerations

### Message Complexity
- **Per Eating Session**:
  - 1 REQUEST_FORKS message (philosopher → server)
  - 1 GRANT_FORKS message (server → philosopher)
  - 1 RELEASE_FORKS message (philosopher → server)
  - Total: 3 messages per eating session

- **For N iterations with 5 philosophers**:
  - Total messages: 5 × N × 3 = 15N messages

### Time Complexity
- **Server**: O(1) per request (with simple availability check)
- **With Queue**: O(P) per request where P = number of philosophers
- **Bottleneck**: Server is centralized coordinator

### Scalability
- **Good**: Small number of philosophers (5) makes centralized approach viable
- **Poor**: Doesn't scale well to hundreds of philosophers
- **Alternative**: Distributed fork management (each fork is a process)

### Concurrency
- **Maximum Parallelism**: 2 philosophers can eat simultaneously (in 5-philosopher setup)
  - Philosophers 0 and 2 can eat together (no shared forks)
  - Philosophers 0 and 3 can eat together
  - Philosophers 1 and 3 can eat together
  - Philosophers 1 and 4 can eat together
  - Philosophers 2 and 4 can eat together

## Implementation Recommendations

### 1. Data Structures

#### Server Side:
```c
typedef struct {
    bool available;
    int held_by;  // -1 if available, philosopher_id otherwise
} Fork;

typedef struct {
    int philosopher_id;
    int left_fork;
    int right_fork;
    time_t timestamp;  // For fairness
} Request;

Fork forks[5];
Request waiting_queue[MAX_QUEUE_SIZE];
int queue_length = 0;
```

#### Philosopher Side:
```c
typedef enum {
    THINKING,
    HUNGRY,
    EATING
} PhilosopherState;

int my_id;
int left_fork;
int right_fork;
PhilosopherState state = THINKING;
```

### 2. Message Protocol
```c
// Message tags
#define REQUEST_FORKS   1
#define GRANT_FORKS     2
#define RELEASE_FORKS   3
#define TERMINATE       4

// Message structure (can use MPI derived types or simple arrays)
typedef struct {
    int philosopher_id;
    int left_fork;
    int right_fork;
} ForkMessage;
```

### 3. Random Sleep Implementation
```c
void random_sleep(int min_ms, int max_ms) {
    int sleep_time = min_ms + (rand() % (max_ms - min_ms + 1));
    usleep(sleep_time * 1000);  // Convert to microseconds
}

// Usage
random_sleep(100, 500);  // Think for 100-500ms
random_sleep(50, 300);   // Eat for 50-300ms
```

### 4. Trace Output Format
```
[Time 0.000s] Philosopher 0 is thinking
[Time 0.150s] Philosopher 0 is hungry
[Time 0.151s] Philosopher 0 picked up forks 4 and 0
[Time 0.151s] Philosopher 0 is eating
[Time 0.300s] Philosopher 0 put down forks 4 and 0
[Time 0.300s] Philosopher 0 is thinking
```

### 5. Termination Handling
```c
// Option 1: Fixed number of eating sessions
for (int i = 0; i < NUM_SESSIONS; i++) {
    thinking(); hungry(); eating();
}

// Option 2: Time-based termination
time_t start = time(NULL);
while (time(NULL) - start < SIMULATION_TIME) {
    thinking(); hungry(); eating();
}

// Option 3: Coordinator sends TERMINATE message
```

## Concurrency and Distribution Issues

### MPI-Specific Implementation
```c
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 6) {
        if (rank == 0) {
            printf("Error: Must run with exactly 6 processes\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (rank == 0) {
        table_server();  // Manage forks
    } else {
        philosopher(rank);  // Philosopher 1-5
    }

    MPI_Finalize();
    return 0;
}
```

### Server Implementation:
```c
void table_server() {
    bool fork_available[5] = {true, true, true, true, true};
    int active_philosophers = 5;

    while (active_philosophers > 0) {
        MPI_Status status;
        ForkMessage msg;

        MPI_Recv(&msg, sizeof(ForkMessage), MPI_BYTE,
                 MPI_ANY_SOURCE, MPI_ANY_TAG,
                 MPI_COMM_WORLD, &status);

        switch (status.MPI_TAG) {
            case REQUEST_FORKS:
                if (fork_available[msg.left_fork] &&
                    fork_available[msg.right_fork]) {
                    fork_available[msg.left_fork] = false;
                    fork_available[msg.right_fork] = false;
                    MPI_Send(&msg, sizeof(ForkMessage), MPI_BYTE,
                             status.MPI_SOURCE, GRANT_FORKS,
                             MPI_COMM_WORLD);
                }
                // Otherwise, queue request (not shown)
                break;

            case RELEASE_FORKS:
                fork_available[msg.left_fork] = true;
                fork_available[msg.right_fork] = true;
                // Check queued requests (not shown)
                break;

            case TERMINATE:
                active_philosophers--;
                break;
        }
    }
}
```

### Philosopher Implementation:
```c
void philosopher(int id) {
    int left_fork = (id - 1 + 5) % 5;  // Adjust for 0-based indexing
    int right_fork = id % 5;
    ForkMessage msg = {id, left_fork, right_fork};

    srand(time(NULL) + id);  // Unique seed per philosopher

    for (int i = 0; i < NUM_SESSIONS; i++) {
        // THINKING
        printf("Philosopher %d is thinking\n", id);
        random_sleep(100, 500);

        // HUNGRY
        printf("Philosopher %d is hungry\n", id);
        MPI_Send(&msg, sizeof(ForkMessage), MPI_BYTE,
                 0, REQUEST_FORKS, MPI_COMM_WORLD);

        // Wait for grant
        MPI_Recv(&msg, sizeof(ForkMessage), MPI_BYTE,
                 0, GRANT_FORKS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Philosopher %d picked up forks %d and %d\n",
               id, left_fork, right_fork);

        // EATING
        printf("Philosopher %d is eating\n", id);
        random_sleep(50, 300);

        // Release forks
        printf("Philosopher %d put down forks %d and %d\n",
               id, left_fork, right_fork);
        MPI_Send(&msg, sizeof(ForkMessage), MPI_BYTE,
                 0, RELEASE_FORKS, MPI_COMM_WORLD);
    }

    // Notify server of completion
    MPI_Send(&msg, sizeof(ForkMessage), MPI_BYTE,
             0, TERMINATE, MPI_COMM_WORLD);
}
```

## Testing and Validation

### 1. Correctness Tests
- **Fork Exclusion**: No two philosophers hold the same fork
- **No Deadlock**: System makes progress, philosophers eventually eat
- **No Starvation**: All philosophers eat roughly equal number of times
- **Proper Termination**: All processes exit cleanly

### 2. Test Scenarios
```bash
# Short run
mpirun -np 6 ./dining_philosophers 5

# Long run to check starvation
mpirun -np 6 ./dining_philosophers 100

# With verbose logging
mpirun -np 6 ./dining_philosophers 10 --verbose
```

### 3. Metrics to Collect
- Number of times each philosopher ate
- Average waiting time per philosopher
- Total simulation time
- Maximum concurrent diners

### 4. Debugging
```c
// Add detailed logging
#ifdef DEBUG
    printf("[%d] State: %s, Forks: %d,%d\n",
           rank, state_string(state), left_fork, right_fork);
#endif

// Track fork state
void print_fork_state(bool forks[]) {
    printf("Forks: ");
    for (int i = 0; i < 5; i++) {
        printf("%c ", forks[i] ? 'A' : 'U');
    }
    printf("\n");
}
```

## Advanced Enhancements

### 1. Fairness Improvement
```c
// Server maintains eat count
int eat_count[5] = {0};

// Prioritize philosopher who ate least
int find_hungriest() {
    int min_count = INT_MAX;
    int hungriest = -1;
    for (int i = 0; i < 5; i++) {
        if (waiting[i] && eat_count[i] < min_count) {
            min_count = eat_count[i];
            hungriest = i;
        }
    }
    return hungriest;
}
```

### 2. Statistics Collection
```c
typedef struct {
    int times_ate;
    int total_wait_time;
    int total_eat_time;
} PhilosopherStats;
```

### 3. Distributed Fork Management
Alternative architecture: Each fork is a separate process
- 5 philosopher processes
- 5 fork processes
- 1 coordinator process
- Total: 11 processes
- More scalable but more complex

## Expected Learning Outcomes
1. Classic concurrency problem in distributed setting
2. Resource allocation and deadlock prevention
3. Centralized vs distributed coordination
4. MPI client-server patterns
5. Fairness and starvation in distributed systems
6. Event tracing in concurrent systems
7. Random simulation in parallel programs
