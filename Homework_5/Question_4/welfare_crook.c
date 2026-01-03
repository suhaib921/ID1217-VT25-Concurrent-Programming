/**
 * @file welfare_crook.c
 * @brief Finds common values in three distributed arrays using an MPI pipeline.
 *
 * This program solves the "Welfare Crook" problem, which is equivalent to
 * finding the intersection of three sets of data distributed across three processes.
 * The processes are named F, G, and H (mapped to ranks 0, 1, and 2).
 *
 * The distributed algorithm works as a three-stage pipeline in a ring F->G->H:
 *
 * 1. Stage 1 (F to G):
 *    - Process F sends its local data, one value at a time, to process G.
 *    - Process G receives each value, checks if it exists in its own local data,
 *      and stores the matches (the intersection of F and G) in a temporary list.
 *
 * 2. Stage 2 (G to H):
 *    - After F is done, G sends the values from its temporary list (F ∩ G),
 *      one at a time, to process H.
 *    - Process H receives each value, checks if it exists in its local data,
 *      and stores the matches. This list is the final result (F ∩ G ∩ H).
 *
 * 3. Stage 3 (H to all):
 *    - Process H now holds the definitive list of common values. It broadcasts
 *      these values, one at a time, to all other processes (including itself).
 *    - All processes receive the final values and print them.
 *
 * This approach respects the constraint that messages should contain only one value.
 * Special "end-of-transmission" messages are used to signal the end of each stage.
 *
 * To compile:
 *   mpicc -o welfare_crook welfare_crook.c
 *
 * To run (requires exactly 3 processes):
 *   mpiexec -n 3 ./welfare_crook
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RANK_F 0
#define RANK_G 1
#define RANK_H 2

#define END_OF_TRANSMISSION -1

// Helper function to check if a value is in an array
int contains(int* arr, int size, int val) {
    for (int i = 0; i < size; i++) {
        if (arr[i] == val) return 1;
    }
    return 0;
}

void process_F(int rank) {
    int f_data[] = {1, 5, 9, 12, 15, 20, 88, 99};
    int f_size = sizeof(f_data) / sizeof(int);
    printf("F(%d): My data is {1, 5, 9, 12, 15, 20, 88, 99}\n", rank);

    // Stage 1: Send all my data to G
    for (int i = 0; i < f_size; i++) {
        MPI_Send(&f_data[i], 1, MPI_INT, RANK_G, 0, MPI_COMM_WORLD);
    }
    // Signal end of transmission to G
    int eot = END_OF_TRANSMISSION;
    MPI_Send(&eot, 1, MPI_INT, RANK_G, 0, MPI_COMM_WORLD);
    printf("F(%d): Sent all my data to G.\n", rank);

    // Stage 3: Receive final results from H
    printf("F(%d): Common values are: ", rank);
    fflush(stdout);
    while (1) {
        int common_val;
        MPI_Bcast(&common_val, 1, MPI_INT, RANK_H, MPI_COMM_WORLD);
        if (common_val == END_OF_TRANSMISSION) break;
        printf("%d ", common_val);
    }
    printf("\n");
}

void process_G(int rank) {
    int g_data[] = {2, 5, 10, 12, 18, 20, 99};
    int g_size = sizeof(g_data) / sizeof(int);
    printf("G(%d): My data is {2, 5, 10, 12, 18, 20, 99}\n", rank);
    
    int intersection_fg[100]; // Assume max size
    int intersection_size = 0;

    // Stage 1: Receive data from F and compute intersection
    while (1) {
        int received_val;
        MPI_Recv(&received_val, 1, MPI_INT, RANK_F, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (received_val == END_OF_TRANSMISSION) break;

        if (contains(g_data, g_size, received_val)) {
            intersection_fg[intersection_size++] = received_val;
        }
    }
    printf("G(%d): Intersection with F is complete.\n", rank);

    // Stage 2: Send the intersection (F ∩ G) to H
    for (int i = 0; i < intersection_size; i++) {
        MPI_Send(&intersection_fg[i], 1, MPI_INT, RANK_H, 0, MPI_COMM_WORLD);
    }
    // Signal end of transmission to H
    int eot = END_OF_TRANSMISSION;
    MPI_Send(&eot, 1, MPI_INT, RANK_H, 0, MPI_COMM_WORLD);
    printf("G(%d): Sent intersection data to H.\n", rank);
    
    // Stage 3: Receive final results from H
    printf("G(%d): Common values are: ", rank);
    fflush(stdout);
    while (1) {
        int common_val;
        MPI_Bcast(&common_val, 1, MPI_INT, RANK_H, MPI_COMM_WORLD);
        if (common_val == END_OF_TRANSMISSION) break;
        printf("%d ", common_val);
    }
    printf("\n");
}

void process_H(int rank) {
    int h_data[] = {5, 11, 12, 20, 30, 99};
    int h_size = sizeof(h_data) / sizeof(int);
    printf("H(%d): My data is {5, 11, 12, 20, 30, 99}\n", rank);

    int final_intersection[100]; // Assume max size
    int final_size = 0;

    // Stage 2: Receive data from G and compute final intersection
    while (1) {
        int received_val;
        MPI_Recv(&received_val, 1, MPI_INT, RANK_G, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (received_val == END_OF_TRANSMISSION) break;
        
        if (contains(h_data, h_size, received_val)) {
            final_intersection[final_size++] = received_val;
        }
    }
    printf("H(%d): Final intersection calculation is complete.\n", rank);

    // Stage 3: Broadcast the final results to everyone
    printf("H(%d): Broadcasting final results...\n", rank);
    for (int i = 0; i < final_size; i++) {
        MPI_Bcast(&final_intersection[i], 1, MPI_INT, RANK_H, MPI_COMM_WORLD);
    }
    // Signal end of broadcast
    int eot = END_OF_TRANSMISSION;
    MPI_Bcast(&eot, 1, MPI_INT, RANK_H, MPI_COMM_WORLD);
    
    // H also participates in the Bcast loop to print its own results
    printf("H(%d): Common values are: ", rank);
    for(int i = 0; i < final_size; i++) {
        printf("%d ", final_intersection[i]);
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    int rank, world_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size != 3) {
        if (rank == 0) {
            fprintf(stderr, "This application requires exactly 3 processes.\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (rank == RANK_F) {
        process_F(rank);
    } else if (rank == RANK_G) {
        process_G(rank);
    } else if (rank == RANK_H) {
        process_H(rank);
    }

    MPI_Finalize();
    return 0;
}
