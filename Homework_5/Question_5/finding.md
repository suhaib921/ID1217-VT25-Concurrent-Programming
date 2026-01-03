# Question 5: Stable Marriage Problem (Distributed) - Analysis

## Summary of the Task
Implement a distributed solution to the classic Stable Marriage Problem:
- **2n processes**: n men and n women
- **Preferences**: Each person ranks all members of opposite gender (1 to n)
- **Goal**: Find stable pairings (marriages)
- **Stability**: No two people prefer each other over their current partners
- **Algorithm**: Men propose, women accept/reject
- **Implementation**: Distributed using message passing (MPI)
- **Output**: Trace of interesting events (proposals, acceptances, rejections, breakups)

## Implementation Status
**NOT IMPLEMENTED** - Only the problem description exists. No code files are present in this directory.

## Required Implementation Approach

### Architecture
This problem requires a **peer-to-peer distributed architecture** using MPI:
- n man processes (e.g., ranks 0 to n-1)
- n woman processes (e.g., ranks n to 2n-1)
- Optional: 1 coordinator process for termination detection (rank 2n)
- Total MPI processes: 2n or 2n+1

### Classic Stable Marriage Problem

#### Problem Definition:
```
Input:
- n men: M[1..n]
- n women: W[1..n]
- Each man has preference list ranking all women: rank_m[woman] ∈ [1..n]
- Each woman has preference list ranking all men: rank_w[man] ∈ [1..n]

Output:
- Set of n pairings: (man, woman) pairs
- Pairing is STABLE if no blocking pair exists

Blocking Pair:
- (M[i], W[p]) and (M[j], W[q]) are two pairings
- M[i] prefers W[q] over W[p] AND W[q] prefers M[i] over M[j]
- This is unstable (they would leave their partners for each other)
```

#### Gale-Shapley Algorithm (Centralized):
```
Initialize all men and women to free

While exists free man m who hasn't proposed to all women:
    w = highest-ranked woman m hasn't proposed to yet
    m proposes to w

    If w is free:
        w accepts m (they become engaged)
    Else:
        current_partner = w's current partner
        If w prefers m over current_partner:
            w breaks up with current_partner (he becomes free)
            w accepts m (they become engaged)
        Else:
            w rejects m
```

**Properties**:
- Always terminates in at most n² proposals
- Produces stable matching
- Man-optimal: Each man gets best possible partner in any stable matching
- Woman-pessimal: Each woman gets worst possible partner in any stable matching

### Distributed Algorithm Design

#### Process Types:
```
Man Process (Proposer):
- Maintains preference list of women
- Maintains current proposal index
- Maintains current partner (or NULL if free)

Woman Process (Acceptor):
- Maintains preference list of men
- Maintains current partner (or NULL if free)
- Evaluates proposals based on preferences

Coordinator Process (Optional):
- Counts total successful engagements
- Detects termination when n women are engaged
- Broadcasts termination signal
```

#### Message Types:
```c
#define PROPOSAL        1  // Man proposes to woman
#define ACCEPT          2  // Woman accepts proposal
#define REJECT          3  // Woman rejects proposal
#define BREAKUP         4  // Woman breaks engagement
#define STATUS_REQUEST  5  // Query engagement status
#define STATUS_RESPONSE 6  // Respond with status
#define TERMINATE       7  // Termination signal
#define ENGAGED_NOTIFY  8  // Notify coordinator of engagement
```

#### Man's Algorithm:
```
State: FREE | ENGAGED
Partner: woman_id or NULL
Next_proposal_index: 0 (start with most preferred)

Loop:
1. If FREE:
   a. If next_proposal_index < n:
      - woman_id = preference_list[next_proposal_index]
      - Send PROPOSAL to woman_id
      - next_proposal_index++
      - Wait for ACCEPT or REJECT
   b. Else:
      - All proposals exhausted (shouldn't happen in valid algorithm)

2. Receive messages:
   a. ACCEPT from woman_id:
      - Partner = woman_id
      - State = ENGAGED
      - Print "Man X engaged to Woman Y"

   b. REJECT from woman_id:
      - Continue proposing (loop to step 1)
      - Print "Man X rejected by Woman Y"

   c. BREAKUP from woman_id:
      - Partner = NULL
      - State = FREE
      - Print "Man X dumped by Woman Y"
      - Continue proposing

   d. TERMINATE:
      - Exit loop

3. Check termination:
   - If ENGAGED and all women engaged: done
```

#### Woman's Algorithm:
```
State: FREE | ENGAGED
Partner: man_id or NULL
First_engagement: false (for termination counting)

Loop:
1. Wait for message (MPI_Recv with MPI_ANY_SOURCE)

2. Receive messages:
   a. PROPOSAL from man_id:
      - If FREE:
        * Partner = man_id
        * State = ENGAGED
        * Send ACCEPT to man_id
        * Print "Woman X accepted Man Y"
        * If !first_engagement:
          - Send ENGAGED_NOTIFY to coordinator
          - first_engagement = true
      - Else (ENGAGED):
        * Compare man_id with current Partner using preference list
        * If prefers man_id:
          - Send BREAKUP to current Partner
          - Print "Woman X dumped Man P for Man Y"
          - Partner = man_id
          - Send ACCEPT to man_id
        * Else:
          - Send REJECT to man_id
          - Print "Woman X rejected Man Y (staying with Man P)"

   b. TERMINATE:
      - Exit loop
```

#### Coordinator's Algorithm (Termination Detection):
```
engaged_count = 0

Loop:
1. Receive ENGAGED_NOTIFY from any woman
2. engaged_count++
3. If engaged_count == n:
   - All women engaged, algorithm complete
   - Send TERMINATE to all men and women
   - Exit
```

## Correctness Considerations

### Synchronization Requirements
1. **Mutual Exclusion**: Woman engaged to at most one man at a time
2. **Atomic Breakup-Accept**: Woman breaks up and accepts new partner atomically
3. **Proposal Ordering**: Men propose in preference order
4. **Termination**: All processes must detect completion

### Correctness Guarantees
1. **Termination**: At most n² proposals (each man proposes to each woman at most once)
2. **Stability**: Gale-Shapley guarantees stable matching
3. **Completeness**: All participants eventually paired
4. **Determinism**: Given same preferences, always produces same result (man-optimal)

### Potential Race Conditions
1. **Simultaneous Proposals**: Woman receives multiple proposals
   - **Safe**: Woman processes sequentially (MPI_Recv is sequential)
2. **Breakup Message Timing**: Broken-up man receives breakup while proposing elsewhere
   - **Safe**: State transitions handled atomically within each process
3. **Termination Race**: Some processes terminate before others receive final messages
   - **Solution**: Coordinator ensures orderly shutdown

### Edge Cases
1. **n = 1**: Single man and woman (trivial pairing)
2. **Identical preferences**: All men rank women the same way
3. **Reverse preferences**: Men and women have opposite rankings
4. **Cyclic preferences**: M1→W1→M2→W2→M3→W3→M1

## Performance Considerations

### Message Complexity
- **Best case**: n proposals (each man proposes once, accepted immediately)
  - Unlikely: requires perfect alignment of preferences
- **Average case**: O(n log n) proposals (probabilistic analysis)
- **Worst case**: n² proposals (each man proposes to all women)
  - Example: All men rank women identically, women rank men reverse

**Total messages**:
- Proposals: O(n²) worst case
- Responses (accept/reject): O(n²) worst case
- Breakups: O(n²) worst case (woman can break up multiple times)
- Termination: O(n) messages
- **Total: O(n²) worst case**

### Time Complexity
- **Proposals**: O(n²) iterations worst case
- **Preference Lookup**: O(n) per proposal (can optimize to O(1) with preprocessing)
- **Total: O(n³) worst case without optimization**
- **With preprocessing**: O(n²) time

### Scalability
- **Good**: Distributed processing, no centralized bottleneck
- **Sequential dependencies**: Men propose sequentially through their lists
- **Parallelism**: Multiple men can propose simultaneously to different women
- **Network load**: O(n²) messages distributed across 2n processes

## Implementation Recommendations

### 1. Data Structures

#### Man Process:
```c
typedef struct {
    int id;                      // My ID (rank)
    int n;                       // Number of men/women
    int *preference_list;        // preference_list[i] = woman_id (ranked)
    int next_proposal_idx;       // Next woman to propose to
    int partner;                 // Current partner woman_id (-1 if free)
    bool engaged;
} ManState;
```

#### Woman Process:
```c
typedef struct {
    int id;                      // My ID (rank)
    int n;                       // Number of men/women
    int *preference_list;        // preference_list[i] = man_id (ranked)
    int *rank;                   // rank[man_id] = preference rank (inverse mapping)
    int partner;                 // Current partner man_id (-1 if free)
    bool engaged;
    bool first_engagement;       // For coordinator notification
} WomanState;
```

#### Message Structure:
```c
typedef struct {
    int proposer_id;
    int target_id;
} ProposalMessage;
```

### 2. Preprocessing - Inverse Ranking
```c
// Woman: Create inverse ranking for O(1) preference comparison
void create_rank_array(int *preference_list, int *rank, int n) {
    for (int i = 0; i < n; i++) {
        int man_id = preference_list[i];
        rank[man_id] = i;  // Lower rank = higher preference
    }
}

// Compare two men
bool prefers(WomanState *w, int man1, int man2) {
    return w->rank[man1] < w->rank[man2];
}
```

### 3. Preference List Initialization
```c
// Random preferences (for testing)
void generate_random_preferences(int *pref, int n, int my_id) {
    // Initialize with identity permutation
    for (int i = 0; i < n; i++) {
        pref[i] = i;
    }
    // Fisher-Yates shuffle
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = pref[i];
        pref[i] = pref[j];
        pref[j] = temp;
    }
}

// Or load from file for reproducibility
void load_preferences(const char *filename, int *pref, int n) {
    FILE *f = fopen(filename, "r");
    for (int i = 0; i < n; i++) {
        fscanf(f, "%d", &pref[i]);
    }
    fclose(f);
}
```

### 4. Man Process Implementation
```c
void man_process(int rank, int n) {
    ManState state;
    state.id = rank;
    state.n = n;
    state.preference_list = malloc(n * sizeof(int));
    state.next_proposal_idx = 0;
    state.partner = -1;
    state.engaged = false;

    initialize_preferences(&state);

    while (!state.engaged) {
        if (state.next_proposal_idx >= n) {
            fprintf(stderr, "ERROR: Man %d exhausted all proposals\n", rank);
            break;
        }

        // Propose to next woman on preference list
        int woman_id = state.preference_list[state.next_proposal_idx];
        printf("[Man %d] Proposing to Woman %d\n", state.id, woman_id);

        ProposalMessage msg = {state.id, woman_id};
        MPI_Send(&msg, sizeof(ProposalMessage), MPI_BYTE,
                 woman_id + n, PROPOSAL, MPI_COMM_WORLD);

        state.next_proposal_idx++;

        // Wait for response
        MPI_Status status;
        ProposalMessage response;
        MPI_Recv(&response, sizeof(ProposalMessage), MPI_BYTE,
                 woman_id + n, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == ACCEPT) {
            state.partner = woman_id;
            state.engaged = true;
            printf("[Man %d] ENGAGED to Woman %d\n", state.id, woman_id);
        }
        else if (status.MPI_TAG == REJECT) {
            printf("[Man %d] Rejected by Woman %d\n", state.id, woman_id);
            // Continue loop to propose to next woman
        }
    }

    // Wait for termination signal
    MPI_Status status;
    char buffer;
    MPI_Recv(&buffer, 1, MPI_CHAR, 2*n, TERMINATE, MPI_COMM_WORLD, &status);

    free(state.preference_list);
}
```

### 5. Woman Process Implementation
```c
void woman_process(int rank, int n) {
    WomanState state;
    state.id = rank - n;  // Adjust rank to woman ID
    state.n = n;
    state.preference_list = malloc(n * sizeof(int));
    state.rank = malloc(n * sizeof(int));
    state.partner = -1;
    state.engaged = false;
    state.first_engagement = false;

    initialize_preferences(&state);
    create_rank_array(state.preference_list, state.rank, n);

    while (true) {
        MPI_Status status;
        ProposalMessage msg;
        MPI_Recv(&msg, sizeof(ProposalMessage), MPI_BYTE,
                 MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TERMINATE) {
            printf("[Woman %d] Final partner: Man %d\n", state.id, state.partner);
            break;
        }

        if (status.MPI_TAG == PROPOSAL) {
            int proposer = msg.proposer_id;
            printf("[Woman %d] Received proposal from Man %d\n", state.id, proposer);

            if (!state.engaged) {
                // Accept first proposal
                state.partner = proposer;
                state.engaged = true;
                printf("[Woman %d] ACCEPTED Man %d\n", state.id, proposer);

                ProposalMessage response = {state.id, proposer};
                MPI_Send(&response, sizeof(ProposalMessage), MPI_BYTE,
                         proposer, ACCEPT, MPI_COMM_WORLD);

                // Notify coordinator on first engagement
                if (!state.first_engagement) {
                    char notify = 1;
                    MPI_Send(&notify, 1, MPI_CHAR, 2*n, ENGAGED_NOTIFY, MPI_COMM_WORLD);
                    state.first_engagement = true;
                }
            }
            else {
                // Compare with current partner
                if (prefers(&state, proposer, state.partner)) {
                    // Prefer new proposer, break up with current
                    printf("[Woman %d] Dumping Man %d for Man %d\n",
                           state.id, state.partner, proposer);

                    ProposalMessage breakup_msg = {state.id, state.partner};
                    MPI_Send(&breakup_msg, sizeof(ProposalMessage), MPI_BYTE,
                             state.partner, BREAKUP, MPI_COMM_WORLD);

                    state.partner = proposer;
                    ProposalMessage accept_msg = {state.id, proposer};
                    MPI_Send(&accept_msg, sizeof(ProposalMessage), MPI_BYTE,
                             proposer, ACCEPT, MPI_COMM_WORLD);
                }
                else {
                    // Reject new proposer
                    printf("[Woman %d] Rejecting Man %d (keeping Man %d)\n",
                           state.id, proposer, state.partner);

                    ProposalMessage reject_msg = {state.id, proposer};
                    MPI_Send(&reject_msg, sizeof(ProposalMessage), MPI_BYTE,
                             proposer, REJECT, MPI_COMM_WORLD);
                }
            }
        }
    }

    free(state.preference_list);
    free(state.rank);
}
```

### 6. Coordinator Process Implementation
```c
void coordinator_process(int n) {
    int engaged_count = 0;

    while (engaged_count < n) {
        MPI_Status status;
        char buffer;
        MPI_Recv(&buffer, 1, MPI_CHAR, MPI_ANY_SOURCE,
                 ENGAGED_NOTIFY, MPI_COMM_WORLD, &status);

        engaged_count++;
        printf("[Coordinator] %d/%d women engaged\n", engaged_count, n);
    }

    printf("[Coordinator] All women engaged. Stable matching complete!\n");

    // Send termination to all men and women
    char terminate_msg = 0;
    for (int i = 0; i < 2*n; i++) {
        MPI_Send(&terminate_msg, 1, MPI_CHAR, i, TERMINATE, MPI_COMM_WORLD);
    }
}
```

### 7. Main Program
```c
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) {
            printf("Usage: %s <num_pairs>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    int n = atoi(argv[1]);

    if (size != 2*n + 1) {
        if (rank == 0) {
            printf("Error: Expected %d processes (n=%d), got %d\n", 2*n+1, n, size);
        }
        MPI_Finalize();
        return 1;
    }

    srand(time(NULL) + rank);  // Unique seed per process

    if (rank < n) {
        man_process(rank, n);
    }
    else if (rank < 2*n) {
        woman_process(rank, n);
    }
    else {
        coordinator_process(n);
    }

    MPI_Finalize();
    return 0;
}
```

### 8. Compilation and Execution
```bash
# Compilation
mpicc -o stable_marriage stable_marriage.c

# Execution with n=5 pairs (11 processes: 5 men + 5 women + 1 coordinator)
mpirun -np 11 ./stable_marriage 5

# With hostfile for true distribution
mpirun -np 11 --hostfile hosts ./stable_marriage 5
```

## Testing and Validation

### 1. Stability Verification
```c
void verify_stability(int *men_partners, int *women_partners,
                      int **men_prefs, int **women_prefs, int n) {
    for (int m = 0; m < n; m++) {
        for (int w = 0; w < n; w++) {
            int m_partner = men_partners[m];
            int w_partner = women_partners[w];

            // Check if (m, w) is a blocking pair
            bool m_prefers_w = rank_of(men_prefs[m], w, n) < rank_of(men_prefs[m], m_partner, n);
            bool w_prefers_m = rank_of(women_prefs[w], m, n) < rank_of(women_prefs[w], w_partner, n);

            if (m_prefers_w && w_prefers_m) {
                printf("INSTABILITY: Man %d and Woman %d form blocking pair!\n", m, w);
            }
        }
    }
    printf("Matching is STABLE\n");
}
```

### 2. Test Cases
```
Test 1: Simple 2x2
Men preferences:    Women preferences:
M0: [W0, W1]        W0: [M0, M1]
M1: [W0, W1]        W1: [M1, M0]
Expected: (M0,W0), (M1,W1)

Test 2: Reverse preferences
Men preferences:    Women preferences:
M0: [W0, W1, W2]    W0: [M2, M1, M0]
M1: [W0, W1, W2]    W1: [M2, M1, M0]
M2: [W0, W1, W2]    W2: [M2, M1, M0]
Expected: (M0,W2), (M1,W1), (M2,W0)
```

## Expected Learning Outcomes
1. Classic combinatorial algorithm in distributed setting
2. Distributed coordination with asymmetric roles (proposers vs acceptors)
3. Termination detection with coordinator pattern
4. Message-driven state machines
5. Stability verification in distributed systems
6. Trade-offs between centralized and distributed coordination
7. Handling complex interaction patterns (proposals, acceptances, breakups)
