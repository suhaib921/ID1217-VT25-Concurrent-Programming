/**
 * @file dining_philosophers_dist.c
 * @brief A distributed simulation of the Dining Philosophers problem using MPI.
 *
 * This program models the classic Dining Philosophers problem in a distributed
 * environment using a client-server architecture.
 *
 * Architecture:
 * - Process 0 acts as the "Table Server". It manages the state of all forks
 *   and arbitrates requests from the philosophers.
 * - Processes 1 to 5 act as the "Philosopher Clients".
 *
 * Algorithm:
 * 1. The Table Server (Rank 0) maintains an array representing the availability
 *    of the 5 forks. It also manages a queue of pending fork requests.
 * 2. Each Philosopher (Rank 1-5) runs in a loop of thinking, getting hungry,
 *    eating, and releasing forks.
 * 3. When a philosopher wants to eat, it sends a `TAG_GET_FORKS` request message
 *    to the Table Server.
 * 4. The Table Server receives requests. It attempts to grant the request immediately
 *    if both forks are available. If not, the request is added to a queue.
 *    The server continuously checks its queue and grants requests when forks become free.
 * 5. The philosopher waits for a `TAG_OK_TO_EAT` message before it can "eat".
 * 6. After eating, the philosopher sends a `TAG_REL_FORKS` message to the server.
 * 7. The server receives the `TAG_REL_FORKS` message, marks the forks as available,
 *    and then tries to grant pending requests from its queue.
 *
 * To compile:
 *   mpicc -o dining_philosophers_dist dining_philosophers_dist.c
 *
 * To run (requires 6 processes):
 *   mpiexec -n 6 ./dining_philosophers_dist <num_rounds>
 * Example:   mpiexec -n 6 ./dining_philosophers_dist 5
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#define NUM_PHILOSOPHERS 5
#define SERVER_RANK 0

// Message Tags
#define TAG_GET_FORKS 1
#define TAG_REL_FORKS 2
#define TAG_OK_TO_EAT 3
#define TAG_TERMINATE 4

// Structure for a fork request
typedef struct {
    int philosopher_rank;
    int left_fork;
    int right_fork;
} ForkRequest;

// Queue for pending fork requests
ForkRequest request_queue[NUM_PHILOSOPHERS];
int queue_head = 0;
int queue_tail = 0;
int queue_size = 0;

void enqueue_request(ForkRequest req) {
    if (queue_size < NUM_PHILOSOPHERS) {
        request_queue[queue_tail] = req;
        queue_tail = (queue_tail + 1) % NUM_PHILOSOPHERS;
        queue_size++;
    } else {
        fprintf(stderr, "Server: Request queue full, dropping request from P%d\n", req.philosopher_rank);
    }
}

ForkRequest dequeue_request() {
    ForkRequest req = request_queue[queue_head];
    queue_head = (queue_head + 1) % NUM_PHILOSOPHERS;
    queue_size--;
    return req;
}

// Function to try and grant a fork request
void try_grant_forks(int forks_status[], ForkRequest req) {
    int phil_id = req.philosopher_rank - 1; // 0-indexed for forks_status
    int left_fork = phil_id; // Fork to philosopher's left (same as philosopher's ID)
    int right_fork = (phil_id + 1) % NUM_PHILOSOPHERS; // Fork to philosopher's right

    // Handle asymmetric fork picking for deadlock prevention
    // Philosopher NUM_PHILOSOPHERS-1 (ID 4) picks up right fork then left
    if (phil_id == NUM_PHILOSOPHERS - 1) { // Philosopher 4
        left_fork = (phil_id + 1) % NUM_PHILOSOPHERS; // Fork 0
        right_fork = phil_id;                          // Fork 4
    }
    
    if (forks_status[left_fork] == 0 && forks_status[right_fork] == 0) {
        forks_status[left_fork] = 1; // Mark as taken
        forks_status[right_fork] = 1;
        printf("Server: Granted forks to Philosopher %d (Forks %d, %d)\n", req.philosopher_rank, left_fork, right_fork);
        MPI_Send(NULL, 0, MPI_INT, req.philosopher_rank, TAG_OK_TO_EAT, MPI_COMM_WORLD);
        return true;
    }
    return false;
}

void server_process(int rounds) {
    int forks_status[NUM_PHILOSOPHERS]; // 0 = available, 1 = taken
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        forks_status[i] = 0;
    }
    
    int philosophers_terminated = 0;
    printf("Table Server is running.\n");
    
    // Server loop: continuously poll for messages and check queue
    while(philosophers_terminated < NUM_PHILOSOPHERS) {
        MPI_Status status;
        int flag;
        ForkRequest incoming_req;

        // Probe for incoming messages without blocking
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

        if (flag) { // Message received
            int sender_rank = status.MPI_SOURCE;
            int tag = status.MPI_TAG;

            if (tag == TAG_GET_FORKS) {
                MPI_Recv(&incoming_req, sizeof(ForkRequest), MPI_BYTE, sender_rank, TAG_GET_FORKS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                printf("Server: Received GET_FORKS request from P%d\n", incoming_req.philosopher_rank);
                
                // Try to grant immediately
                if (!try_grant_forks(forks_status, incoming_req)) {
                    enqueue_request(incoming_req);
                    printf("Server: P%d request queued. Queue size: %d\n", incoming_req.philosopher_rank, queue_size);
                }
            } else if (tag == TAG_REL_FORKS) {
                MPI_Recv(&incoming_req, sizeof(ForkRequest), MPI_BYTE, sender_rank, TAG_REL_FORKS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                printf("Server: Philosopher %d released forks (Forks %d, %d)\n", incoming_req.philosopher_rank, incoming_req.left_fork, incoming_req.right_fork);
                
                // Mark forks as available
                int phil_id = incoming_req.philosopher_rank - 1;
                int left_fork = phil_id;
                int right_fork = (phil_id + 1) % NUM_PHILOSOPHERS;
                if (phil_id == NUM_PHILOSOPHERS - 1) { // Philosopher 4
                    left_fork = (phil_id + 1) % NUM_PHILOSOPHERS; // Fork 0
                    right_fork = phil_id;                          // Fork 4
                }
                forks_status[left_fork] = 0;
                forks_status[right_fork] = 0;

                // After releasing forks, try to grant any pending requests
                // Iterate through the queue and try to grant in FIFO order
                for (int i = 0; i < queue_size; ++i) {
                    ForkRequest current_req = request_queue[queue_head];
                    if (try_grant_forks(forks_status, current_req)) {
                        dequeue_request(); // Remove from queue if granted
                        i--; // Re-check this position since elements shifted
                    } else {
                        // Move current_req to back of queue to maintain order of ungranted
                        dequeue_request();
                        enqueue_request(current_req);
                    }
                }
                
            } else if (tag == TAG_TERMINATE) {
                MPI_Recv(NULL, 0, MPI_INT, sender_rank, TAG_TERMINATE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                printf("Server: Philosopher %d terminated.\n", sender_rank);
                philosophers_terminated++;
            }
        }
        // Small sleep to prevent busy-waiting if no messages
        usleep(100);
    }
    
    printf("Table Server is shutting down.\n");
}

void philosopher_process(int rank, int rounds) {
    int id = rank; // 1-indexed
    int phil_id_0_indexed = rank - 1; // 0-indexed for fork logic

    srand(time(NULL) + rank);

    int left_fork = phil_id_0_indexed; // Fork to philosopher's left
    int right_fork = (phil_id_0_indexed + 1) % NUM_PHILOSOPHERS; // Fork to philosopher's right

    // Asymmetric fork picking for deadlock prevention
    // Philosopher N-1 (ID 4) picks up right fork then left
    if (phil_id_0_indexed == NUM_PHILOSOPHERS - 1) { // Philosopher 4
        left_fork = (phil_id_0_indexed + 1) % NUM_PHILOSOPHERS; // Fork 0
        right_fork = phil_id_0_indexed;                          // Fork 4
    }

    ForkRequest request_msg;
    request_msg.philosopher_rank = id;
    request_msg.left_fork = left_fork;
    request_msg.right_fork = right_fork;

    for (int i = 0; i < rounds; i++) {
        // Think
        printf("Philosopher %d is thinking.\n", id);
        sleep(rand() % 3 + 1);

        // Get hungry, request forks
        printf("Philosopher %d is hungry and requesting forks (%d, %d).\n", id, left_fork, right_fork);
        MPI_Send(&request_msg, sizeof(ForkRequest), MPI_BYTE, SERVER_RANK, TAG_GET_FORKS, MPI_COMM_WORLD);
        
        // Wait for server's permission to eat
        MPI_Recv(NULL, 0, MPI_INT, SERVER_RANK, TAG_OK_TO_EAT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Eat
        printf("Philosopher %d is eating (Forks %d, %d).\n", id, left_fork, right_fork);
        sleep(rand() % 3 + 1);

        // Release forks
        MPI_Send(&request_msg, sizeof(ForkRequest), MPI_BYTE, SERVER_RANK, TAG_REL_FORKS, MPI_COMM_WORLD);
        printf("Philosopher %d finished eating and released forks.\n", id);
    }
    printf("Philosopher %d finished all rounds.\n", id);
    
    // Send termination signal to server
    MPI_Send(NULL, 0, MPI_INT, SERVER_RANK, TAG_TERMINATE, MPI_COMM_WORLD);
}


int main(int argc, char* argv[]) {
    int rank, world_size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size != NUM_PHILOSOPHERS + 1) {
        if (rank == 0) {
            fprintf(stderr, "This application requires exactly %d processes (%d philosophers + 1 server).\n", NUM_PHILOSOPHERS + 1, NUM_PHILOSOPHERS);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    if (argc != 2) {
        if (rank == 0) {
            fprintf(stderr, "Usage: mpiexec -n %d %s <num_rounds>\n", NUM_PHILOSOPHERS + 1, argv[0]);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int rounds = atoi(argv[1]);

    if (rank == SERVER_RANK) {
        server_process(rounds);
    } else {
        philosopher_process(rank, rounds);
    }

    MPI_Finalize();
    return 0;
}
