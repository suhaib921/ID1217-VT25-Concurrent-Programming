/*
mpicc stable_marriage.c -o stable_marriage
mpirun --hostfile hostfile -np 7 ./stable_marriage
*/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define PROPOSE_TAG 1
#define ACCEPT_TAG 2
#define REJECT_TAG 3
#define COUNTER_TAG 4
#define TERMINATE_TAG 5

void shuffle(int *array, size_t n) {
    if (n > 1) {
        for (size_t i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Ensure the number of processes is valid
    if ((size - 1) % 2 != 0 || size < 3) {
        if (rank == 0) {
            printf("Error: Number of processes must be odd and at least 3.\n");
        }
        MPI_Finalize();
        return 1;
    }

    int n = (size - 1) / 2; // Number of men and women
    bool terminate = false;

    if (rank == 2 * n) { // Counter process
        int count = 0;
        while (count < n) {
            MPI_Status status;
            int msg;
            MPI_Recv(&msg, 1, MPI_INT, MPI_ANY_SOURCE, COUNTER_TAG, MPI_COMM_WORLD, &status);
            count++;
            printf("Counter: %d women have accepted\n", count);
        }
        printf("Counter: All women have accepted. Terminating...\n");

        // Broadcast termination signal
        int terminate_signal = 1;
        MPI_Bcast(&terminate_signal, 1, MPI_INT, 2 * n, MPI_COMM_WORLD);

        // Ensure all processes synchronize before exiting
        MPI_Barrier(MPI_COMM_WORLD);
    } else if (rank < 2 * n) {
        if (rank < n) { // Man process
            srand(time(NULL) + rank);
            int *preferences = malloc(n * sizeof(int));
            for (int i = 0; i < n; i++) preferences[i] = n + i;
            shuffle(preferences, n);

            int engaged_to = -1;
            int current_proposal = 0;

            while (!terminate && current_proposal < n) {
                if (engaged_to == -1) {
                    int woman = preferences[current_proposal];
                    printf("Man %d proposes to Woman %d\n", rank, woman - n);
                    MPI_Send(&rank, 1, MPI_INT, woman, PROPOSE_TAG, MPI_COMM_WORLD);

                    MPI_Status status;
                    int response;
                    MPI_Recv(&response, 1, MPI_INT, woman, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                    if (status.MPI_TAG == ACCEPT_TAG) {
                        engaged_to = woman;
                        printf("Man %d engaged to Woman %d\n", rank, woman - n);
                    } else {
                        current_proposal++;
                    }
                }

                // Check for termination signal
                MPI_Status status;
                int flag = 0;
                MPI_Iprobe(2 * n, TERMINATE_TAG, MPI_COMM_WORLD, &flag, &status);
                if (flag) {
                    MPI_Recv(&terminate, 1, MPI_C_BOOL, 2 * n, TERMINATE_TAG, MPI_COMM_WORLD, &status);
                    break; // Stop proposing once termination is signaled
                }
            }

            free(preferences);
        } else { // Woman process
            srand(time(NULL) + rank);
            int *preferences = malloc(n * sizeof(int));
            for (int i = 0; i < n; i++) preferences[i] = i;
            shuffle(preferences, n);

            int current_man = -1;
            int current_man_rank = n;
            bool has_accepted = false;

            while (!terminate) {
                MPI_Status status;
                int proposer;
                MPI_Recv(&proposer, 1, MPI_INT, MPI_ANY_SOURCE, PROPOSE_TAG, MPI_COMM_WORLD, &status);

                int proposer_rank = -1;
                for (int i = 0; i < n; i++) {
                    if (preferences[i] == proposer) {
                        proposer_rank = i;
                        break;
                    }
                }

                if (current_man == -1) {
                    current_man = proposer;
                    current_man_rank = proposer_rank;
                    MPI_Send(&current_man, 1, MPI_INT, proposer, ACCEPT_TAG, MPI_COMM_WORLD);
                    printf("Woman %d accepts proposal from Man %d\n", rank - n, proposer);

                    if (!has_accepted) {
                        has_accepted = true;
                        MPI_Send(&rank, 1, MPI_INT, 2 * n, COUNTER_TAG, MPI_COMM_WORLD);
                    }
                } else {
                    if (proposer_rank < current_man_rank) {
                        MPI_Send(&current_man, 1, MPI_INT, current_man, REJECT_TAG, MPI_COMM_WORLD);
                        current_man = proposer;
                        current_man_rank = proposer_rank;
                        MPI_Send(&current_man, 1, MPI_INT, proposer, ACCEPT_TAG, MPI_COMM_WORLD);
                        printf("Woman %d dumps Man %d for Man %d\n", rank - n, current_man, proposer);
                    } else {
                        MPI_Send(&proposer, 1, MPI_INT, proposer, REJECT_TAG, MPI_COMM_WORLD);
                        printf("Woman %d rejects Man %d\n", rank - n, proposer);
                    }
                }

                // Check for termination signal
                MPI_Status term_status;
                int term_flag = 0;
                MPI_Iprobe(2 * n, TERMINATE_TAG, MPI_COMM_WORLD, &term_flag, &term_status);
                if (term_flag) {
                    MPI_Recv(&terminate, 1, MPI_C_BOOL, 2 * n, TERMINATE_TAG, MPI_COMM_WORLD, &term_status);
                    break; // Stop processing proposals once termination is signaled
                }
            }

            free(preferences);
        }
    }

    MPI_Finalize();
    return 0;
}