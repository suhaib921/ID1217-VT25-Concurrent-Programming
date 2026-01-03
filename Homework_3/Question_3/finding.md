# Question 3: The One-Lane Bridge Problem - Analysis

## Summary of the Question/Task

**Problem Type**: Readers-Writers Variant (Directional Access Control)

**Scenario**:
- One-lane bridge shared by cars from north and south
- Cars heading in same direction can cross simultaneously
- Cars heading in opposite directions cannot cross simultaneously
- Each car crosses bridge multiple times, alternating directions

**Requirements**:

**Part (a) - Basic Version (20 points)**:
- Use ONLY semaphores for synchronization
- Each car crosses "trips" times, alternating directions
- Odd-numbered cars start northbound, even-numbered cars start southbound
- Random sleep between crossings (travel time)
- Shorter random sleep while crossing (crossing time)
- Do NOT worry about fairness
- Print trace of simulation events

**Part (b) - Enhanced Version (Extra 20 points)**:
Choose ONE of two options:
1. **Fairness**: Ensure any car waiting eventually crosses (prevent starvation)
2. **FIFO Order**: Cars leave bridge in same order they entered

## Implementation Approach Analysis

### Part (a): Basic Implementation Without Fairness

**Required Synchronization Primitives**:

1. **mutex** (binary semaphore, initial = 1): Protects shared state variables
2. **northQueue** (binary semaphore, initial = 1): Controls northbound car entry
3. **southQueue** (binary semaphore, initial = 1): Controls southbound car entry

**Shared Variables**:
- `northCount` (int): Number of cars currently crossing northbound
- `southCount` (int): Number of cars currently crossing southbound

**Basic Implementation Pattern**:

```c
// Shared variables
int northCount = 0;
int southCount = 0;
sem_t mutex;
sem_t northQueue;
sem_t southQueue;

// Initialization
sem_init(&mutex, 0, 1);
sem_init(&northQueue, 0, 1);
sem_init(&southQueue, 0, 1);

// Car thread
void* car_thread(void* arg) {
    int car_id = *(int*)arg;
    int direction = (car_id % 2 == 1) ? NORTH : SOUTH;  // Odd=North, Even=South

    for (int trip = 0; trip < TRIPS; trip++) {
        // Sleep before approaching bridge
        sleep(rand() % MAX_TRAVEL_TIME);

        if (direction == NORTH) {
            arrive_north(car_id);
            cross_north(car_id);
            exit_north(car_id);
        } else {
            arrive_south(car_id);
            cross_south(car_id);
            exit_south(car_id);
        }

        // Alternate direction
        direction = (direction == NORTH) ? SOUTH : NORTH;
    }
    return NULL;
}

// Northbound arrival and crossing
void arrive_north(int car_id) {
    sem_wait(&northQueue);  // Wait for northbound entry permission
    sem_wait(&mutex);

    northCount++;
    if (northCount == 1) {
        // First northbound car blocks southbound traffic
        sem_wait(&southQueue);
    }

    sem_post(&mutex);
    sem_post(&northQueue);  // Allow next northbound car
}

void cross_north(int car_id) {
    printf("Car %d crossing NORTH (cars on bridge: %d)\n", car_id, northCount);
    usleep((rand() % MAX_CROSSING_TIME) * 1000);  // Crossing time
}

void exit_north(int car_id) {
    sem_wait(&mutex);

    northCount--;
    printf("Car %d exited NORTH (cars on bridge: %d)\n", car_id, northCount);

    if (northCount == 0) {
        // Last northbound car releases southbound traffic
        sem_post(&southQueue);
    }

    sem_post(&mutex);
}

// Southbound arrival and crossing (symmetric)
void arrive_south(int car_id) {
    sem_wait(&southQueue);
    sem_wait(&mutex);

    southCount++;
    if (southCount == 1) {
        sem_wait(&northQueue);
    }

    sem_post(&mutex);
    sem_post(&southQueue);
}

void cross_south(int car_id) {
    printf("Car %d crossing SOUTH (cars on bridge: %d)\n", car_id, southCount);
    usleep((rand() % MAX_CROSSING_TIME) * 1000);
}

void exit_south(int car_id) {
    sem_wait(&mutex);

    southCount--;
    printf("Car %d exited SOUTH (cars on bridge: %d)\n", car_id, southCount);

    if (southCount == 0) {
        sem_post(&northQueue);
    }

    sem_post(&mutex);
}
```

### Part (b): Enhanced Implementation with Fairness

**Additional Synchronization Needed**:

1. **turnstile** (binary semaphore, initial = 1): Enforces FIFO ordering
2. **waiting_north** (counter): Number of northbound cars waiting
3. **waiting_south** (counter): Number of southbound cars waiting

**Fair Implementation Using Turnstile Pattern**:

```c
// Additional shared variables
int waiting_north = 0;
int waiting_south = 0;
sem_t turnstile;

sem_init(&turnstile, 0, 1);

// Fair northbound arrival
void arrive_north_fair(int car_id) {
    sem_wait(&turnstile);  // Enter turnstile (FIFO enforcer)

    sem_wait(&mutex);
    waiting_north++;

    // Check if southbound traffic has priority (fairness)
    if (northCount == 0 && southCount > 0) {
        sem_post(&mutex);
        sem_post(&turnstile);
        // Wait for southbound to clear
        sem_wait(&northQueue);
        sem_wait(&mutex);
    }

    northCount++;
    waiting_north--;

    if (northCount == 1) {
        sem_wait(&southQueue);  // Block southbound
    }

    sem_post(&mutex);
    sem_post(&turnstile);  // Release turnstile for next car
}

// Alternative: Ticket-based fairness
typedef struct {
    int ticket_counter;
    int now_serving;
    sem_t service_queue;
} TicketSystem;

TicketSystem bridge_ticket;

void arrive_with_ticket(int car_id, int direction) {
    sem_wait(&mutex);
    int my_ticket = bridge_ticket.ticket_counter++;
    sem_post(&mutex);

    // Wait for my turn
    while (my_ticket != bridge_ticket.now_serving) {
        usleep(100);
    }

    // Proceed with normal arrival logic
    if (direction == NORTH) {
        // ... northbound logic
    } else {
        // ... southbound logic
    }

    // After crossing:
    sem_wait(&mutex);
    bridge_ticket.now_serving++;
    sem_post(&mutex);
}
```

### Part (b): FIFO Order Implementation

**Approach**: Use a queue to track entry order

```c
typedef struct {
    int car_id;
    int direction;
    sem_t personal_sem;
} CarEntry;

CarEntry entry_queue[MAX_CARS];
int queue_head = 0;
int queue_tail = 0;
sem_t queue_mutex;

void arrive_fifo(int car_id, int direction) {
    // Join queue
    sem_wait(&queue_mutex);
    int my_position = queue_tail;
    entry_queue[my_position].car_id = car_id;
    entry_queue[my_position].direction = direction;
    sem_init(&entry_queue[my_position].personal_sem, 0, 0);
    queue_tail++;
    sem_post(&queue_mutex);

    // Wait for my turn
    sem_wait(&entry_queue[my_position].personal_sem);

    // Cross bridge
    cross_bridge(car_id, direction);

    // Signal next in queue or handle exit
    sem_wait(&queue_mutex);
    queue_head++;

    // Check if next car can go (same direction or bridge empty)
    if (queue_head < queue_tail) {
        int next_dir = entry_queue[queue_head].direction;
        if (next_dir == direction || bridge_is_empty()) {
            sem_post(&entry_queue[queue_head].personal_sem);
        }
    }

    sem_post(&queue_mutex);
}
```

## Correctness Assessment

### Part (a): Basic Implementation

**Correctness Requirements**:

1. **Mutual Exclusion of Directions**:
   - Northbound and southbound cars cannot be on bridge simultaneously
   - VERIFICATION: Check that northCount > 0 and southCount > 0 never occur together

2. **Multiple Cars Same Direction**:
   - Multiple cars going same direction can cross simultaneously
   - CORRECT: northCount or southCount can be > 1

3. **First Car Blocks Opposite Direction**:
   - When first northbound car arrives, it blocks southbound (sem_wait(&southQueue))
   - When first southbound car arrives, it blocks northbound (sem_wait(&northQueue))
   - CRITICAL: This must happen BEFORE incrementing count

4. **Last Car Releases Opposite Direction**:
   - When last northbound car exits (northCount becomes 0), release southbound
   - When last southbound car exits (southCount becomes 0), release northbound
   - CRITICAL: Check count AFTER decrementing

**Potential Correctness Issues**:

**Issue 1: Race Condition in First Car Logic**
```c
// WRONG - Race condition!
northCount++;
sem_post(&mutex);
if (northCount == 1) {  // Check outside critical section!
    sem_wait(&southQueue);
}
```
**FIX**: Check and block must be atomic:
```c
sem_wait(&mutex);
northCount++;
if (northCount == 1) {
    sem_wait(&southQueue);  // Block inside critical section
}
sem_post(&mutex);
```

**Issue 2: Deadlock Potential**
**Scenario**:
- Northbound car holds southQueue (southCount=1)
- Tries to acquire northQueue (blocked)
- Southbound car holds northQueue (northCount=1)
- Tries to acquire southQueue (blocked)

**This shouldn't happen with correct implementation**, but verify:
- Only the FIRST car in a direction blocks opposite direction
- Opposite direction is already blocked, so no contention

**Issue 3: Starvation (Intentional in Part a)**
- Continuous stream of northbound cars can starve southbound
- If northCount never reaches 0, southbound waits forever
- This is ACCEPTABLE for part (a) - fairness not required

### Part (b): Fair Implementation

**Additional Correctness Requirements**:

1. **Starvation Prevention**:
   - Every waiting car must eventually cross
   - No direction can monopolize bridge indefinitely

2. **FIFO Ordering** (if chosen):
   - Cars must exit in same order they arrived
   - Requires explicit queue management

**Fairness Verification**:
- Track waiting times for each car
- Ensure maximum waiting time is bounded
- No car waits indefinitely

## Synchronization and Concurrency Issues

### Issue 1: Reader-Writer Starvation Pattern

**Classic Reader-Writer Problem Analogy**:
- Northbound cars = Readers
- Southbound cars = Writers (or vice versa)
- Same starvation issues apply

**Writer Starvation**:
- If readers (northbound) keep arriving, writer (southbound) waits forever
- In bridge problem, either direction can starve the other

**Solutions**:
1. **Priority to Waiting Direction**:
   ```c
   if (waiting_south > 0 && northCount == 0) {
       // Give priority to waiting southbound cars
       // Don't allow new northbound cars to start
   }
   ```

2. **Turn-Based System**:
   ```c
   enum { NORTH_TURN, SOUTH_TURN } current_turn = NORTH_TURN;

   // Northbound car checks:
   while (current_turn != NORTH_TURN || southCount > 0) {
       usleep(100);
   }
   ```

3. **Time-Based Fairness**:
   ```c
   time_t north_start_time = 0;
   #define MAX_DIRECTION_TIME 5  // seconds

   if (northCount > 0 && difftime(time(NULL), north_start_time) > MAX_DIRECTION_TIME) {
       // Force direction change
       // Don't allow new northbound cars
   }
   ```

### Issue 2: Turnstile Implementation Issues

**Common Mistake**:
```c
// WRONG - turnstile doesn't enforce ordering
sem_wait(&turnstile);
sem_wait(&mutex);
// ... critical section
sem_post(&mutex);
sem_post(&turnstile);
```

**Why it's wrong**: Turnstile is released immediately, doesn't enforce FIFO

**Correct Turnstile Usage**:
```c
sem_wait(&turnstile);  // Enter turnstile one at a time

sem_wait(&mutex);
// Decide if can proceed or must wait
if (can_proceed) {
    // Update counts
    sem_post(&mutex);
    sem_post(&turnstile);  // Let next car through turnstile
    // Proceed to cross
} else {
    // Must wait
    waiting_count++;
    sem_post(&mutex);
    sem_post(&turnstile);  // Let next car through turnstile
    sem_wait(&my_semaphore);  // Wait for my turn
    // ... later, when signaled
}
```

### Issue 3: Direction Alternation Logic

**Car's Direction Alternation**:
```c
int direction = (car_id % 2 == 1) ? NORTH : SOUTH;

for (int trip = 0; trip < TRIPS; trip++) {
    // Cross in current direction
    // ...

    // Alternate
    direction = (direction == NORTH) ? SOUTH : NORTH;
}
```

**Correctness**:
- Odd cars: NORTH, SOUTH, NORTH, SOUTH, ...
- Even cars: SOUTH, NORTH, SOUTH, NORTH, ...
- CORRECT: Satisfies problem requirements

### Issue 4: Counting Errors

**Critical Section for Counts**:
```c
// Both increment and first-check must be atomic:
sem_wait(&mutex);
northCount++;
if (northCount == 1) {
    sem_wait(&southQueue);
}
sem_post(&mutex);

// Both decrement and last-check must be atomic:
sem_wait(&mutex);
northCount--;
if (northCount == 0) {
    sem_post(&southQueue);
}
sem_post(&mutex);
```

**Assertion Checks**:
```c
sem_wait(&mutex);
assert(northCount >= 0);
assert(southCount >= 0);
assert(!(northCount > 0 && southCount > 0));  // Never both on bridge
sem_post(&mutex);
```

## Fairness Analysis

### Part (a): Unfair by Design

**Expected Behavior**:
- Starvation is possible and acceptable
- One direction can monopolize bridge
- No fairness guarantees required

**Starvation Scenarios**:
1. Continuous northbound arrivals → southbound starves
2. Northbound cars on bridge → new northbound cars join → southbound never gets chance
3. Timing-dependent: If one direction arrives more frequently, it dominates

**Testing Starvation**:
```c
// Create many cars in one direction:
// 20 northbound (odd IDs: 1,3,5,...,39)
// 2 southbound (even IDs: 2,4)

// With random arrival times, southbound will likely starve
```

### Part (b): Achieving Fairness

**Definition of Fairness**:
1. **Weak Fairness**: No car waits indefinitely while others of opposite direction cross repeatedly
2. **Strong Fairness**: Bounded waiting time - cars cross within reasonable time
3. **Perfect Fairness**: Strict alternation or FIFO ordering

**Fairness Strategies**:

**1. Waiting Counter Priority**:
```c
int waiting_north = 0;
int waiting_south = 0;

// Before allowing new northbound cars to start:
if (waiting_south > 0 && northCount == 0) {
    // Don't allow new northbound, give south a chance
    sem_wait(&southQueue);  // Block self
    waiting_north++;
    sem_post(&mutex);

    // Wait for south to finish
    sem_wait(&northQueue);
    sem_wait(&mutex);
    waiting_north--;
}
```

**2. Strict Alternation**:
```c
enum Direction { NORTH, SOUTH } allowed_direction = NORTH;

// Car arrival:
while (allowed_direction != my_direction) {
    sem_post(&mutex);
    usleep(100);
    sem_wait(&mutex);
}

// Last car exiting:
if (count == 0) {
    allowed_direction = opposite_direction;
}
```

**3. Maximum Consecutive Cars**:
```c
#define MAX_CONSECUTIVE 5
int consecutive_north = 0;
int consecutive_south = 0;

// Northbound arrival:
if (consecutive_north >= MAX_CONSECUTIVE && waiting_south > 0) {
    // Force direction change
    consecutive_north = 0;
    consecutive_south = 0;
    // Block northbound, allow southbound
}
```

## Performance Considerations

### Throughput Analysis

**Basic Implementation**:
- Multiple cars same direction cross simultaneously
- High throughput when traffic is unidirectional
- Direction changes incur overhead (waiting for bridge to clear)

**Throughput Metrics**:
- Cars per second crossing bridge
- Average crossing time per car
- Bridge utilization percentage

**Factors Affecting Performance**:
1. **Number of Cars**: More cars → more contention
2. **Trip Count**: More trips per car → longer simulation
3. **Crossing Time**: Longer crossing → fewer cars per unit time
4. **Travel Time**: Longer travel → less contention at bridge
5. **Direction Distribution**: Balanced traffic → more direction changes

### Contention Points

**Mutex Contention**:
- All cars accessing shared state contend for mutex
- Short critical sections minimize impact
- O(n) contention with n cars

**Queue Semaphore Contention**:
- Cars waiting for their direction compete for queue semaphore
- Directional bottleneck

**Bridge Capacity**:
- Unlimited cars same direction (in theory)
- In practice, crossing time limits number of simultaneous crossers

### Fairness vs Performance Trade-off

**Unfair Implementation**:
- ADVANTAGE: Higher throughput (fewer direction changes)
- ADVANTAGE: Simpler logic, less overhead
- DISADVANTAGE: Starvation possible, poor average waiting time for minority direction

**Fair Implementation**:
- ADVANTAGE: Bounded waiting time, predictable behavior
- ADVANTAGE: Better average waiting time across all cars
- DISADVANTAGE: More direction changes → lower throughput
- DISADVANTAGE: Additional synchronization overhead

**FIFO Implementation**:
- ADVANTAGE: Predictable order, perfect fairness
- ADVANTAGE: No starvation possible
- DISADVANTAGE: Lowest throughput (no parallelism of same direction)
- DISADVANTAGE: Complex queue management overhead

## Recommendations and Improvements

### Part (a): Basic Implementation

**1. Correct Synchronization Pattern**:
```c
// Ensure atomic first-car and last-car logic
void arrive_north(int car_id) {
    sem_wait(&northQueue);
    sem_wait(&mutex);

    northCount++;
    if (northCount == 1) {
        sem_wait(&southQueue);  // First car blocks opposite direction
    }

    sem_post(&mutex);
    sem_post(&northQueue);
}

void exit_north(int car_id) {
    sem_wait(&mutex);

    northCount--;
    if (northCount == 0) {
        sem_post(&southQueue);  // Last car releases opposite direction
    }

    sem_post(&mutex);
}
```

**2. Initialization**:
```c
sem_init(&mutex, 0, 1);
sem_init(&northQueue, 0, 1);
sem_init(&southQueue, 0, 1);
```

**3. Error Checking**:
```c
if (sem_wait(&mutex) != 0) {
    perror("sem_wait failed");
    pthread_exit(NULL);
}
```

**4. Assertions**:
```c
assert(northCount >= 0 && southCount >= 0);
assert(!(northCount > 0 && southCount > 0));
```

### Part (b): Fair Implementation

**5. Implement Waiting Counters**:
```c
int waiting_north = 0;
int waiting_south = 0;

// Priority to waiting direction when bridge clears
```

**6. Direction Change Policy**:
```c
// When last car of a direction exits:
if (northCount == 0 && waiting_south > 0) {
    // Don't allow more north, give south priority
    // Signal waiting south cars
}
```

**7. Prevent Starvation**:
```c
// Maximum time one direction can hold bridge:
#define MAX_HOLD_TIME 10  // seconds

time_t direction_start_time;

// Check timeout and force direction change if needed
```

### FIFO Implementation

**8. Queue Data Structure**:
```c
typedef struct {
    int car_id;
    int direction;
    sem_t ready;
    struct QueueNode* next;
} QueueNode;

QueueNode* queue_head = NULL;
QueueNode* queue_tail = NULL;
```

**9. Entry and Exit Management**:
```c
// On arrival: Add to queue
// On bridge clear: Signal next in queue if same direction or bridge empty
// On exit: Check if next car can enter
```

### Testing and Validation

**10. Starvation Test**:
```c
// Create imbalanced traffic:
// 90% northbound, 10% southbound
// Verify southbound cars eventually cross (part b)
```

**11. Correctness Test**:
```c
// Log all bridge crossings with timestamps
// Verify no northbound and southbound overlap
// Verify all cars complete all trips
```

**12. Fairness Test**:
```c
// Track waiting times for each car
// Calculate average and maximum waiting time
// Ensure no car waits indefinitely

int waiting_times[MAX_CARS] = {0};
time_t arrival_times[MAX_CARS];

// On arrival: Record time
// On entry: Calculate waiting time
// After simulation: Analyze statistics
```

**13. Performance Test**:
```bash
# Vary parameters:
./bridge 10 5   # 10 cars, 5 trips each
./bridge 50 10  # 50 cars, 10 trips each
./bridge 100 3  # 100 cars, 3 trips each

# Measure:
# - Total simulation time
# - Average crossing time
# - Bridge utilization
# - Direction changes per minute
```

## Example Output Format

**Part (a) - Basic Implementation**:
```
Simulation started: 10 cars, 5 trips each

Car 1 (North) approaching bridge
Car 1 (North) entering bridge [North: 1, South: 0]
Car 3 (North) approaching bridge
Car 1 (North) crossing bridge...
Car 3 (North) entering bridge [North: 2, South: 0]
Car 2 (South) approaching bridge
Car 2 (South) waiting (North traffic on bridge)
Car 3 (North) crossing bridge...
Car 1 (North) exited bridge [North: 1, South: 0]
Car 3 (North) exited bridge [North: 0, South: 0]
Car 2 (South) entering bridge [North: 0, South: 1]
...
```

**Part (b) - Fair Implementation**:
```
Simulation started: 10 cars, 5 trips each (FAIR MODE)

Car 1 (North) approaching bridge
Car 1 (North) entering bridge [North: 1, South: 0]
Car 2 (South) approaching bridge (added to queue)
Car 3 (North) approaching bridge (added to queue)
Car 1 (North) exited bridge [North: 0, South: 0]
Car 2 (South) entering bridge (priority: waiting) [North: 0, South: 1]
Car 2 (South) exited bridge [North: 0, South: 0]
Car 3 (North) entering bridge (from queue) [North: 1, South: 0]
...

Statistics:
  Total crossings: 100
  Direction changes: 45
  Average waiting time: 1.2 seconds
  Max waiting time: 3.5 seconds
  Bridge utilization: 67%
```

## Conclusion

The One-Lane Bridge Problem is a sophisticated variant of the Readers-Writers problem that demonstrates:

**Key Concepts Tested**:
1. Multiple readers (same direction) can proceed simultaneously
2. Readers and writers (opposite directions) are mutually exclusive
3. First reader blocks writers, last reader unblocks writers
4. Fairness vs performance trade-offs
5. Starvation prevention techniques

**Part (a) Key Challenges**:
- Implementing first-car and last-car logic correctly
- Ensuring mutual exclusion between directions
- Allowing parallelism within same direction
- Understanding that starvation is acceptable

**Part (b) Key Challenges**:
- Preventing starvation of either direction
- Implementing fair scheduling policies
- FIFO ordering requires explicit queue management
- Balancing fairness with performance

**Common Mistakes**:
- Non-atomic first-car or last-car checks
- Releasing direction lock prematurely
- Not handling direction alternation correctly
- Incorrect semaphore initialization
- Missing synchronization in count updates
- Ignoring fairness implications

**Success Criteria (Part a)**:
- No northbound and southbound cars on bridge simultaneously
- Multiple same-direction cars can cross together
- All cars complete all trips
- No deadlocks or race conditions

**Success Criteria (Part b)**:
- All part (a) requirements plus:
- No car waits indefinitely (fairness option)
- OR cars exit in entry order (FIFO option)
- Bounded waiting time
- Correct handling of priority/queueing
