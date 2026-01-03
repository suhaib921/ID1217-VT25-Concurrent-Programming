# Question 7: The One-Lane Bridge Problem - Analysis

## Summary of the Question/Task

A classic readers-writers variant modeling traffic flow:
- **One-lane bridge** shared by cars from north and south
- Cars heading in **same direction** can cross simultaneously
- Cars heading in **opposite directions** CANNOT cross simultaneously
- Each car crosses the bridge **trips** times, alternating directions
- `northCars` cars start going northbound
- `southCars` cars start going southbound

**Requirements**:
- Use a monitor to control bridge access
- Monitor operations: Car calls to request permission and indicate completion
- Ensure **mutual exclusion** (no opposite-direction conflicts)
- Avoid **deadlock**
- Ensure **fairness** (any waiting car eventually crosses)
- Print trace: timestamps, car IDs, events (wanting to cross, starting, leaving)
- Command-line args: `trips`, `northCars`, `southCars`

## Implementation Approach Required

### Problem Analysis

**Key Insights**:
1. This is a **readers-writers variant** where:
   - "Readers" = cars going same direction (multiple allowed)
   - "Writers" = cars going opposite direction (exclusive)

2. **Direction switching** is critical:
   - When last car in one direction exits, allow opposite direction
   - Must prevent starvation of one direction

3. **Fairness challenge**:
   - Without fairness, one direction could monopolize bridge
   - Example: Continuous stream of north-bound cars blocks south-bound forever

### Monitor Design

**State Variables**:
- `carsOnBridge`: Number of cars currently on bridge
- `currentDirection`: Current traffic direction (NORTH, SOUTH, or NONE)
- `northWaiting`: Number of north-bound cars waiting
- `southWaiting`: Number of south-bound cars waiting

**Condition Variables**:

Option 1 - Two condition variables (direction-specific):
- `northQueue`: North-bound cars wait here
- `southQueue`: South-bound cars wait here

Option 2 - Single condition variable:
- `bridgeAvailable`: All cars wait here

**Monitor Methods**:

Design Choice: 2 methods or 4 methods?

**2-Method Approach**:
```java
enterBridge(Direction direction)
exitBridge(Direction direction)
```

**4-Method Approach**:
```java
enterBridgeNorth()
exitBridgeNorth()
enterBridgeSouth()
exitBridgeSouth()
```

**Recommended**: 2-method approach (more flexible, less code duplication)

### Basic Algorithm (Unfair)

```java
synchronized void enterBridge(Direction dir) {
    while (carsOnBridge > 0 && currentDirection != dir) {
        wait(); // Opposite direction has bridge
    }
    if (carsOnBridge == 0) {
        currentDirection = dir; // Set direction
    }
    carsOnBridge++;
}

synchronized void exitBridge(Direction dir) {
    carsOnBridge--;
    if (carsOnBridge == 0) {
        currentDirection = Direction.NONE;
        notifyAll(); // Allow opposite direction
    }
}
```

**Problem with Basic Algorithm**: **Starvation possible**
- If north-bound cars keep arriving, south-bound cars never get access
- Not fair!

### Fair Algorithm

To ensure fairness, use **turn-based scheduling** or **priority to waiting direction**:

**Approach 1: Alternate Directions**
```java
private Direction nextDirection = Direction.NORTH;

synchronized void enterBridge(Direction dir) {
    // If wrong turn and opposite direction waiting, wait
    while ((carsOnBridge > 0 && currentDirection != dir) ||
           (carsOnBridge == 0 && nextDirection != dir && oppositeWaiting(dir))) {
        incrementWaiting(dir);
        wait();
        decrementWaiting(dir);
    }

    if (carsOnBridge == 0) {
        currentDirection = dir;
    }
    carsOnBridge++;
}

synchronized void exitBridge(Direction dir) {
    carsOnBridge--;
    if (carsOnBridge == 0) {
        currentDirection = Direction.NONE;
        nextDirection = opposite(dir); // Alternate
        notifyAll();
    }
}
```

**Approach 2: Priority to Waiting Cars**
- If opposite direction has waiting cars when bridge clears, give them priority
- Current direction can't re-enter until opposite direction's waiting queue clears

**Approach 3: Time-Based Fairness**
- Track wait time for each direction
- Prioritize direction that has waited longest

## Correctness Assessment

### Safety Properties

1. **Mutual Exclusion (Opposite Directions)**:
   - No north-bound and south-bound cars on bridge simultaneously
   - Ensured by: `currentDirection` check before entering
   - Violation would be catastrophic (collision!)

2. **Concurrent Same-Direction Access**:
   - Multiple cars in same direction allowed
   - Tracked by: `carsOnBridge` counter

3. **Bounded Cars on Bridge**:
   - Implicitly bounded by number of cars in simulation
   - Could add explicit limit if desired

### Liveness Properties

1. **No Deadlock**:
   - Cars wait on condition variables (release lock)
   - When bridge clears, waiting cars are notified
   - No circular wait

2. **Eventual Progress (Fairness)**:
   - **Critical requirement**: Any waiting car eventually crosses
   - **Basic algorithm FAILS** this requirement
   - **Fair algorithm** required

### Potential Correctness Issues

**Issue 1: Starvation (Unfair Algorithm)**

Scenario:
```
Bridge clear, direction NONE
North car N1 enters (direction = NORTH)
South cars S1, S2, S3 arrive and wait
N1 exits, clears bridge
But immediately N2, N3, N4 enter (same direction)
South cars keep waiting indefinitely
STARVATION!
```

**Solution**: Fair scheduling (see algorithms above)

**Issue 2: Direction Not Reset**

```java
// WRONG
synchronized void exitBridge(Direction dir) {
    carsOnBridge--;
    // Forgot to reset direction when bridge empty!
}

// Bridge stuck in one direction forever
```

**Solution**: Reset `currentDirection` when `carsOnBridge == 0`

**Issue 3: Spurious Wakeups**

```java
// WRONG - if statement
synchronized void enterBridge(Direction dir) {
    if (carsOnBridge > 0 && currentDirection != dir) {
        wait();
    }
    carsOnBridge++; // Might violate after spurious wakeup!
}

// CORRECT - while loop
synchronized void enterBridge(Direction dir) {
    while (carsOnBridge > 0 && currentDirection != dir) {
        wait();
    }
    carsOnBridge++;
}
```

**Issue 4: Race in Direction Setting**

```java
// WRONG - race condition
synchronized void enterBridge(Direction dir) {
    while (carsOnBridge > 0 && currentDirection != dir) {
        wait();
    }
    // Multiple cars wake, race to set direction
    currentDirection = dir; // Should only set if bridge empty!
    carsOnBridge++;
}

// CORRECT
synchronized void enterBridge(Direction dir) {
    while (carsOnBridge > 0 && currentDirection != dir) {
        wait();
    }
    if (carsOnBridge == 0) {
        currentDirection = dir; // Only first car sets direction
    }
    carsOnBridge++;
}
```

## Performance Considerations

### Concurrency Analysis

**Maximum Concurrency**: Unbounded (all cars in one direction can cross simultaneously)
- Example: All 10 north-bound cars on bridge at once
- Limited only by physical bridge capacity (not modeled in problem)

**Bottleneck**: Direction switches
- When last car in one direction exits, must switch directions
- All waiting opposite-direction cars wake and compete for lock
- Thundering herd on direction change

### Efficiency Optimizations

1. **Batching Same Direction**:
   - Allow all waiting same-direction cars to enter before switching
   - Reduces direction switches
   - But can hurt fairness

2. **Separate Condition Variables**:
   ```java
   synchronized void enterBridge(Direction dir) {
       Condition myQueue = (dir == NORTH) ? northQueue : southQueue;
       while (!canEnter(dir)) {
           myQueue.await();
       }
       // ...
   }

   synchronized void exitBridge(Direction dir) {
       carsOnBridge--;
       if (carsOnBridge == 0) {
           // Signal opposite direction
           Condition oppositeQueue = (dir == NORTH) ? southQueue : northQueue;
           oppositeQueue.signalAll();
       }
   }
   ```
   - More targeted signaling
   - Reduces unnecessary wake-ups

3. **Limit Cars Per Batch**:
   - To improve fairness, limit how many cars in one direction before forced switch
   - Example: Max 5 north-bound, then must switch to south

### Fairness vs. Throughput Trade-off

**Maximize Throughput**:
- Keep traffic flowing in one direction as long as cars available
- Minimize direction switches (expensive)
- **Risk**: Starvation of opposite direction

**Maximize Fairness**:
- Strictly alternate directions
- Each direction gets equal opportunity
- **Risk**: Lower throughput, more context switches

**Balanced Approach**:
- Allow batch of cars in one direction
- Switch when opposite direction has waiting cars and current batch done
- Prevents starvation while maintaining reasonable throughput

## Synchronization and Concurrency Issues

### Critical Synchronization Points

1. **Bridge Empty → First Car Enters**:
   - Set `currentDirection` to car's direction
   - Allow subsequent same-direction cars

2. **Last Car Exits**:
   - Reset `currentDirection` to NONE
   - Signal waiting opposite-direction cars
   - **Fairness decision**: Who gets access next?

3. **Direction Switch**:
   - Transition from all cars in one direction to all cars in opposite
   - Must be atomic and fair

### Fairness Implementation Details

**Tracking Waiting Cars**:
```java
private int northWaiting = 0;
private int southWaiting = 0;

synchronized void enterBridge(Direction dir) {
    if (dir == NORTH) northWaiting++;
    else southWaiting++;

    while (!canEnter(dir)) {
        wait();
    }

    if (dir == NORTH) northWaiting--;
    else southWaiting--;

    // Proceed
}

private boolean canEnter(Direction dir) {
    // Complex fairness logic
    if (carsOnBridge > 0) {
        return currentDirection == dir;
    }
    // Bridge empty - fairness decision
    if (dir == NORTH) {
        // If south has been waiting, give them priority
        return southWaiting == 0 || lastDirection == NORTH;
    } else {
        return northWaiting == 0 || lastDirection == SOUTH;
    }
}
```

**Alternating Priority**:
```java
private Direction lastDirection = Direction.NONE;

synchronized void exitBridge(Direction dir) {
    carsOnBridge--;
    if (carsOnBridge == 0) {
        currentDirection = Direction.NONE;
        lastDirection = dir; // Remember who just finished
        notifyAll();
    }
}
```

## Recommendations and Improvements

### Complete Implementation

**Enum and Monitor**:
```java
enum Direction { NORTH, SOUTH, NONE }

class Bridge {
    private int carsOnBridge = 0;
    private Direction currentDirection = Direction.NONE;
    private int northWaiting = 0;
    private int southWaiting = 0;
    private Direction lastDirection = Direction.NONE;

    public synchronized void enterBridge(Direction dir)
            throws InterruptedException {

        // Increment waiting counter
        if (dir == Direction.NORTH) northWaiting++;
        else southWaiting++;

        // Wait until can enter (fairness-aware)
        while (!canEnter(dir)) {
            wait();
        }

        // Decrement waiting counter
        if (dir == Direction.NORTH) northWaiting--;
        else southWaiting--;

        // Enter bridge
        if (carsOnBridge == 0) {
            currentDirection = dir;
        }
        carsOnBridge++;

        System.out.println(timestamp() + " Car entering bridge " + dir +
                          " (cars on bridge: " + carsOnBridge + ")");
    }

    public synchronized void exitBridge(Direction dir) {
        carsOnBridge--;
        System.out.println(timestamp() + " Car exiting bridge " + dir +
                          " (cars on bridge: " + carsOnBridge + ")");

        if (carsOnBridge == 0) {
            currentDirection = Direction.NONE;
            lastDirection = dir;
            notifyAll(); // Allow opposite direction
        }
    }

    private boolean canEnter(Direction dir) {
        // If bridge occupied, must be same direction
        if (carsOnBridge > 0) {
            return currentDirection == dir;
        }

        // Bridge empty - apply fairness policy
        // Give priority to opposite direction if they're waiting
        Direction opposite = (dir == Direction.NORTH) ?
                             Direction.SOUTH : Direction.NORTH;

        if (opposite == Direction.NORTH) {
            // I'm south, check if north waiting and had priority
            return northWaiting == 0 || lastDirection == Direction.SOUTH;
        } else {
            // I'm north, check if south waiting and had priority
            return southWaiting == 0 || lastDirection == Direction.NORTH;
        }
    }

    private String timestamp() {
        return String.format("%d", System.currentTimeMillis());
    }
}
```

**Car Thread**:
```java
class Car extends Thread {
    private final int id;
    private final Bridge bridge;
    private final Direction initialDirection;
    private final int trips;
    private final Random random = new Random();

    public Car(int id, Direction initialDir, Bridge bridge, int trips) {
        this.id = id;
        this.initialDirection = initialDir;
        this.bridge = bridge;
        this.trips = trips;
    }

    public void run() {
        try {
            Direction currentDir = initialDirection;

            for (int i = 0; i < trips; i++) {
                // Travel to bridge
                Thread.sleep(random.nextInt(1000));

                // Request to cross
                System.out.println(timestamp() + " Car-" + id + "-" + currentDir +
                                 " wants to cross bridge");
                bridge.enterBridge(currentDir);

                // Cross bridge
                System.out.println(timestamp() + " Car-" + id + "-" + currentDir +
                                 " is crossing bridge");
                Thread.sleep(random.nextInt(200));

                // Exit bridge
                bridge.exitBridge(currentDir);
                System.out.println(timestamp() + " Car-" + id + "-" + currentDir +
                                 " left bridge");

                // Switch direction for next trip
                currentDir = (currentDir == Direction.NORTH) ?
                            Direction.SOUTH : Direction.NORTH;
            }

            System.out.println("Car-" + id + " completed all " + trips + " trips");

        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    private String timestamp() {
        return String.format("%d", System.currentTimeMillis());
    }
}
```

**Main Program**:
```java
public class OneLaneBridge {
    public static void main(String[] args) {
        if (args.length < 3) {
            System.out.println("USAGE: java OneLaneBridge <trips> <northCars> <southCars>");
            return;
        }

        int trips = Integer.parseInt(args[0]);
        int northCars = Integer.parseInt(args[1]);
        int southCars = Integer.parseInt(args[2]);

        Bridge bridge = new Bridge();
        List<Car> cars = new ArrayList<>();

        // Create north-bound cars
        for (int i = 0; i < northCars; i++) {
            cars.add(new Car(i, Direction.NORTH, bridge, trips));
        }

        // Create south-bound cars
        for (int i = 0; i < southCars; i++) {
            cars.add(new Car(northCars + i, Direction.SOUTH, bridge, trips));
        }

        // Start all cars
        for (Car car : cars) {
            car.start();
        }

        // Wait for all to finish
        for (Car car : cars) {
            try {
                car.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        System.out.println("All cars finished crossing.");
    }
}
```

### Testing Strategy

1. **Correctness Tests**:
   - Log all enter/exit events with direction
   - Verify no simultaneous opposite directions
   - Check `carsOnBridge` never negative

2. **Fairness Tests**:
   - Equal north/south cars, measure total wait time per direction
   - Skewed distribution (many north, few south), verify south not starved
   - Track max wait time for any car

3. **Stress Tests**:
   - Many cars, many trips
   - Equal north/south split
   - Highly skewed distribution

4. **Edge Cases**:
   - trips = 1 (each car crosses once)
   - northCars = 0 or southCars = 0 (one direction only)
   - Both = 1 (minimal concurrency)

### Expected Output

```
1234567890 Car-0-NORTH wants to cross bridge
1234567891 Car-0-NORTH entering bridge NORTH (cars on bridge: 1)
1234567892 Car-1-NORTH wants to cross bridge
1234567893 Car-1-NORTH entering bridge NORTH (cars on bridge: 2)
1234567894 Car-2-SOUTH wants to cross bridge
1234567894 Car-2-SOUTH waiting...
1234567895 Car-0-NORTH is crossing bridge
1234567896 Car-1-NORTH is crossing bridge
1234567900 Car-0-NORTH exiting bridge NORTH (cars on bridge: 1)
1234567905 Car-1-NORTH exiting bridge NORTH (cars on bridge: 0)
1234567906 Car-2-SOUTH entering bridge SOUTH (cars on bridge: 1)
...
```

## C++ Implementation Sketch

```cpp
enum class Direction { NORTH, SOUTH, NONE };

class Bridge {
    int carsOnBridge = 0;
    Direction currentDirection = Direction::NONE;
    int northWaiting = 0;
    int southWaiting = 0;
    Direction lastDirection = Direction::NONE;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void enterBridge(Direction dir) {
        std::unique_lock<std::mutex> lock(mtx);

        if (dir == Direction::NORTH) northWaiting++;
        else southWaiting++;

        cv.wait(lock, [&]() { return canEnter(dir); });

        if (dir == Direction::NORTH) northWaiting--;
        else southWaiting--;

        if (carsOnBridge == 0) {
            currentDirection = dir;
        }
        carsOnBridge++;
    }

    void exitBridge(Direction dir) {
        std::unique_lock<std::mutex> lock(mtx);
        carsOnBridge--;

        if (carsOnBridge == 0) {
            currentDirection = Direction::NONE;
            lastDirection = dir;
            cv.notify_all();
        }
    }

private:
    bool canEnter(Direction dir) {
        if (carsOnBridge > 0) {
            return currentDirection == dir;
        }

        Direction opposite = (dir == Direction::NORTH) ?
                            Direction::SOUTH : Direction::NORTH;

        if (opposite == Direction::NORTH) {
            return northWaiting == 0 || lastDirection == Direction::SOUTH;
        } else {
            return southWaiting == 0 || lastDirection == Direction::NORTH;
        }
    }
};
```

## Conclusion

The One-Lane Bridge problem is a **readers-writers variant** with direction-based exclusion:
- **Same direction** = multiple readers (concurrent access)
- **Opposite direction** = exclusive writer (mutual exclusion)

**Key Challenges**:
1. **Mutual exclusion** for opposite directions (safety)
2. **Fairness** to prevent starvation (liveness)
3. **Efficient direction switching** (performance)
4. **Handling alternating car directions** (each car switches)

**Critical Design Decisions**:
- **Fairness mechanism**: Alternating priority, waiting counters, time-based
- **Signaling strategy**: Broadcast vs. targeted (direction-specific)
- **Batching policy**: How many cars before forced switch

**Correctness Requirements**:
- No opposite-direction conflicts (safety)
- No deadlock (use condition variables properly)
- Eventual progress for all cars (fairness)
- Proper state transitions (direction resets when bridge empty)

A correct, fair implementation requires:
- Tracking waiting cars per direction
- Priority to opposite direction after current direction finishes
- While-loop guards for spurious wakeups
- Comprehensive logging for verification
- Testing with various car distributions and trip counts

This problem elegantly demonstrates how fairness constraints significantly complicate what seems like a simple synchronization problem.
