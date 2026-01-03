/**
 * @file stable_marriage.c
 * @brief Solves the Stable Marriage Problem using a distributed MPI implementation.
 *
 * This program implements the Gale-Shapley algorithm in a distributed environment.
 * The processes are divided into three groups:
 * - A single Coordinator (Rank 0).
 * - `N` Men processes (Ranks 1 to N).
 * - `N` Women processes (Ranks N+1 to 2N).
 *
 * Algorithm:
 * 1. Initialization: Each man and woman process has a predefined preference list.
 *    All men start as "free".
 *
 * 2. Proposals:
 *    - Each free man proposes to the next woman on his preference list by sending
 *      a `PROPOSAL` message containing his rank.
 *
 * 3. Decisions:
 *    - A woman process waits for proposals.
 *    - If a woman is free and receives a proposal, she becomes engaged to the
 *      proposer and sends back an `ACCEPT` message. She also notifies the
 *      coordinator that she is now engaged.
 *    - If a woman is already engaged and receives a new proposal, she compares
 *      the new suitor with her current partner based on her preference list.
 *    - If the new suitor is preferred, she dumps her current partner (sends him
 *      a `REJECT` message, making him free again) and accepts the new one
 *      (sends an `ACCEPT` message).
 *    - If the current partner is preferred, she rejects the new suitor (sends a
 *      `REJECT` message).
 *
 * 4. Termination:
 *    - The Coordinator process waits for `N` "engaged" notifications from the women.
 *    - Once `N` women are engaged, the solution is stable, and the coordinator
 *      broadcasts a `TERMINATE` signal to all processes.
 *
 * To compile:
 *   mpicc -o stable_marriage stable_marriage.c
 *
 * To run (e.g., for N=5):
 *   mpiexec -n 11 ./stable_marriage
 *   (Note: -n must be 2*N + 1)
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 5 // Number of men/women

#define COORDINATOR_RANK 0

// Tags for messages
#define TAG_PROPOSAL 1
#define TAG_ACCEPT 2
#define TAG_REJECT 3
#define TAG_BREAKUP 4 // New tag for when a woman dumps a man
#define TAG_ENGAGED_NOTIFICATION 5
#define TAG_TERMINATE 5

// Helper to get woman's rank from her 0-indexed ID
#define WOMAN_RANK(id) (N + 1 + id)
// Helper to get woman's 0-indexed ID from her rank
#define WOMAN_ID(rank) (rank - N - 1)

// --- Preference Data ---
// Men's preferences for women (0-indexed)
const int men_prefs[N][N] = {
    {1, 0, 3, 4, 2},
    {3, 1, 0, 2, 4},
    {1, 4, 2, 3, 0},
    {0, 3, 2, 1, 4},
    {1, 3, 0, 4, 2}
};

// Women's preferences for men (0-indexed men IDs)
const int women_prefs[N][N] = {
    {4, 0, 1, 3, 2},
    {2, 1, 3, 0, 4},
    {1, 2, 3, 4, 0},
    {0, 4, 3, 2, 1},
    {3, 1, 4, 2, 0}
};

// Inverse of women's prefs for O(1) lookups
// women_inv_prefs[woman_id][man_id] = preference_level
int women_inv_prefs[N][N];

void populate_inverse_prefs() {
    for (int w = 0; w < N; w++) {
        for (int i = 0; i < N; i++) {
            women_inv_prefs[w][women_prefs[w][i]] = i;
        }
    }
}

// --- Process Functions ---

void man_process(int rank) {
    int man_id = MAN_ID_FROM_RANK(rank);
    bool is_free = true;
    int current_woman_to_propose_idx = 0; // Index in his preference list
    int my_partner_id = -1; // -1 if not engaged (woman's 0-indexed ID)

    // MPI Request for non-blocking termination probe
    MPI_Request terminate_req;
    int terminate_flag = 0;
    MPI_Irecv(NULL, 0, MPI_INT, COORDINATOR_RANK, TAG_TERMINATE, MPI_COMM_WORLD, &terminate_req);

    while (!terminate_flag) {
        // Check for termination signal
        MPI_Test(&terminate_req, &terminate_flag, MPI_STATUS_IGNORE);
        if (terminate_flag) break;

        // --- Handle incoming messages (e.g., breakups) first ---
        MPI_Status status_probe;
        int msg_flag;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_flag, &status_probe);
        
        if (msg_flag) {
            int msg_type = status_probe.MPI_TAG;
            if (msg_type == TAG_BREAKUP) {
                // Man has been dumped. This message comes from his *previous* partner.
                // Receive the message
                MPI_Recv(NULL, 0, MPI_INT, status_probe.MPI_SOURCE, TAG_BREAKUP, MPI_COMM_WORLD, &status_probe);
                if (!is_free) { // If he was currently engaged to this woman
                    printf("Man %d (Rank %d) was DUMPED by Woman %d (Rank %d).\n", man_id, rank, my_partner_id, status_probe.MPI_SOURCE);
                    is_free = true;
                    my_partner_id = -1;
                    // He is now free and will propose to the next woman on his list (current_woman_to_propose_idx is not reset)
                } else {
                    // This case handles a delayed breakup message if he was already free
                    // or currently proposing to someone else.
                    printf("Man %d (Rank %d) received a delayed BREAKUP from Woman %d (Rank %d) while free/proposing.\n", man_id, rank, WOMAN_ID_FROM_RANK(status_probe.MPI_SOURCE), status_probe.MPI_SOURCE);
                }
                continue; // Process next iteration to handle free state
            } else if (msg_type == TAG_TERMINATE) { // Handle termination signal if probed
                MPI_Recv(NULL, 0, MPI_INT, status_probe.MPI_SOURCE, TAG_TERMINATE, MPI_COMM_WORLD, &status_probe);
                terminate_flag = 1;
                break;
            } else if (msg_type == TAG_ACCEPT || msg_type == TAG_REJECT) {
                // These are responses to his proposals.
                // We should only receive these if he's actively proposing.
                // These are handled below in the proposal block, so just ignore or log here.
                // The proposal block will MPI_Probe, MPI_Recv for its specific target.
                // If it's a message from his current target woman, it'll be handled.
                // Otherwise, it might be a delayed response he wasn't expecting, which is fine.
                int dummy;
                MPI_Recv(&dummy, 1, MPI_INT, status_probe.MPI_SOURCE, msg_type, MPI_COMM_WORLD, &status_probe);
                printf("Man %d (Rank %d) received unexpected message (Tag %d) from Rank %d while engaged or busy.\n", man_id, rank, msg_type, status_probe.MPI_SOURCE);
                continue;
            }
        }
        // --- End handling incoming messages ---

        if (is_free) {
            if (current_woman_to_propose_idx < N) {
                int woman_id = men_prefs[man_id][current_woman_to_propose_idx];
                int woman_rank = WOMAN_RANK(woman_id);
                
                printf("Man %d (Rank %d) proposes to Woman %d (Rank %d).\n", man_id, rank, woman_id, woman_rank);
                MPI_Send(&rank, 1, MPI_INT, woman_rank, TAG_PROPOSAL, MPI_COMM_WORLD);

                // Wait for a reply from the woman he just proposed to
                MPI_Status response_status;
                int response_flag = 0;
                while (!response_flag && !terminate_flag) {
                    MPI_Iprobe(woman_rank, MPI_ANY_TAG, MPI_COMM_WORLD, &response_flag, &response_status);
                    MPI_Test(&terminate_req, &terminate_flag, MPI_STATUS_IGNORE);
                    if (!response_flag && !terminate_flag) usleep(100); // Small sleep
                }
                if (terminate_flag) break;

                int msg_type = response_status.MPI_TAG;
                
                // Receive the actual message
                MPI_Recv(NULL, 0, MPI_INT, woman_rank, msg_type, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 

                if (msg_type == TAG_ACCEPT) {
                    is_free = false;
                    my_partner_id = woman_id;
                    printf("Man %d (Rank %d) is now engaged to Woman %d (Rank %d).\n", man_id, rank, woman_id, woman_rank);
                    // Do not increment current_woman_to_propose_idx as he is now engaged
                } else if (msg_type == TAG_REJECT) {
                    printf("Man %d (Rank %d) was rejected by Woman %d (Rank %d).\n", man_id, rank, woman_id, woman_rank);
                    current_woman_to_propose_idx++; // Propose to next woman on his list
                }
                // TAG_BREAKUP is handled by the Iprobe at the beginning of the loop
            } else {
                // Man has proposed to all women and is still free. This shouldn't happen
                // in a valid Gale-Shapley execution for N men and N women, as all women
                // should eventually be engaged.
                fprintf(stderr, "Man %d (Rank %d) exhausted all proposals and is still free. This indicates an algorithm or logic error.\n", man_id, rank);
                terminate_flag = 1; // Force termination to avoid infinite loop
            }
        } else { // Man is engaged (is_free == false)
            // If engaged, just wait for termination signal. Breakups are handled by Iprobe at loop start.
            usleep(100); // Small sleep to prevent busy-waiting if there are no messages
        }
    }
    printf("Man %d (Rank %d) terminating.\n", man_id, rank);
    // Send termination acknowledgement to coordinator
    MPI_Send(NULL, 0, MPI_INT, COORDINATOR_RANK, TAG_TERMINATE, MPI_COMM_WORLD);
}

void woman_process(int rank) {
    int woman_id = WOMAN_ID_FROM_RANK(rank);
    int my_partner_rank = -1; // Man's MPI rank
    int my_partner_id = -1;   // Man's 0-indexed ID

    bool is_engaged = false;
    bool first_engagement_notified = false; // To notify coordinator only once

    // MPI Request for non-blocking termination probe
    MPI_Request terminate_req;
    int terminate_flag = 0;
    MPI_Irecv(NULL, 0, MPI_INT, COORDINATOR_RANK, TAG_TERMINATE, MPI_COMM_WORLD, &terminate_req);

    while (!terminate_flag) {
        // Check for termination signal
        MPI_Test(&terminate_req, &terminate_flag, MPI_STATUS_IGNORE);
        if (terminate_flag) break;
        
        // Wait for any message (proposal or termination)
        MPI_Status status;
        // Use Iprobe to check for messages without blocking
        int msg_flag = 0;
        while (!msg_flag && !terminate_flag) {
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_flag, &status);
            MPI_Test(&terminate_req, &terminate_flag, MPI_STATUS_IGNORE);
            if (!msg_flag && !terminate_flag) usleep(100); // Small sleep to prevent busy-waiting
        }
        if (terminate_flag) break; // Exit if termination signal received while waiting
        
        int msg_type = status.MPI_TAG;
        int sender_rank = status.MPI_SOURCE;
        
        // Handle termination signal if probed
        if (msg_type == TAG_TERMINATE) {
             MPI_Recv(NULL, 0, MPI_INT, sender_rank, TAG_TERMINATE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
             terminate_flag = 1;
             break;
        }
        
        if (msg_type == TAG_PROPOSAL) {
            int suitor_rank;
            MPI_Recv(&suitor_rank, 1, MPI_INT, sender_rank, TAG_PROPOSAL, MPI_COMM_WORLD, &status);
            int suitor_id = MAN_ID_FROM_RANK(suitor_rank);
            printf("Woman %d (Rank %d) received proposal from Man %d (Rank %d).\n", woman_id, rank, suitor_id, suitor_rank);

            if (!is_engaged) {
                // Free, so accept
                my_partner_rank = suitor_rank;
                my_partner_id = suitor_id;
                is_engaged = true;
                MPI_Send(NULL, 0, MPI_INT, suitor_rank, TAG_ACCEPT, MPI_COMM_WORLD);
                printf("Woman %d (Rank %d) ACCEPTS Man %d (Rank %d).\n", woman_id, rank, suitor_id, suitor_rank);
                // Notify coordinator on first engagement
                if (!first_engagement_notified) {
                    MPI_Send(NULL, 0, MPI_INT, COORDINATOR_RANK, TAG_ENGAGED_NOTIFICATION, MPI_COMM_WORLD);
                    first_engagement_notified = true;
                }
            } else {
                // Engaged, check preferences
                int current_partner_id = MAN_ID_FROM_RANK(my_partner_rank);
                if (women_inv_prefs[woman_id][suitor_id] < women_inv_prefs[woman_id][current_partner_id]) {
                    // New suitor is better
                    printf("Woman %d (Rank %d) DUMPS Man %d (Rank %d) for Man %d (Rank %d).\n", woman_id, rank, current_partner_id, my_partner_rank, suitor_id, suitor_rank);
                    // Dump old partner (send BREAKUP message)
                    MPI_Send(NULL, 0, MPI_INT, my_partner_rank, TAG_BREAKUP, MPI_COMM_WORLD);
                    
                    // Accept new partner
                    my_partner_rank = suitor_rank;
                    my_partner_id = suitor_id;
                    MPI_Send(NULL, 0, MPI_INT, suitor_rank, TAG_ACCEPT, MPI_COMM_WORLD);
                } else {
                    // Current partner is better
                    printf("Woman %d (Rank %d) REJECTS Man %d (Rank %d) (keeping Man %d).\n", woman_id, rank, suitor_id, suitor_rank, current_partner_id);
                    MPI_Send(NULL, 0, MPI_INT, suitor_rank, TAG_REJECT, MPI_COMM_WORLD);
                }
            }
        }
    }
    printf("Woman %d (Rank %d) is finally engaged to Man %d (Rank %d). Terminating.\n", woman_id, rank, my_partner_id, my_partner_rank);
    // Send termination acknowledgement to coordinator
    MPI_Send(NULL, 0, MPI_INT, COORDINATOR_RANK, TAG_TERMINATE, MPI_COMM_WORLD);
}

void coordinator_process() {
    int engaged_count = 0;
    printf("Coordinator started. Waiting for %d engagements.\n", N);
    
    while (engaged_count < N) {
        MPI_Recv(NULL, 0, MPI_INT, MPI_ANY_SOURCE, TAG_ENGAGED_NOTIFICATION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        engaged_count++;
        printf("Coordinator: %d women now engaged.\n", engaged_count);
    }
    
    printf("Coordinator: All women are engaged. Broadcasting termination signal.\n");
    for (int i = 1; i <= 2 * N; i++) {
        MPI_Send(NULL, 0, MPI_INT, i, TAG_TERMINATE, MPI_COMM_WORLD);
    }
}


int main(int argc, char* argv[]) {
    int rank, world_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size != 2 * N + 1) {
        if (rank == 0) {
            fprintf(stderr, "This application requires exactly %d processes (%d men, %d women, 1 coordinator).\n", 2 * N + 1, N, N);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    populate_inverse_prefs();

    if (rank == COORDINATOR_RANK) {
        coordinator_process();
    } else if (rank <= N) {
        man_process(rank);
    } else {
        woman_process(rank);
    }

    MPI_Finalize();
    return 0;
}
