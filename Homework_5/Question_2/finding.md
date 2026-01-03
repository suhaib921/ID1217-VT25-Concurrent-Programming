# Question 2: Distributed Pairing 2 (Peer-to-Peer) - Analysis

## Summary of the Task
Implement a distributed peer-to-peer application where:
- **No centralized server**: Students interact directly with each other to form pairs
- **Uniform algorithm**: All students execute the same algorithm
- **Random/Non-deterministic**: Uses random numbers for partner selection
- **Teacher initiator**: Picks one random student to start, sends "your turn" message
- **Message efficiency**: Ideal solution uses only n messages
- **Output**: Each student prints their index and partner's index

## Implementation Status
**NOT IMPLEMENTED** - Only the problem description exists. No code files are present in this directory.

## Required Implementation Approach

### Architecture
This problem requires a **peer-to-peer distributed architecture** using MPI:
- 1 teacher process (rank 0) - initiator only, not paired
- n student processes (ranks 1 to n) - active participants
- Total MPI processes: n+1

### Algorithm Design

#### Teacher's Role (Minimal):
```
1. Pick random student s in [1, n]
2. Send "YOUR_TURN" message to student s
3. Wait until all students are paired (optional monitoring)
```

#### Student's Uniform Algorithm:
```
State: UNPAIRED | WAITING | PAIRED
Partner: initially NULL

1. Wait for message (blocking receive with MPI_ANY_SOURCE)
2. Message types:

   a. YOUR_TURN:
      - If already PAIRED: ignore
      - Select random unpaired student p (excluding self)
      - Send PAIR_REQUEST to p
      - State = WAITING

   b. PAIR_REQUEST from student s:
      - If UNPAIRED:
        * Accept pairing with s
        * Send PAIR_ACK to s
        * Partner = s
        * State = PAIRED
        * Pick random unpaired student and send YOUR_TURN
      - If WAITING or PAIRED:
        * Send PAIR_REJECT to s

   c. PAIR_ACK from student s:
      - Partner = s
      - State = PAIRED
      - Pick random unpaired student and send YOUR_TURN

   d. PAIR_REJECT from student s:
      - Pick different random unpaired student
      - Send PAIR_REQUEST again
      - (Or send YOUR_TURN to pass responsibility)

3. When PAIRED and all students paired: terminate
```

### Challenge: Tracking Global State
**Problem**: How does each student know who is unpaired?
**Solutions**:
1. **Broadcast on pairing**: When paired, broadcast to all students
2. **Gossip protocol**: Students maintain local view, update via messages
3. **Probe messages**: Query students before requesting pairing
4. **Accept-reject pattern**: Try random students, handle rejections

## Correctness Considerations

### Synchronization Requirements
1. **Mutual Exclusion on Pairing**: Student can pair with only one partner
2. **Atomic Pairing**: Both students must agree on pairing
3. **No Deadlock**: Students shouldn't wait indefinitely
4. **Termination**: All students must eventually be paired

### Potential Race Conditions
1. **Simultaneous Requests**: Two students request each other simultaneously
   - **Solution**: Use rank-based tie-breaking or accept first request
2. **Double Pairing**: Student pairs before rejecting pending request
   - **Solution**: Atomic state transition UNPAIRED → PAIRED
3. **Message Reordering**: Network delays cause out-of-order delivery
   - **Solution**: MPI guarantees FIFO between process pairs

### Deadlock Scenarios
1. **Circular Waiting**: A waits for B, B waits for C, C waits for A
   - **Prevention**: Use YOUR_TURN token to serialize selections
   - **Detection**: Timeout mechanisms
2. **Lost YOUR_TURN**: Token lost if sent to already-paired student
   - **Solution**: Paired students forward token to unpaired student

### Liveness Issues
1. **Infinite Loops**: Student keeps getting rejected
   - **Solution**: Eventually someone accepts (probabilistic progress)
2. **Token Loss**: YOUR_TURN message sent to paired student
   - **Solution**: Paired students pass token along

## Performance Considerations

### Message Complexity
**Ideal**: n messages total (as specified in problem)

**Achievable pattern** (n messages):
```
Teacher → Student1: YOUR_TURN
Student1 → Student2: PAIR_REQUEST
Student2 → Student3: YOUR_TURN (after accepting Student1)
Student3 → Student4: PAIR_REQUEST
... continues until all paired
```

**Realistic** with rejections:
- Best case: n messages
- Average case: 2n - 3n messages
- Worst case: O(n²) if many rejections occur

### Time Complexity
- **Sequential nature**: YOUR_TURN token serializes pairing
- **Best case**: O(n) if no rejections
- **Average case**: O(n) with low probability rejections
- **Worst case**: O(n²) if unlucky random selections

### Scalability
- **Better than Q1**: No centralized bottleneck
- **Communication**: Distributed, direct peer-to-peer
- **Limitation**: Token-based approach is sequential
- **Improvement**: Allow parallel pairing attempts (increases messages)

## Concurrency and Distribution Issues

### MPI-Specific Considerations
1. **Message Tags**: Use tags to distinguish message types
   ```c
   #define YOUR_TURN       1
   #define PAIR_REQUEST    2
   #define PAIR_ACK        3
   #define PAIR_REJECT     4
   #define PAIRED_BROADCAST 5
   ```

2. **Non-blocking Receives**: May need MPI_Probe or MPI_Irecv
   ```c
   MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
   ```

3. **Buffering**: Messages may arrive while processing another message

### Distributed Algorithm Challenges
1. **Global Termination Detection**: When do all students know pairing is complete?
   - **Solution 1**: Count mechanism (when n/2 pairs formed)
   - **Solution 2**: Termination detection algorithm (e.g., distributed snapshot)
   - **Solution 3**: Teacher collects completion messages

2. **Unpaired Student Discovery**: How to find unpaired students?
   - **Solution 1**: Maintain local list, update via broadcasts
   - **Solution 2**: Random probing with accept/reject
   - **Solution 3**: Coordinator maintains global state (hybrid approach)

### Odd n Handling
- Last student pairs with themselves
- Must detect when they're the only one left
- Requires knowledge of global pairing state

## Implementation Challenges

### State Management
```c
typedef enum {
    UNPAIRED,
    WAITING,      // Sent request, awaiting response
    PAIRED
} StudentState;

typedef struct {
    StudentState state;
    int partner;
    int total_students;
    bool *paired_students;  // Track who's paired (via broadcasts)
} StudentInfo;
```

### Message Handling Loop
```c
while (state != PAIRED || !all_students_paired()) {
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    switch (status.MPI_TAG) {
        case YOUR_TURN:
            handle_your_turn();
            break;
        case PAIR_REQUEST:
            handle_pair_request(status.MPI_SOURCE);
            break;
        case PAIR_ACK:
            handle_pair_ack(status.MPI_SOURCE);
            break;
        case PAIR_REJECT:
            handle_pair_reject(status.MPI_SOURCE);
            break;
        case PAIRED_BROADCAST:
            handle_paired_broadcast();
            break;
    }
}
```

### Random Selection Strategy
```c
int select_random_unpaired_student(StudentInfo *info) {
    int candidates[MAX_STUDENTS];
    int count = 0;

    for (int i = 1; i <= info->total_students; i++) {
        if (i != my_rank && !info->paired_students[i]) {
            candidates[count++] = i;
        }
    }

    if (count == 0) return -1;  // All paired or only self remains

    return candidates[rand() % count];
}
```

## Recommendations for Implementation

### 1. Algorithm Choice
**Recommended approach** (achieves close to n messages):
- Use YOUR_TURN token to serialize pairing attempts
- Student with token picks random unpaired peer
- On successful pairing, both students known to be paired
- Winner sends YOUR_TURN to next random unpaired student
- Use broadcast to announce pairings (adds overhead but ensures correctness)

### 2. Termination Detection
```c
// Option 1: Count pairs
int total_pairs = (total_students % 2 == 0) ? total_students/2 : (total_students+1)/2;
int pairs_formed = 0;  // Increment on each PAIRED_BROADCAST

// Option 2: Teacher collects completion messages
if (state == PAIRED) {
    MPI_Send(&partner, 1, MPI_INT, 0, COMPLETION, MPI_COMM_WORLD);
}
```

### 3. Testing Strategy
- Test with n = 2, 3, 4, 5, 10, 20
- Run multiple times (non-deterministic due to randomness)
- Verify each student pairs exactly once
- Check odd n case
- Measure message count

### 4. Debugging
- Print detailed trace initially
- Log message type, source, destination
- Track state transitions
- Verify no student paired twice
- Check for deadlock (add timeouts)

### 5. Edge Cases
- n = 1: Student pairs with self immediately
- n = 2: Simple pair formation
- Odd n: Last student self-pairs
- All students reject: Should eventually succeed via random selection

## Comparison with Question 1

| Aspect | Q1 (Client-Server) | Q2 (Peer-to-Peer) |
|--------|-------------------|-------------------|
| Architecture | Centralized | Distributed |
| Messages | Exactly 2n | Ideally n, realistically 2n-3n |
| Complexity | Simple | Complex |
| Scalability | Server bottleneck | Better distributed |
| Determinism | Deterministic | Non-deterministic |
| Fault tolerance | Single point of failure | More resilient |

## Advanced Optimizations

### 1. Parallel Pairing
- Allow multiple YOUR_TURN tokens
- Students can pair concurrently
- Increases messages but reduces latency
- Requires collision handling

### 2. Preference Lists
- Pre-generate random preference order
- Try preferences in order
- Similar to stable marriage problem (Q5)

### 3. Two-phase Approach
- Phase 1: Collect all students' random choices
- Phase 2: Resolve conflicts and form pairs
- More messages but simpler logic

## Expected Learning Outcomes
1. Design of distributed peer-to-peer algorithms
2. Handling non-determinism and randomness in distributed systems
3. Distributed termination detection
4. Managing distributed state without shared memory
5. Trade-offs between message complexity and algorithm complexity
6. Debugging distributed systems with race conditions
7. Token-based coordination patterns
