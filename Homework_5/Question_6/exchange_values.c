/**
 * @file exchange_values.c
 * @brief Implements and compares three different distributed value exchange algorithms using MPI.
 *
 * This program implements three distinct algorithms for a set of processes to exchange
 * integer values over a series of rounds. The performance of each algorithm is measured.
 * The three algorithms are:
 *
 * 1. Centralized Gather/Broadcast:
 *    - All processes send their value to a root process (Rank 0) using MPI_Gather.
 *    - The root process then broadcasts the complete array of values back to all
 *      processes using MPI_Bcast.
 *    - This is simple but can create a bottleneck at the root.
 *
 * 2. Ring Shift (Bucket Brigade):
 *    - Processes are arranged in a logical ring.
 *    - In each of `P-1` steps, every process sends a value to its right neighbor
 *      and receives a value from its left neighbor.
 *    - After `P-1` steps, every process has received every other process's original value.
 *
 * 3. Point-to-Point All-to-All:
 *    - Every process sends its value directly to every other process using a loop
 *      of `P-1` non-blocking sends (MPI_Isend).
 *    - Every process also posts `P-1` non-blocking receives (MPI_Irecv).
 *    - MPI_Waitall is used to ensure all communications are complete.
 *    - This avoids a central bottleneck but increases the total number of messages.
 *
 * The program runs each algorithm for a specified number of rounds and prints the
 * total execution time.
 *
 * To compile:
 *   mpicc -o exchange_values exchange_values.c
 *
 * To run (e.g., with 4 processes and 10 rounds):
 *   mpiexec -n 4 ./exchange_values 10
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ROOT_RANK 0

// --- Algorithm 1: Centralized Gather/Broadcast ---
void exchange_centralized(int rank, int world_size, int* values) {
    int* gathered_values = NULL;
    if (rank == ROOT_RANK) {
        gathered_values = (int*)malloc(world_size * sizeof(int));
    }
    // All processes send their value to the root
    MPI_Gather(&values[rank], 1, MPI_INT, gathered_values, 1, MPI_INT, ROOT_RANK, MPI_COMM_WORLD);
    
    // Root broadcasts the full array back to everyone
    if (rank == ROOT_RANK) {
        // The root's gathered_values is the final array, copy it to values
        for(int i=0; i<world_size; ++i) values[i] = gathered_values[i];
    }
    MPI_Bcast(values, world_size, MPI_INT, ROOT_RANK, MPI_COMM_WORLD);

    if (rank == ROOT_RANK) {
        free(gathered_values);
    }
}

// --- Algorithm 2: Ring Shift ---
void exchange_ring(int rank, int world_size, int* values) {
    int right_neighbor = (rank + 1) % world_size;
    int left_neighbor = (rank - 1 + world_size) % world_size;
    
    int temp_recv_val;
    int val_to_send = values[rank];

    for (int i = 0; i < world_size - 1; ++i) {
        MPI_Sendrecv(&val_to_send, 1, MPI_INT, right_neighbor, 0,
                     &temp_recv_val, 1, MPI_INT, left_neighbor, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // The value received is from the process that originally owned it.
        // Its rank is (rank - i - 1 + world_size) % world_size
        int original_owner_rank = (rank - (i + 1) + world_size) % world_size;
        values[original_owner_rank] = temp_recv_val;
        val_to_send = temp_recv_val; // Pass the received value on
    }
}


// --- Algorithm 3: Point-to-Point All-to-All ---
void exchange_p2p_all_to_all(int rank, int world_size, int* values) {
    MPI_Request send_reqs[world_size];
    MPI_Request recv_reqs[world_size];
    
    // Post all sends and receives
    for (int i = 0; i < world_size; ++i) {
        if (i != rank) {
            MPI_Isend(&values[rank], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &send_reqs[i]);
            MPI_Irecv(&values[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &recv_reqs[i]);
        } else {
            // Mark self-communication as already done
            send_reqs[i] = MPI_REQUEST_NULL;
            recv_reqs[i] = MPI_REQUEST_NULL;
        }
    }
    
    MPI_Status statuses[world_size];
    MPI_Waitall(world_size, recv_reqs, statuses);
    MPI_Waitall(world_size, send_reqs, statuses);
}


int main(int argc, char* argv[]) {
    int rank, world_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (argc != 2) {
        if (rank == 0) {
            fprintf(stderr, "Usage: %s <num_rounds>\n", argv[0]);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    int num_rounds = atoi(argv[1]);

    double start_time, end_time;

    // --- Run Algorithm 1 ---
    int* values1 = (int*)malloc(world_size * sizeof(int));
    values1[rank] = rank * 10; // Initial value
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        printf("\n--- Testing Algorithm 1 (Centralized Gather/Broadcast) for %d rounds ---\\n", num_rounds);
        start_time = MPI_Wtime();
    }
    for (int i = 0; i < num_rounds; ++i) {
        exchange_centralized(rank, world_size, values1);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        end_time = MPI_Wtime();
        printf("Total execution time: %f seconds\n", end_time - start_time);
    }
    free(values1);

    // --- Run Algorithm 2 ---
    int* values2 = (int*)malloc(world_size * sizeof(int));
    values2[rank] = rank * 10;
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        printf("\n--- Testing Algorithm 2 (Ring Shift) for %d rounds ---\\n", num_rounds);
        start_time = MPI_Wtime();
    }
    for (int i = 0; i < num_rounds; ++i) {
        exchange_ring(rank, world_size, values2);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        end_time = MPI_Wtime();
        printf("Total execution time: %f seconds\n", end_time - start_time);
    }
    free(values2);
    
    // --- Run Algorithm 3 ---
    int* values3 = (int*)malloc(world_size * sizeof(int));
    values3[rank] = rank * 10;
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        printf("\n--- Testing Algorithm 3 (Point-to-Point All-to-All) for %d rounds ---\\n", num_rounds);
        start_time = MPI_Wtime();
    }
    for (int i = 0; i < num_rounds; ++i) {
        exchange_p2p_all_to_all(rank, world_size, values3);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        end_time = MPI_Wtime();
        printf("Total execution time: %f seconds\n", end_time - start_time);
    }
    free(values3);

    MPI_Finalize();
    return 0;
}
