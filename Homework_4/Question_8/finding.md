# Question 8: The Unisex Bathroom Problem - Analysis

## Summary of the Question/Task

A classic synchronization problem modeling shared resource access with gender-based exclusion:
- **One bathroom** in a building
- **Any number** of men OR women can use it, but **not simultaneously**
- Men and women must be mutually exclusive (no mixing)

**Monitor Interface**:
- `manEnter()`: Man requests permission to enter
- `manExit()`: Man indicates finished
- `womanEnter()`: Woman requests permission to enter
- `womanExit()`: Woman indicates finished

**Requirements**:
- Use **Signal and Continue** signaling discipline (default)
- Ensure **mutual exclusion** (no mixed genders)
- Avoid **deadlock**
- Ensure **fairness** (anyone waiting eventually gets in)
- Implement in Java or C++
- Print trace of events with timestamps and person IDs

## Implementation Approach Required

### Problem Analysis

**Key Insight**: This is **identical** to the One-Lane Bridge problem!
- Men = cars going north
- Women = cars going south
- Bathroom = bridge
- Same-gender = same-direction (multiple allowed)
- Opposite-gender = opposite-direction (mutually exclusive)

**Differences**:
- Bridge: Cars switch directions after crossing
- Bathroom: People keep same gender (no switching)
- Otherwise, synchronization logic is the same

### Monitor Design

**State Variables**:
- `menInside`: Number of men currently in bathroom
- `womenInside`: Number of women currently in bathroom
- `menWaiting`: Number of men waiting to enter
- `womenWaiting`: Number of women waiting to enter

**Invariant**: `menInside > 0` XOR `womenInside > 0` XOR `both == 0`
(Exactly one gender or empty, never both)

**Condition Variables**:

Option 1 - Separate per gender:
- `menQueue`: Men wait here
- `womenQueue`: Women wait here

Option 2 - Single condition variable:
- `canEnter`: Everyone waits here

**Recommended**: Separate condition variables for efficient signaling

### Basic Algorithm (Unfair)

```java
synchronized void manEnter() {
    while (womenInside > 0) {
        wait(); // Women currently inside
    }
    menInside++;
}

synchronized void manExit() {
    menInside--;
    if (menInside == 0) {
        notifyAll(); // Allow women
    }
}

synchronized void womanEnter() {
    while (menInside > 0) {
        wait(); // Men currently inside
    }
    womenInside++;
}

synchronized void womanExit() {
    womenInside--;
    if (womenInside == 0) {
        notifyAll(); // Allow men
    }
}
```

**Problem**: **Starvation possible**
- Continuous stream of men entering blocks women indefinitely
- Not fair!

### Fair Algorithm

To ensure fairness, prioritize waiting opposite gender when bathroom becomes empty:

```java
private int menInside = 0;
private int womenInside = 0;
private int menWaiting = 0;
private int womenWaiting = 0;

synchronized void manEnter() throws InterruptedException {
    menWaiting++;

    // Wait if women inside OR if women waiting and they have priority
    while (womenInside > 0 ||
           (menInside == 0 && womenWaiting > 0 && lastGender == MAN)) {
        wait();
    }

    menWaiting--;
    menInside++;
}

synchronized void manExit() {
    menInside--;
    if (menInside == 0) {
        lastGender = MAN;
        notifyAll(); // Prioritize women
    }
}

// Similar for womanEnter() and womanExit()
```

**Fairness Mechanism**: Track `lastGender`, give priority to opposite gender when bathroom clears

## Correctness Assessment

### Safety Properties

1. **Mutual Exclusion (Gender-Based)**:
   - Never have `menInside > 0 && womenInside > 0`
   - Ensured by: Wait while opposite gender inside
   - Verification: `assert(!(menInside > 0 && womenInside > 0))`

2. **Bounded Occupancy**:
   - No explicit limit, but bounded by total people in simulation
   - Could add max capacity if desired

3. **Consistent State**:
   - `menInside >= 0` and `womenInside >= 0` always
   - Increments and decrements properly paired

### Liveness Properties

1. **No Deadlock**:
   - Use condition variables (release lock while waiting)
   - No circular wait
   - When bathroom empties, waiting people notified

2. **Eventual Progress (Fairness)**:
   - **Critical requirement**: Anyone waiting eventually enters
   - **Basic algorithm FAILS** fairness
   - **Fair algorithm REQUIRED**

### Potential Correctness Issues

**Issue 1: Gender Mixing**

```java
// WRONG - race condition
synchronized void manEnter() {
    if (womenInside == 0) { // Check
        // Another woman could enter here!
        menInside++;        // Increment
    }
}

// CORRECT - while loop in monitor
synchronized void manEnter() {
    while (womenInside > 0) {
        wait();
    }
    menInside++; // Atomic within monitor
}
```

**Issue 2: Starvation (Unfair Algorithm)**

Scenario:
```
Bathroom empty
Man M1 enters (menInside = 1)
Women W1, W2, W3 arrive and wait
M1 exits (menInside = 0)
But men M2, M3, M4 immediately enter
Women keep waiting → STARVATION
```

**Solution**: Fair scheduling (see fair algorithm above)

**Issue 3: Notification After Last Exit**

```java
// WRONG - forgot to notify
synchronized void manExit() {
    menInside--;
    // Forgot to notify waiting women!
}

// Women wait forever → DEADLOCK
```

**Issue 4: Spurious Wakeups**

```java
// WRONG - if statement
synchronized void manEnter() {
    if (womenInside > 0) {
        wait();
    }
    menInside++; // Could violate after spurious wakeup
}

// CORRECT - while loop
synchronized void manEnter() {
    while (womenInside > 0) {
        wait();
    }
    menInside++;
}
```

## Performance Considerations

### Concurrency Analysis

**Maximum Concurrency**: Unbounded (all people of one gender can enter)
- Example: All 50 men in bathroom simultaneously
- All 50 women must wait

**Bottleneck**: Gender switches
- When last person of one gender exits, switch occurs
- All waiting opposite-gender people wake up and compete

### Efficiency Optimizations

1. **Separate Condition Variables**:
   ```java
   Condition menQueue = lock.newCondition();
   Condition womenQueue = lock.newCondition();

   synchronized void manExit() {
       menInside--;
       if (menInside == 0) {
           womenQueue.signalAll(); // Only wake women
       }
   }
   ```
   - Reduces unnecessary wake-ups

2. **Batching Same Gender**:
   - Allow all waiting same-gender people to enter before switch
   - Reduces context switches
   - Balance with fairness

3. **Max Batch Size**:
   - Limit consecutive same-gender entries
   - Example: Max 10 men, then forced switch to women
   - Improves fairness at cost of throughput

### Fairness vs. Throughput

**Maximize Throughput**:
- Keep bathroom occupied by one gender as long as possible
- Minimize gender switches
- **Risk**: Starvation of minority gender

**Maximize Fairness**:
- Strictly alternate genders
- Each gender gets equal opportunity
- **Risk**: Lower utilization, more switches

**Balanced Approach**:
- Allow batch of one gender
- Switch when opposite gender waiting and current batch done
- Prevents starvation, maintains reasonable throughput

## Synchronization and Concurrency Issues

### Critical Synchronization Points

1. **Empty Bathroom → First Person Enters**:
   - Set bathroom to that gender
   - Allow subsequent same-gender people

2. **Last Person Exits**:
   - Bathroom becomes empty
   - Signal waiting opposite-gender people
   - **Fairness decision**: Who gets access next?

3. **Gender Switch**:
   - Transition from all men to all women (or vice versa)
   - Must be atomic and fair

### Fairness Implementation Strategies

**Strategy 1: Alternating Priority**

```java
private Gender lastGender = Gender.NONE;

synchronized void manEnter() throws InterruptedException {
    menWaiting++;

    while (womenInside > 0 ||
           (menInside == 0 && womenWaiting > 0 && lastGender == Gender.MAN)) {
        wait();
    }

    menWaiting--;
    menInside++;
}

synchronized void manExit() {
    menInside--;
    if (menInside == 0) {
        lastGender = Gender.MAN;
        notifyAll();
    }
}
```

**Strategy 2: Time-Based Priority**
- Track how long each gender has been waiting
- Prioritize gender with longest wait time

**Strategy 3: Ticket System**
- Assign tickets to arriving people
- Serve in ticket order (like Question 3 extra credit)
- Complex but provably fair

## Recommendations and Improvements

### Complete Implementation

**Monitor Class**:
```java
class UnisexBathroom {
    private int menInside = 0;
    private int womenInside = 0;
    private int menWaiting = 0;
    private int womenWaiting = 0;

    enum Gender { MAN, WOMAN, NONE }
    private Gender lastGender = Gender.NONE;

    public synchronized void manEnter() throws InterruptedException {
        menWaiting++;
        System.out.println(timestamp() + " Man-" + Thread.currentThread().getId() +
                          " waiting to enter (menWaiting: " + menWaiting + ")");

        // Wait if women inside, or if women waiting and they have priority
        while (womenInside > 0 ||
               (menInside == 0 && womenWaiting > 0 && lastGender == Gender.MAN)) {
            wait();
        }

        menWaiting--;
        menInside++;
        System.out.println(timestamp() + " Man-" + Thread.currentThread().getId() +
                          " entered bathroom (menInside: " + menInside + ")");
    }

    public synchronized void manExit() {
        menInside--;
        System.out.println(timestamp() + " Man-" + Thread.currentThread().getId() +
                          " exited bathroom (menInside: " + menInside + ")");

        if (menInside == 0) {
            lastGender = Gender.MAN;
            notifyAll(); // Prioritize women
        }
    }

    public synchronized void womanEnter() throws InterruptedException {
        womenWaiting++;
        System.out.println(timestamp() + " Woman-" + Thread.currentThread().getId() +
                          " waiting to enter (womenWaiting: " + womenWaiting + ")");

        // Wait if men inside, or if men waiting and they have priority
        while (menInside > 0 ||
               (womenInside == 0 && menWaiting > 0 && lastGender == Gender.WOMAN)) {
            wait();
        }

        womenWaiting--;
        womenInside++;
        System.out.println(timestamp() + " Woman-" + Thread.currentThread().getId() +
                          " entered bathroom (womenInside: " + womenInside + ")");
    }

    public synchronized void womanExit() {
        womenInside--;
        System.out.println(timestamp() + " Woman-" + Thread.currentThread().getId() +
                          " exited bathroom (womenInside: " + womenInside + ")");

        if (womenInside == 0) {
            lastGender = Gender.WOMAN;
            notifyAll(); // Prioritize men
        }
    }

    private String timestamp() {
        return String.format("%d", System.currentTimeMillis());
    }

    // For verification
    public synchronized void assertInvariants() {
        assert(menInside >= 0 && womenInside >= 0);
        assert(!(menInside > 0 && womenInside > 0)); // Never both!
    }
}
```

**Person Threads**:
```java
class Man extends Thread {
    private final int id;
    private final UnisexBathroom bathroom;
    private final Random random = new Random();

    public Man(int id, UnisexBathroom bathroom) {
        this.id = id;
        this.bathroom = bathroom;
    }

    public void run() {
        try {
            // Simulate arriving at bathroom multiple times
            for (int i = 0; i < 5; i++) {
                // Do other things
                Thread.sleep(random.nextInt(2000));

                // Need bathroom
                bathroom.manEnter();

                // Use bathroom
                Thread.sleep(random.nextInt(500));

                // Leave
                bathroom.manExit();
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}

class Woman extends Thread {
    private final int id;
    private final UnisexBathroom bathroom;
    private final Random random = new Random();

    public Woman(int id, UnisexBathroom bathroom) {
        this.id = id;
        this.bathroom = bathroom;
    }

    public void run() {
        try {
            for (int i = 0; i < 5; i++) {
                Thread.sleep(random.nextInt(2000));
                bathroom.womanEnter();
                Thread.sleep(random.nextInt(500));
                bathroom.womanExit();
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
```

**Main Program**:
```java
public class UnisexBathroomSimulation {
    public static void main(String[] args) {
        final int NUM_MEN = 10;
        final int NUM_WOMEN = 10;

        UnisexBathroom bathroom = new UnisexBathroom();

        // Create and start men threads
        List<Man> men = new ArrayList<>();
        for (int i = 0; i < NUM_MEN; i++) {
            men.add(new Man(i, bathroom));
            men.get(i).start();
        }

        // Create and start women threads
        List<Woman> women = new ArrayList<>();
        for (int i = 0; i < NUM_WOMEN; i++) {
            women.add(new Woman(i, bathroom));
            women.get(i).start();
        }

        // Wait for all to finish
        for (Man man : men) {
            try {
                man.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        for (Woman woman : women) {
            try {
                woman.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        System.out.println("Simulation completed.");
    }
}
```

### Testing Strategy

1. **Correctness Tests**:
   - Verify `menInside > 0 && womenInside > 0` NEVER occurs
   - Log all enter/exit events
   - Check counters never negative

2. **Fairness Tests**:
   - Equal men/women, measure total wait time per gender
   - Skewed distribution (many men, few women), verify women not starved
   - Track max wait time for any person

3. **Stress Tests**:
   - Many people of each gender
   - High contention (short bathroom usage time)
   - Varying arrival patterns

4. **Edge Cases**:
   - All men (no women)
   - All women (no men)
   - One person of each gender
   - Alternating single arrivals

### Expected Output

```
1234567890 Man-10 waiting to enter (menWaiting: 1)
1234567891 Man-10 entered bathroom (menInside: 1)
1234567892 Man-11 waiting to enter (menWaiting: 1)
1234567893 Man-11 entered bathroom (menInside: 2)
1234567894 Woman-20 waiting to enter (womenWaiting: 1)
1234567895 Man-10 exited bathroom (menInside: 1)
1234567896 Man-11 exited bathroom (menInside: 0)
1234567897 Woman-20 entered bathroom (womenInside: 1)
1234567900 Woman-21 waiting to enter (womenWaiting: 1)
1234567901 Woman-21 entered bathroom (womenInside: 2)
...
```

## C++ Implementation Sketch

```cpp
class UnisexBathroom {
    int menInside = 0;
    int womenInside = 0;
    int menWaiting = 0;
    int womenWaiting = 0;

    enum class Gender { MAN, WOMAN, NONE };
    Gender lastGender = Gender::NONE;

    std::mutex mtx;
    std::condition_variable cv;

public:
    void manEnter() {
        std::unique_lock<std::mutex> lock(mtx);
        menWaiting++;

        cv.wait(lock, [&]() {
            return womenInside == 0 &&
                   !(menInside == 0 && womenWaiting > 0 && lastGender == Gender::MAN);
        });

        menWaiting--;
        menInside++;
        std::cout << timestamp() << " Man entered (menInside: "
                  << menInside << ")\\n";
    }

    void manExit() {
        std::unique_lock<std::mutex> lock(mtx);
        menInside--;
        std::cout << timestamp() << " Man exited (menInside: "
                  << menInside << ")\\n";

        if (menInside == 0) {
            lastGender = Gender::MAN;
            cv.notify_all();
        }
    }

    void womanEnter() {
        std::unique_lock<std::mutex> lock(mtx);
        womenWaiting++;

        cv.wait(lock, [&]() {
            return menInside == 0 &&
                   !(womenInside == 0 && menWaiting > 0 && lastGender == Gender::WOMAN);
        });

        womenWaiting--;
        womenInside++;
        std::cout << timestamp() << " Woman entered (womenInside: "
                  << womenInside << ")\\n";
    }

    void womanExit() {
        std::unique_lock<std::mutex> lock(mtx);
        womenInside--;
        std::cout << timestamp() << " Woman exited (womenInside: "
                  << womenInside << ")\\n";

        if (womenInside == 0) {
            lastGender = Gender::WOMAN;
            cv.notify_all();
        }
    }

private:
    std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
        return std::to_string(ms);
    }
};
```

## Advanced Considerations

### Variations

1. **Bathroom Capacity Limit**:
   - Max K people at once (regardless of gender)
   - Adds another constraint to wait condition

2. **Priority Levels**:
   - Emergency vs. normal usage
   - Priority overrides fairness

3. **Non-Binary Genders**:
   - Extend to multiple gender categories
   - More complex exclusion rules

4. **Time-Limited Usage**:
   - Maximum occupancy time
   - Forced exit after timeout

### Relationship to Classic Problems

This is a variant of:
- **Readers-Writers**: Same-gender = readers, opposite-gender = writers
- **One-Lane Bridge**: Direct analogy (as discussed)
- **Barbershop**: Similar capacity and service constraints

### Real-World Applications

Similar synchronization patterns in:
- **Shared resource with category-based exclusion**: File locks by operation type
- **Network protocols**: Shared channel with different message types
- **Database transactions**: Read/write locks with fairness
- **Operating systems**: Process scheduling with priority

## Conclusion

The Unisex Bathroom problem is a **category-based mutual exclusion** problem:
- **Same category** (gender) = multiple concurrent access
- **Different categories** = mutual exclusion
- **Critical requirement**: Fairness to prevent starvation

**Key Challenges**:
1. **Mutual exclusion** between genders (safety)
2. **Fairness** to prevent gender starvation (liveness)
3. **Efficient signaling** on gender switches (performance)
4. **Proper state management** (counters, last gender)

**Solution Components**:
- Monitor with separate enter/exit methods per gender
- Wait while opposite gender inside
- Track waiting counts for fairness
- Prioritize opposite gender when bathroom clears
- Use while loops for spurious wakeup handling

**Fairness Mechanisms**:
- Alternating priority based on last gender
- Waiting counters to detect starvation risk
- Broadcast signaling when bathroom empties
- Balance fairness with throughput

A correct implementation requires:
- Proper condition variable usage with while loops
- Careful fairness policy implementation
- Comprehensive logging for verification
- Testing with various gender distributions
- Explicit invariant checking (never mixed genders)

This problem elegantly demonstrates how seemingly simple requirements (mutual exclusion based on attributes) become complex when fairness is added as a requirement.
