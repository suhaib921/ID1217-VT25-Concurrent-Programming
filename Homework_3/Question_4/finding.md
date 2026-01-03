# Question 4: The Unisex Bathroom Problem - Analysis

## Summary of the Question/Task

**Problem Type**: Fair Readers-Writers Problem (Gender-based Access Control)

**Scenario**:
- Single bathroom shared by men and women
- Any number of same gender can use bathroom simultaneously
- Men and women cannot use bathroom simultaneously
- People repeatedly work (sleep) and use bathroom
- Must ensure fairness: no gender is starved

**Requirements**:
- Use ONLY semaphores for synchronization
- Allow multiple same-gender people in bathroom simultaneously
- Ensure mutual exclusion between genders
- Avoid deadlock
- Ensure FAIRNESS: every person eventually gets to use bathroom
- Random work time between bathroom visits
- Shorter random time in bathroom
- Print trace of simulation events
- Worth 40 points (double other problems - fairness required!)

**Key Challenge**: This is explicitly the Readers-Writers problem with fairness requirement

## Implementation Approach Analysis

### Why This Problem is Challenging

**Classic Readers-Writers Starvation**:
- Without fairness, one gender can monopolize bathroom
- Continuous stream of men can starve women (or vice versa)
- Similar to bridge problem but fairness is REQUIRED, not optional

**Fairness is Hard with Only Semaphores**:
- POSIX semaphores don't guarantee FIFO by default
- Must implement explicit fairness mechanism
- Need to track waiting persons and prioritize starved gender

### Solution Approaches

**Approach 1: Unfair (INCORRECT for this problem)**
- Simple readers-writers pattern
- First person of gender blocks opposite gender
- Last person of gender releases opposite gender
- PROBLEM: Starvation possible, doesn't satisfy requirements

**Approach 2: Turnstile Pattern (Correct)**
- Use turnstile semaphore to enforce ordering
- All persons pass through turnstile before accessing bathroom
- Prevents continuous stream of same gender

**Approach 3: Priority to Waiting Gender (Correct)**
- Track number of waiting persons per gender
- Give priority to gender with waiting persons when bathroom empties
- Prevents starvation by forcing alternation when opposite gender waits

**Approach 4: Ticket System (Most Fair)**
- Each person gets a ticket number
- Bathroom serves persons in ticket order
- Perfect FIFO fairness
- More complex to implement with only semaphores

### Recommended Implementation: Turnstile Pattern

**Required Synchronization Primitives**:

1. **mutex** (binary semaphore, initial = 1): Protects shared state
2. **menQueue** (binary semaphore, initial = 1): Controls men's entry
3. **womenQueue** (binary semaphore, initial = 1): Controls women's entry
4. **turnstile** (binary semaphore, initial = 1): Enforces fairness

**Shared Variables**:
- `menCount` (int): Number of men currently in bathroom
- `womenCount` (int): Number of women currently in bathroom
- `menWaiting` (int): Number of men waiting to enter
- `womenWaiting` (int): Number of women waiting to enter

**Implementation Pattern**:

```c
// Shared state
int menCount = 0;
int womenCount = 0;
int menWaiting = 0;
int womenWaiting = 0;

sem_t mutex;
sem_t menQueue;
sem_t womenQueue;
sem_t turnstile;

// Initialization
sem_init(&mutex, 0, 1);
sem_init(&menQueue, 0, 1);
sem_init(&womenQueue, 0, 1);
sem_init(&turnstile, 0, 1);

// Man thread
void* man_thread(void* arg) {
    int id = *(int*)arg;

    while (simulation_running) {
        // Work (sleep)
        work(id, MAN);

        // Enter bathroom
        man_enter(id);

        // Use bathroom
        use_bathroom(id, MAN);

        // Exit bathroom
        man_exit(id);
    }
    return NULL;
}

// Man entering bathroom (with fairness)
void man_enter(int id) {
    // Pass through turnstile (fairness enforcer)
    sem_wait(&turnstile);
    sem_post(&turnstile);

    sem_wait(&menQueue);

    sem_wait(&mutex);
    menWaiting++;

    // Wait if women are in bathroom
    while (womenCount > 0) {
        sem_post(&mutex);
        usleep(1000);  // Brief wait
        sem_wait(&mutex);
    }

    menCount++;
    menWaiting--;

    if (menCount == 1) {
        // First man blocks women
        sem_wait(&womenQueue);
    }

    printf("Man %d entered bathroom [Men: %d, Women: %d]\n", id, menCount, womenCount);

    sem_post(&mutex);
    sem_post(&menQueue);
}

void man_exit(int id) {
    sem_wait(&mutex);

    menCount--;
    printf("Man %d left bathroom [Men: %d, Women: %d]\n", id, menCount, womenCount);

    if (menCount == 0) {
        // Last man releases women
        sem_post(&womenQueue);
    }

    sem_post(&mutex);
}

// Woman entering bathroom (symmetric)
void woman_enter(int id) {
    sem_wait(&turnstile);
    sem_post(&turnstile);

    sem_wait(&womenQueue);

    sem_wait(&mutex);
    womenWaiting++;

    while (menCount > 0) {
        sem_post(&mutex);
        usleep(1000);
        sem_wait(&mutex);
    }

    womenCount++;
    womenWaiting--;

    if (womenCount == 1) {
        sem_wait(&menQueue);
    }

    printf("Woman %d entered bathroom [Men: %d, Women: %d]\n", id, menCount, womenCount);

    sem_post(&mutex);
    sem_post(&womenQueue);
}

void woman_exit(int id) {
    sem_wait(&mutex);

    womenCount--;
    printf("Woman %d left bathroom [Men: %d, Women: %d]\n", id, menCount, womenCount);

    if (womenCount == 0) {
        sem_post(&menQueue);
    }

    sem_post(&mutex);
}
```

### Enhanced Implementation with Priority Queue

For stronger fairness guarantees:

```c
typedef enum { MAN, WOMAN } Gender;

typedef struct {
    int person_id;
    Gender gender;
    time_t arrival_time;
    sem_t personal_sem;
} WaitingPerson;

WaitingPerson waiting_queue[MAX_PERSONS];
int queue_head = 0;
int queue_tail = 0;
sem_t queue_mutex;

void enter_bathroom_fair(int id, Gender gender) {
    // Join waiting queue
    sem_wait(&queue_mutex);

    int my_position = queue_tail;
    waiting_queue[my_position].person_id = id;
    waiting_queue[my_position].gender = gender;
    waiting_queue[my_position].arrival_time = time(NULL);
    sem_init(&waiting_queue[my_position].personal_sem, 0, 0);

    queue_tail++;

    // Check if I can enter immediately
    if (can_enter(my_position)) {
        // Bathroom is empty or same gender
        enter_bathroom(id, gender);
        queue_head++;
        sem_post(&queue_mutex);
        return;
    }

    sem_post(&queue_mutex);

    // Wait for my turn
    sem_wait(&waiting_queue[my_position].personal_sem);

    // Enter bathroom
    enter_bathroom(id, gender);
}

void exit_bathroom_fair(int id, Gender gender) {
    sem_wait(&mutex);

    if (gender == MAN) {
        menCount--;
    } else {
        womenCount--;
    }

    int bathroom_empty = (menCount == 0 && womenCount == 0);

    sem_post(&mutex);

    if (bathroom_empty) {
        // Signal next person(s) in queue
        sem_wait(&queue_mutex);

        if (queue_head < queue_tail) {
            Gender next_gender = waiting_queue[queue_head].gender;

            // Signal all waiting persons of next gender
            while (queue_head < queue_tail &&
                   waiting_queue[queue_head].gender == next_gender) {
                sem_post(&waiting_queue[queue_head].personal_sem);
                queue_head++;
            }
        }

        sem_post(&queue_mutex);
    }
}
```

## Correctness Assessment

### Critical Correctness Requirements

**1. Mutual Exclusion Between Genders**:
- Men and women cannot be in bathroom simultaneously
- VERIFY: `menCount > 0 && womenCount > 0` should NEVER occur
- Must be enforced atomically

**2. Multiple Same Gender Allowed**:
- Multiple men can be in bathroom together (menCount > 1)
- Multiple women can be in bathroom together (womenCount > 1)
- This is REQUIRED behavior

**3. First Person Blocks Opposite Gender**:
```c
if (menCount == 1) {  // First man
    sem_wait(&womenQueue);  // Block all women
}
```
- Critical section ensures atomicity of check and block

**4. Last Person Releases Opposite Gender**:
```c
if (menCount == 0) {  // Last man leaving
    sem_post(&womenQueue);  // Allow women to enter
}
```
- Must check count AFTER decrementing

**5. No Deadlock**:
- Men wait for women to finish
- Women wait for men to finish
- No circular wait if implemented correctly
- Turnstile doesn't create circular dependencies

**6. Fairness (CRITICAL)**:
- Every person must eventually use bathroom
- No gender can be starved
- Requires explicit fairness mechanism

### Common Correctness Issues

**Issue 1: Unfair Simple Implementation**
```c
// WRONG - No fairness guarantee!
void man_enter(int id) {
    sem_wait(&menQueue);
    sem_wait(&mutex);

    menCount++;
    if (menCount == 1) {
        sem_wait(&womenQueue);
    }

    sem_post(&mutex);
    sem_post(&menQueue);
}
```
**Problem**: Continuous stream of men can starve women
**Solution**: Add turnstile or priority mechanism

**Issue 2: Incorrect Turnstile Usage**
```c
// WRONG - Turnstile doesn't help here!
sem_wait(&turnstile);
sem_wait(&menQueue);
// ... enter bathroom
sem_post(&menQueue);
sem_post(&turnstile);
```
**Problem**: Turnstile held too long, blocks all entry
**Correct**:
```c
sem_wait(&turnstile);  // Brief pass-through
sem_post(&turnstile);  // Immediately release

sem_wait(&menQueue);   // Then proceed with normal logic
// ...
```

**Issue 3: Race Condition in Count Checks**
```c
// WRONG - Non-atomic check and update
if (menCount == 1) {
    sem_post(&mutex);  // Released too early!
    sem_wait(&womenQueue);
    sem_wait(&mutex);
}
```
**Correct**:
```c
sem_wait(&mutex);
menCount++;
if (menCount == 1) {
    sem_wait(&womenQueue);  // Within critical section
}
sem_post(&mutex);
```

**Issue 4: Forgetting Waiting Counters**
- Need to track `menWaiting` and `womenWaiting`
- Used to determine if opposite gender should have priority
- Essential for fairness

**Issue 5: Starvation Despite Turnstile**
- Turnstile alone doesn't guarantee fairness
- Need to check waiting counters and give priority
- Or implement explicit queue

## Synchronization and Concurrency Issues

### Issue 1: Implementing True Fairness with Semaphores

**Challenge**: POSIX semaphores are not FIFO by default

**Solution Options**:

**A. Turnstile + Priority Logic**:
```c
void man_enter(int id) {
    sem_wait(&turnstile);
    sem_post(&turnstile);

    sem_wait(&menQueue);
    sem_wait(&mutex);

    // Priority to waiting women
    if (womenWaiting > 0 && menCount == 0) {
        // Give women a chance first
        sem_post(&mutex);
        sem_post(&menQueue);
        usleep(10000);  // Back off
        // Retry later
        return;  // Or loop
    }

    menCount++;
    // ... rest of entry logic
}
```

**B. Explicit FIFO Queue**:
- Maintain linked list or array of waiting persons
- Each person gets personal semaphore
- Controller signals next person(s) in order

**C. Ticket System**:
```c
int ticket_counter = 0;
int now_serving = 0;

// Get ticket
sem_wait(&mutex);
int my_ticket = ticket_counter++;
sem_post(&mutex);

// Wait for my ticket
while (my_ticket != now_serving) {
    usleep(100);
}

// Enter bathroom
// ...

// After exit, increment now_serving
```

### Issue 2: Batching Same Gender

**Efficiency Consideration**:
- When bathroom empties, allow ALL waiting persons of next gender to enter
- Maximizes parallelism
- Reduces gender alternation overhead

**Implementation**:
```c
void exit_bathroom(int id, Gender gender) {
    sem_wait(&mutex);

    if (gender == MAN) {
        menCount--;
    } else {
        womenCount--;
    }

    if (menCount == 0 && womenCount == 0) {
        // Bathroom empty - signal ALL waiting of one gender

        if (womenWaiting > 0) {
            // Signal all waiting women
            for (int i = 0; i < womenWaiting; i++) {
                sem_post(&womenQueue);
            }
        } else if (menWaiting > 0) {
            // Signal all waiting men
            for (int i = 0; i < menWaiting; i++) {
                sem_post(&menQueue);
            }
        }
    }

    sem_post(&mutex);
}
```

**Problem with Above**: sem_post() on counting semaphore accumulates
**Fix**: Use conditional entry logic to consume signals appropriately

### Issue 3: Preventing Barging

**Barging**: New arrival cuts ahead of waiting persons

**Example**:
- Women in bathroom
- Men waiting
- Woman exits, bathroom empty
- New woman arrives and enters before waiting men

**Solution**:
```c
// Check if opposite gender is waiting
sem_wait(&mutex);

if (menCount == 0 && womenWaiting == 0) {
    // No one waiting, can enter
    womenCount++;
} else if (menCount == 0 && womenWaiting > 0) {
    // Men waiting - must wait even though bathroom empty
    womenWaiting++;
    sem_post(&mutex);
    sem_wait(&womenQueue);
    sem_wait(&mutex);
    womenWaiting--;
    womenCount++;
}

sem_post(&mutex);
```

### Issue 4: Deadlock Prevention

**Potential Deadlock Scenario**:
- Man holds menQueue, waiting for women to leave
- Woman holds womenQueue, waiting for men to leave
- Both hold their queue semaphore indefinitely

**Why This Shouldn't Happen**:
- Queue semaphores are released after entering
- Not held while in bathroom
- First person blocks opposite queue, but doesn't hold own queue

**Verification**:
- Review all sem_wait() and sem_post() pairs
- Ensure no circular wait conditions
- Test with thread sanitizer

## Fairness Analysis

### What is Fairness in This Context?

**Fairness Requirements**:
1. **No Starvation**: Every person eventually uses bathroom
2. **Bounded Waiting**: Maximum wait time is reasonable
3. **No Gender Bias**: Neither gender systematically favored
4. **Responsive**: System responds to waiting persons

### Measuring Fairness

**Metrics**:
1. **Maximum Wait Time**: Longest any person waited
2. **Average Wait Time**: Per person, per gender
3. **Standard Deviation**: Lower = more fair
4. **Starvation Count**: Number of persons who never got access
5. **Gender Alternation Frequency**: How often bathroom changes gender

### Fairness Strategies

**Strategy 1: Strict Alternation**
```c
Gender allowed_gender = MAN;

void enter(int id, Gender my_gender) {
    while (my_gender != allowed_gender) {
        usleep(100);
    }

    // Enter bathroom
}

void exit(int id, Gender my_gender) {
    if (count[my_gender] == 0) {
        allowed_gender = opposite(my_gender);
    }
}
```
**Pros**: Perfect fairness, no starvation
**Cons**: Poor performance if one gender has more persons, forced alternation even when no one waiting

**Strategy 2: Priority to Waiting**
```c
// When bathroom empties, check who's waiting:
if (bathroom_empty) {
    if (opposite_waiting > 0 && same_waiting == 0) {
        // Give to opposite gender
        allow_opposite();
    } else if (opposite_waiting > 0 && same_waiting > 0) {
        // Both genders waiting - use policy (e.g., alternate)
        if (last_served == my_gender) {
            allow_opposite();
        } else {
            allow_same();
        }
    }
}
```
**Pros**: Responsive to demand, prevents starvation
**Cons**: Complex logic, requires tracking

**Strategy 3: FIFO Queue**
```c
// Perfect fairness through explicit ordering
// Persons enter in order they arrived
// Gender changes when next person in queue is different gender
```
**Pros**: Perfect fairness, intuitive behavior
**Cons**: Implementation complexity with semaphores, less parallelism

**Strategy 4: Time-Based**
```c
#define MAX_GENDER_TIME 10  // seconds

time_t gender_start_time;

if (difftime(time(NULL), gender_start_time) > MAX_GENDER_TIME) {
    // Force gender change
    force_change_gender();
}
```
**Pros**: Bounded wait time guarantee
**Cons**: Arbitrary time threshold, may force change when no one waiting

### Recommended: Turnstile + Priority

**Implementation**:
```c
Gender last_served_gender = WOMAN;

void enter_fair(int id, Gender my_gender) {
    // Turnstile prevents barging
    sem_wait(&turnstile);
    sem_post(&turnstile);

    sem_wait(my_gender == MAN ? &menQueue : &womenQueue);

    sem_wait(&mutex);

    // Wait for opposite gender to clear
    while (opposite_count > 0) {
        sem_post(&mutex);
        usleep(1000);
        sem_wait(&mutex);
    }

    // Priority check: if opposite gender waiting and we've had our turn
    if (opposite_waiting > 0 && last_served_gender == my_gender && my_count == 0) {
        // Give opposite gender a chance
        sem_post(&mutex);
        sem_post(my_queue);
        usleep(5000);
        // Retry
        return;
    }

    my_count++;
    my_waiting--;

    if (my_count == 1) {
        sem_wait(opposite_queue);
        last_served_gender = my_gender;
    }

    sem_post(&mutex);
    sem_post(my_queue);
}
```

## Performance Considerations

### Throughput Analysis

**Best Case**: Single gender dominates
- All persons of one gender can use bathroom simultaneously
- High throughput, high parallelism

**Worst Case**: Constant gender alternation
- Only one person uses bathroom at a time (FIFO with alternating genders)
- Low throughput, no parallelism

**Realistic Case**: Batches of same gender
- Groups of same gender use bathroom together
- Periodic gender changes
- Moderate throughput

### Contention Points

**1. Mutex Contention**:
- All persons contend for mutex to check/update counts
- Critical section should be short
- O(n) contention with n persons

**2. Queue Semaphore Contention**:
- Persons of same gender contend for gender queue
- Affects entry rate

**3. Turnstile Contention**:
- All persons pass through turnstile
- Very brief hold time (immediate release)
- Minimal impact

### Scalability

**With Increasing Number of Persons**:
- More contention on mutex and queues
- Longer wait times
- More persons in bathroom simultaneously (same gender)
- Higher chance of gender alternation

**Optimization Opportunities**:
1. Minimize critical section duration
2. Batch signal all waiting of same gender
3. Use faster synchronization primitives if available
4. Consider separate bathrooms (not allowed by problem!)

## Recommendations and Improvements

### Core Implementation Requirements

**1. Proper Initialization**:
```c
sem_init(&mutex, 0, 1);
sem_init(&menQueue, 0, 1);
sem_init(&womenQueue, 0, 1);
sem_init(&turnstile, 0, 1);

menCount = 0;
womenCount = 0;
menWaiting = 0;
womenWaiting = 0;
```

**2. Atomic Count Operations**:
```c
// All count reads/writes must be within mutex:
sem_wait(&mutex);
menCount++;  // Atomic with check below
if (menCount == 1) {
    sem_wait(&womenQueue);
}
sem_post(&mutex);
```

**3. Fairness Mechanism**:
```c
// MUST implement one of:
// - Turnstile pattern
// - Priority to waiting gender
// - FIFO queue
// - Ticket system
// Simple readers-writers is NOT sufficient!
```

**4. Error Checking**:
```c
if (sem_wait(&mutex) != 0) {
    perror("sem_wait failed");
    pthread_exit(NULL);
}
```

**5. Assertions for Invariants**:
```c
sem_wait(&mutex);
assert(menCount >= 0);
assert(womenCount >= 0);
assert(!(menCount > 0 && womenCount > 0));  // Never both genders
assert(menWaiting >= 0);
assert(womenWaiting >= 0);
sem_post(&mutex);
```

### Correctness Enhancements

**6. Prevent Count Underflow**:
```c
sem_wait(&mutex);
if (menCount == 0) {
    fprintf(stderr, "ERROR: menCount underflow!\n");
    sem_post(&mutex);
    pthread_exit(NULL);
}
menCount--;
sem_post(&mutex);
```

**7. Proper Cleanup**:
```c
// Signal termination
simulation_running = 0;

// Wait for all threads
for (int i = 0; i < num_men; i++) {
    pthread_join(men_threads[i], NULL);
}
for (int i = 0; i < num_women; i++) {
    pthread_join(women_threads[i], NULL);
}

// Destroy semaphores
sem_destroy(&mutex);
sem_destroy(&menQueue);
sem_destroy(&womenQueue);
sem_destroy(&turnstile);
```

**8. Timeout Mechanism** (for testing):
```c
struct timespec timeout;
clock_gettime(CLOCK_REALTIME, &timeout);
timeout.tv_sec += 5;  // 5 second timeout

if (sem_timedwait(&mutex, &timeout) != 0) {
    if (errno == ETIMEDOUT) {
        fprintf(stderr, "Possible deadlock detected!\n");
    }
}
```

### Fairness Improvements

**9. Track and Display Statistics**:
```c
typedef struct {
    int person_id;
    Gender gender;
    int bathroom_visits;
    double total_wait_time;
    double max_wait_time;
    time_t last_entry_time;
} PersonStats;

PersonStats stats[MAX_PERSONS];

void print_statistics() {
    printf("\n=== Bathroom Usage Statistics ===\n");

    double men_avg_wait = calculate_average_wait(MAN);
    double women_avg_wait = calculate_average_wait(WOMAN);

    printf("Men average wait: %.2f seconds\n", men_avg_wait);
    printf("Women average wait: %.2f seconds\n", women_avg_wait);

    int men_visits = count_total_visits(MAN);
    int women_visits = count_total_visits(WOMAN);

    printf("Total men visits: %d\n", men_visits);
    printf("Total women visits: %d\n", women_visits);

    double fairness_ratio = (double)men_visits / women_visits;
    printf("Visit ratio (M/W): %.2f\n", fairness_ratio);

    if (fairness_ratio > 1.5 || fairness_ratio < 0.67) {
        printf("WARNING: Significant unfairness detected!\n");
    }
}
```

**10. Adaptive Priority**:
```c
// Give higher priority to gender that has waited longest
time_t men_first_waiting = get_first_waiting_time(MAN);
time_t women_first_waiting = get_first_waiting_time(WOMAN);

if (bathroom_empty) {
    if (men_first_waiting < women_first_waiting) {
        // Men have waited longer - give them priority
        signal_all_men();
    } else {
        signal_all_women();
    }
}
```

### Testing Recommendations

**11. Correctness Tests**:
```c
// Test 1: Single man, single woman
// Should alternate without deadlock

// Test 2: Multiple men, single woman
// Woman should not starve

// Test 3: Single man, multiple women
// Man should not starve

// Test 4: Equal numbers
// Should show balanced usage

// Test 5: Unequal numbers (10 men, 2 women)
// Both genders should get access
```

**12. Fairness Tests**:
```bash
# Run with different configurations:
./bathroom 10 10 60  # 10 men, 10 women, 60 seconds
./bathroom 20 5 60   # 20 men, 5 women, 60 seconds
./bathroom 5 20 60   # 5 men, 20 women, 60 seconds

# Analyze output:
# - Count bathroom visits per person
# - Calculate wait time statistics
# - Check for starvation (zero visits)
# - Verify gender alternation occurs
```

**13. Stress Tests**:
```c
// Many persons, long simulation:
./bathroom 50 50 300  # 50 men, 50 women, 5 minutes

// Track:
// - Maximum number of same gender in bathroom simultaneously
// - Gender change frequency
// - Any deadlock or livelock
// - Memory usage
```

**14. Race Condition Detection**:
```bash
# Compile with thread sanitizer:
gcc -fsanitize=thread -g bathroom.c -lpthread -o bathroom

# Run multiple times:
for i in {1..100}; do
    ./bathroom 10 10 10
done

# Should detect any race conditions
```

**15. Deadlock Detection**:
```c
// Add watchdog timer:
void* watchdog_thread(void* arg) {
    while (simulation_running) {
        sleep(5);

        sem_wait(&mutex);
        int total_persons = menCount + womenCount +
                           menWaiting + womenWaiting;

        if (total_persons == 0 && num_active_threads > 0) {
            printf("WARNING: Possible deadlock - no one in/waiting for bathroom!\n");
        }
        sem_post(&mutex);
    }
    return NULL;
}
```

## Example Output Format

```
Simulation started: 5 men, 5 women

Man 1 working...
Woman 1 working...
Man 2 working...
Woman 2 working...
...

Man 1 wants to use bathroom
Man 1 entered bathroom [Men: 1, Women: 0]
Woman 1 wants to use bathroom (waiting - men in bathroom)
Man 2 wants to use bathroom
Man 2 entered bathroom [Men: 2, Women: 0]
Man 1 using bathroom...
Man 2 using bathroom...
Man 1 left bathroom [Men: 1, Women: 0]
Man 2 left bathroom [Men: 0, Women: 0]

Woman 1 entered bathroom [Men: 0, Women: 1]
Woman 2 wants to use bathroom
Woman 2 entered bathroom [Men: 0, Women: 2]
Woman 1 using bathroom...
Woman 2 using bathroom...
Man 1 wants to use bathroom (waiting - women in bathroom)
Woman 1 left bathroom [Men: 0, Women: 1]
Woman 2 left bathroom [Men: 0, Women: 0]

Man 1 entered bathroom [Men: 1, Women: 0]
...

=== Statistics (After 60 seconds) ===
Total bathroom visits: 247
  Men: 124 visits (50.2%)
  Women: 123 visits (49.8%)

Average wait time:
  Men: 0.35 seconds
  Women: 0.38 seconds

Maximum wait time:
  Men: 2.1 seconds (Man 3)
  Women: 1.9 seconds (Woman 4)

Gender changes: 45
Average persons per visit: 2.3

No starvation detected - all persons used bathroom
Fairness ratio: 1.01 (FAIR)
```

## Conclusion

The Unisex Bathroom Problem is the most complex problem in Homework 3 because it REQUIRES fairness:

**Key Concepts Tested**:
1. Readers-Writers problem with fairness constraint
2. Preventing starvation in concurrent systems
3. Implementing fairness with only semaphores
4. Trade-offs between performance and fairness
5. Complex synchronization logic with multiple conditions

**Critical Challenges**:
1. **Fairness is Required**: Cannot use simple readers-writers pattern
2. **Semaphore Limitations**: POSIX semaphores don't guarantee FIFO
3. **Preventing Barging**: New arrivals shouldn't cut ahead of waiting persons
4. **Batching Same Gender**: Allow multiple same-gender entries for efficiency
5. **Testing Fairness**: Must verify no starvation occurs

**Common Mistakes**:
- Using simple readers-writers without fairness mechanism
- Incorrect turnstile usage (holding it too long)
- Forgetting to track waiting counters
- Not giving priority to waiting opposite gender
- Ignoring barging problem
- Missing assertions for gender exclusion
- Inadequate fairness testing

**Success Criteria**:
- Men and women never in bathroom simultaneously (mutual exclusion)
- Multiple same gender can use bathroom together (parallelism)
- No deadlock under any scenario
- **FAIRNESS**: No person waits indefinitely - all get access
- Bounded wait times for all persons
- Clean termination and resource cleanup
- Statistical evidence of fairness (balanced usage, low variance in wait times)

**Why This Problem is Worth 40 Points**:
- Fairness requirement doubles complexity
- Requires sophisticated synchronization strategy
- Testing fairness is non-trivial
- Multiple correct solutions possible, must choose and justify one
- Demonstrates mastery of concurrent programming concepts

**Best Practices**:
1. Start with simple readers-writers, then add fairness
2. Use turnstile pattern for basic fairness
3. Add priority logic for stronger fairness guarantees
4. Implement comprehensive statistics tracking
5. Test with various ratios of men/women
6. Verify fairness with long-running simulations
7. Use thread sanitizer for correctness verification
8. Consider FIFO queue for perfect fairness (if complexity allows)
