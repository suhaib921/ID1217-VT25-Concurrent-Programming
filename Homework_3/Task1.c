#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>

#define SHARED 1

/* Global Variables */
sem_t dishSem;       // Counting semaphore representing worms in the dish
sem_t parentSem;     // Semaphore to signal the parent when the dish is empty
sem_t mutex;         // Mutex semaphore to protect the parent notification flag
sem_t refillSem;
int W;               // Number of worms to refill the dish
int nBabyBirds;      // Number of baby birds
int parentNotified = 0; // Flag to ensure only one baby bird notifies the parent

/* Parent Bird Function */
void *parentBird(void *arg) {
    while (1) {
        // Wait for notification from baby birds that the dish is empty
        sem_wait(&parentSem);
        printf("Parent bird: Dish empty! Refilling with %d worms...\n", W);

        // Simulate time taken to gather worms
        sleep(1);

        for (int i = 0; i < W; i++) sem_post(&dishSem); // Refill dish
        for (int i = 0; i < nBabyBirds; i++) sem_post(&refillSem); // Wake all babies

        // Reset the notification flag
        sem_wait(&mutex);
        parentNotified = 0;
        sem_post(&mutex);

        printf("Parent bird: Dish refilled with %d worms.\n", W);
    }
    return NULL;
}

/* Baby Bird Function */
void *babyBird(void *arg) {
    int id = (intptr_t)arg;
    while (1) {
        sem_wait(&dishSem); // Block until worm available
        int wormsLeft;
        sem_getvalue(&dishSem, &wormsLeft);
        printf("Baby %d: Took worm. Left: %d\n", id, wormsLeft);
        sleep(1); // Simulate eating

        // Check if dish is empty and notify parent
        if (wormsLeft == 0) {
            sem_wait(&mutex);
            if (!parentNotified) {
                parentNotified = 1;
                sem_post(&parentSem);
            }
            sem_post(&mutex);
            sem_wait(&refillSem); // Block until parent refills
        }
    }
    return NULL;
}

/* Main Function */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number_of_baby_birds> <initial_worms>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    nBabyBirds = atoi(argv[1]);
    W = atoi(argv[2]);

    // Initialize semaphores
    sem_init(&dishSem, SHARED, W);       // Start with W worms in the dish
    sem_init(&parentSem, SHARED, 0);     // Parent starts waiting for notification
    sem_init(&mutex, SHARED, 1);         // Binary semaphore for mutual exclusion
    sem_init(&refillSem, SHARED, 0);

    pthread_t parentThread;
    pthread_t babyThreads[nBabyBirds];

    // Create the parent bird thread
    if (pthread_create(&parentThread, NULL, parentBird, NULL) != 0) {
        perror("Failed to create parent bird thread");
        exit(EXIT_FAILURE);
    }

    // Create baby bird threads
    for (int i = 0; i < nBabyBirds; i++) {
        if (pthread_create(&babyThreads[i], NULL, babyBird, (void *)(intptr_t)(i + 1)) != 0) {
            perror("Failed to create baby bird thread");
            exit(EXIT_FAILURE);
        }
    }

    // In this simulation, threads run forever. For demonstration purposes,
    // you might add a mechanism to end the simulation gracefully.

    // Join all threads (this part is not reached in an infinite simulation)
    pthread_join(parentThread, NULL);
    for (int i = 0; i < nBabyBirds; i++) {
        pthread_join(babyThreads[i], NULL);
    }

    // Cleanup semaphores
    sem_destroy(&dishSem);
    sem_destroy(&parentSem);
    sem_destroy(&mutex);

    return 0;
}