#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>

#define SHARED 1

sem_t potSem;          // Counting semaphore representing portions of honey in the pot
sem_t bearSem;         // Semaphore to signal the bear when the pot is full
sem_t mutex;           // Mutex semaphore to protect critical sections
int H;                 // Capacity of the pot (number of portions it can hold)
int nHoneybees;        // Number of honeybees
int honeyInPot = 0;    // Current amount of honey in the pot

void *bear(void *arg) {
    while (1) {
        // Wait until the pot is full
        sem_wait(&bearSem);
        printf("Bear: Pot is full! Eating %d portions of honey...\n", H);

        // Critical section: Empty the pot
        sem_wait(&mutex);
        honeyInPot = 0;
        sem_post(&mutex);

        // Simulate eating time
        sleep(2);

        printf("Bear: Finished eating. Going back to sleep.\n");
    }
    return NULL;
}

void *honeybee(void *arg) {
    int id = (intptr_t)arg;

    while (1) {
        // Simulate gathering honey
        sleep((rand() % 2) + 1); // Sleep for 1-2 seconds

        // Critical section: Add honey to the pot
        sem_wait(&mutex);
        if (honeyInPot < H) {
            honeyInPot++;
            printf("Honeybee %d: Added 1 portion of honey. Total honey in pot: %d\n", id, honeyInPot);

            // Signal that a portion of honey has been added
            sem_post(&potSem);

            // If the pot is full, notify the bear
            if (honeyInPot == H) {
                printf("Honeybee %d: Pot is full! Notifying the bear...\n", id);
                sem_post(&bearSem); // Notify the bear
                sem_post(&mutex);   // Release the mutex before skipping the pause
                continue;           // Skip the pause and resume gathering honey
            }
        }
        sem_post(&mutex);

        // Brief pause before next round (unless the pot was just filled)
        usleep(100000); // 100 ms pause
    }
    return NULL;
}

/* Main Function */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number_of_honeybees> <pot_capacity>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    nHoneybees = atoi(argv[1]);
    H = atoi(argv[2]);

    if (nHoneybees <= 0 || H <= 0) {
        fprintf(stderr, "Error: Both number of honeybees and pot capacity must be positive.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize semaphores
    sem_init(&potSem, SHARED, 0);       // Initially, no honey in the pot
    sem_init(&bearSem, SHARED, 0);      // Bear starts waiting for the pot to be full
    sem_init(&mutex, SHARED, 1);        // Binary semaphore for mutual exclusion

    pthread_t bearThread;
    pthread_t honeybeeThreads[nHoneybees];

    // Create the bear thread
    if (pthread_create(&bearThread, NULL, bear, NULL) != 0) {
        perror("Failed to create bear thread");
        exit(EXIT_FAILURE);
    }

    // Create honeybee threads
    for (int i = 0; i < nHoneybees; i++) {
        if (pthread_create(&honeybeeThreads[i], NULL, honeybee, (void *)(intptr_t)(i + 1)) != 0) {
            perror("Failed to create honeybee thread");
            exit(EXIT_FAILURE);
        }
    }

    // In this simulation, threads run forever. For demonstration purposes,
    // you might add a mechanism to end the simulation gracefully.

    // Join all threads (this part is not reached in an infinite simulation)
    pthread_join(bearThread, NULL);
    for (int i = 0; i < nHoneybees; i++) {
        pthread_join(honeybeeThreads[i], NULL);
    }

    // Cleanup semaphores
    sem_destroy(&potSem);
    sem_destroy(&bearSem);
    sem_destroy(&mutex);

    return 0;
}