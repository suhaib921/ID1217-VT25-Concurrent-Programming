/**
 * @file pairing_p2p.c
 * @brief A peer-to-peer distributed application for pairing students using MPI.
 *
 * This program solves the distributed pairing problem using a peer-to-peer
 * algorithm based on a token ring.
 *
 * Algorithm:
 * 1. Process 0 (Teacher) initializes the process. It creates a list of all
 *    student ranks (1 to n). It randomly selects a starting student and sends
 *    them the "token", which is the list of unpaired students.
 *
 * 2. A student process, upon receiving the token (an array of ranks):
 *    a. If the list contains only one student (themselves), they are the last
 *       one and partner with themself. The algorithm terminates.
 *    b. The student removes their own rank from the list.
 *    c. They randomly select a partner from the remaining students in the list.
 *    d. They send a "pairing" message to their chosen partner, informing them of the pair.
 *    e. They remove the partner's rank from the list.
 *    f. If the list is not empty, they randomly select another student from the
 *       list to be the next token holder. They send the reduced list (the token)
 *       to that student.
 *
 * 3. A student process may also receive a "pairing" message, at which point they
 *    learn who their partner is and wait for termination.
 *
 * To compile:
 *   mpicc -o pairing_p2p pairing_p2p.c
 *
 * To run (e.g., with 6 students):
 *   mpiexec -n 7 ./pairing_p2p
 *   (Note: -n must be number_of_students + 1 for the teacher)
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TEACHER_RANK 0
#define TAG_TOKEN 1
#define TAG_PAIRING 2

// Function to remove an element from an array
void remove_from_array(int* array, int* size, int element) {
    int i, found_index = -1;
    for (i = 0; i < *size; i++) {
        if (array[i] == element) {
            found_index = i;
            break;
        }
    }
    if (found_index != -1) {
        for (i = found_index; i < *size - 1; i++) {
            array[i] = array[i + 1];
        }
        (*size)--;
    }
}

void student_process(int rank, int num_students) {
    int partner_rank = -1;
    MPI_Status status;
    
    printf("Student %d is waiting.\n", rank);
    
    // Students wait to receive either the token or a pairing message
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    // --- Case 1: I received the token, so it's my turn to pick ---
    if (status.MPI_TAG == TAG_TOKEN) {
        int token_size;
        MPI_Get_count(&status, MPI_INT, &token_size);
        int* token = (int*)malloc(token_size * sizeof(int));
        MPI_Recv(token, token_size, MPI_INT, status.MPI_SOURCE, TAG_TOKEN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("Student %d received the token from %d.\n", rank, status.MPI_SOURCE);

        // If I'm the last one in the token, I pair with myself
        if (token_size == 1 && token[0] == rank) {
            partner_rank = rank;
        } else {
            // Remove myself from the list of candidates
            remove_from_array(token, &token_size, rank);
            
            // Choose a random partner from the remaining list
            int partner_index = rand() % token_size;
            partner_rank = token[partner_index];
            
            // Inform my partner
            printf("Student %d is pairing with Student %d.\n", rank, partner_rank);
            MPI_Send(&rank, 1, MPI_INT, partner_rank, TAG_PAIRING, MPI_COMM_WORLD);

            // Remove partner from the token
            remove_from_array(token, &token_size, partner_rank);
            
            // If there are still unpaired students, pass the token on
            if (token_size > 0) {
                int next_student_rank = token[rand() % token_size];
                printf("Student %d is passing token to Student %d.\n", rank, next_student_rank);
                MPI_Send(token, token_size, MPI_INT, next_student_rank, TAG_TOKEN, MPI_COMM_WORLD);
            }
        }
        free(token);
    }
    // --- Case 2: Someone else paired with me ---
    else if (status.MPI_TAG == TAG_PAIRING) {
        MPI_Recv(&partner_rank, 1, MPI_INT, status.MPI_SOURCE, TAG_PAIRING, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Student %d was paired by Student %d.\n", rank, partner_rank);
    }

    // A barrier to wait for all processes to finish before printing final results
    MPI_Barrier(MPI_COMM_WORLD);
    if(rank != TEACHER_RANK) {
        printf("FINAL: Student %d is partnered with Student %d.\n", rank, partner_rank);
    }
}

void teacher_process(int num_students) {
    printf("Teacher process started.\n");
    
    int* students = (int*)malloc(num_students * sizeof(int));
    for (int i = 0; i < num_students; i++) {
        students[i] = i + 1; // Ranks 1 to num_students
    }
    
    // Pick a random student to start
    int start_student_rank = students[rand() % num_students];
    
    printf("Teacher chose Student %d to start the pairing.\n", start_student_rank);
    
    // Send the initial token (list of all students)
    MPI_Send(students, num_students, MPI_INT, start_student_rank, TAG_TOKEN, MPI_COMM_WORLD);
    
    free(students);
    
    // Barrier to wait for students to finish
    MPI_Barrier(MPI_COMM_WORLD);
    printf("Teacher process finished.\n");
}


int main(int argc, char* argv[]) {
    int rank, world_size;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    srand(time(NULL) + rank); // Seed random number generator 

    if (world_size < 2) {
        fprintf(stderr, "This application requires at least 2 processes.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int num_students = world_size - 1;

    if (rank == TEACHER_RANK) {
        teacher_process(num_students);
    } else {
        student_process(rank, num_students);
    }

    MPI_Finalize();
    return 0;
}
