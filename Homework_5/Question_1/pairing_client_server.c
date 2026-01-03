/**
 * @file pairing_client_server.c
 * @brief A distributed client-server application for pairing students using MPI.
 *
 * This program solves the distributed pairing problem with a centralized server.
 * - Process 0 acts as the "teacher" (server).
 * - Processes 1 to n act as "students" (clients).
 *
 * Algorithm:
 * 1. Each student process sends a message containing its rank to the teacher process.
 *    This acts as a "request for a partner".
 * 2. The teacher process waits to receive two such requests.
 * 3. Upon receiving two requests (from student A and student B), the teacher pairs
 *    them up. It sends student B's rank to student A, and student A's rank to student B.
 * 4. This is repeated until all students are paired.
 * 5. If there is an odd number of students, the last remaining student is paired
 *    with themself.
 * 6. Each student, upon receiving their partner's rank, prints the result and terminates.
 *
 * To compile:
 *   mpicc -o pairing_client_server pairing_client_server.c
 *
 * To run (e.g., with 6 students):
 *   mpiexec -n 7 ./pairing_client_server
 *   (Note: -n must be number_of_students + 1 for the teacher)
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define TEACHER_RANK 0

void teacher_process(int num_students) {
    printf("Teacher process started. Waiting for %d students.\n", num_students);
    int pairs_made = 0;
    
    while (pairs_made < num_students / 2) {
        int student1_rank, student2_rank;
        MPI_Status status;

        // Receive the first student's request
        MPI_Recv(&student1_rank, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        printf("Teacher received request from Student %d.\n", student1_rank);

        // Receive the second student's request
        MPI_Recv(&student2_rank, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        printf("Teacher received request from Student %d.\n", student2_rank);

        // Pair them up and send back partner info
        printf("Teacher pairing Student %d with Student %d.\n", student1_rank, student2_rank);
        MPI_Send(&student2_rank, 1, MPI_INT, student1_rank, 0, MPI_COMM_WORLD);
        MPI_Send(&student1_rank, 1, MPI_INT, student2_rank, 0, MPI_COMM_WORLD);

        pairs_made++;
    }

    // Handle the case of an odd number of students
    if (num_students % 2 != 0) {
        int last_student_rank;
        MPI_Status status;
        MPI_Recv(&last_student_rank, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        printf("Teacher received request from the last student: %d.\n", last_student_rank);
        
        // Pair the last student with themself
        printf("Teacher pairing last Student %d with themself.\n", last_student_rank);
        MPI_Send(&last_student_rank, 1, MPI_INT, last_student_rank, 0, MPI_COMM_WORLD);
    }
    
    printf("Teacher process finished.\n");
}

void student_process(int rank) {
    int my_rank = rank;
    int partner_rank;
    MPI_Status status;

    printf("Student %d sending pairing request to teacher.\n", my_rank);

    // Send rank to teacher to request a partner
    MPI_Send(&my_rank, 1, MPI_INT, TEACHER_RANK, 0, MPI_COMM_WORLD);

    // Wait to receive partner's rank from teacher
    MPI_Recv(&partner_rank, 1, MPI_INT, TEACHER_RANK, 0, MPI_COMM_WORLD, &status);
    
    // Print the result
    printf("Student %d is partnered with Student %d.\n", my_rank, partner_rank);
}

int main(int argc, char* argv[]) {
    int rank, world_size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size < 2) {
        fprintf(stderr, "This application requires at least 2 processes (1 teacher, 1 student).\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int num_students = world_size - 1;

    if (rank == TEACHER_RANK) {
        teacher_process(num_students);
    } else {
        student_process(rank);
    }

    MPI_Finalize();
    return 0;
}
