/**
 * @file bear_and_bees.c
 * @brief Simulates the "Bear and Honeybees" problem using POSIX semaphores.
 *
 * This program models a classic synchronization problem with multiple producers
 * (honeybees) and a single consumer (the bear).
 *
 * The simulation logic is as follows:
 * - A honey pot has a capacity of H portions.
 * - Honeybees (threads) repeatedly gather honey and add one portion to the pot.
 * - Access to the pot's portion count is protected by a mutex semaphore.
 * - The honeybee that adds the H-th portion to the pot signals the `pot_full`
 *   semaphore to wake up the bear.
 * - The bear (thread) waits on the `pot_full` semaphore. When woken, it "eats"
 *   all the honey, resetting the portion count to zero.
 * - After eating, the bear signals the `pot_empty` semaphore. This is crucial
 *   for fairness, as it unblocks any bees that arrived while the pot was full, 
 *   allowing them to proceed.
 *
 * To compile:
 *   gcc -o bear_and_bees bear_and_bees.c -lpthread
 *
 * To run:
 *   ./bear_and_bees <num_bees> <pot_capacity>
 *   Example: ./bear_and_bees 10 5
 */
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// Global variables
int H; // Pot capacity
int portions_in_pot = 0;

// Semaphores
sem_t mutex;     // Protects access to portions_in_pot
sem_t pot_full;  // Signals the bear that the pot is full
sem_t pot_empty; // Signals bees that the pot is empty and they can add honey

/**
 * @brief The thread function for honeybees.
 *
 * Each bee thread loops, "gathering" honey and then attempting to add it to
 * the pot. If the pot is full, the bee waits until the bear has eaten.
 *
 * @param arg A void pointer to the honeybee's ID (long).
 * @return NULL.
 */
void* honeybee_func(void* arg) {
    long id = (long)arg;

    while (1) {
        // Bee is gathering one portion of honey
        printf("Bee %ld is gathering honey...\n", id);
        sleep((rand() % 4) + 1);

        // Try to add honey to the pot
        sem_wait(&mutex);

        if (portions_in_pot == H) {
            // Pot is full, wait for the bear to eat
            printf("Bee %ld finds the pot is full and waits.\n", id);
            sem_post(&mutex);
            sem_wait(&pot_empty); // Wait for the 'pot_empty' signal from the bear
            sem_wait(&mutex);     // Re-acquire lock to add honey
        }

        portions_in_pot++;
        printf("Bee %ld adds a portion. Pot now has %d/%d portions.\n", id, portions_in_pot, H);
        
        // If this bee filled the pot, wake up the bear
        if (portions_in_pot == H) {
            printf("Bee %ld filled the pot and wakes up the bear!\n", id);
            sem_post(&pot_full);
        }

        sem_post(&mutex);
    }
    return NULL;
}

/**
 * @brief The thread function for the bear.
 *
 * The bear sleeps until the pot is full, then eats all the honey and goes
 * back to sleep.
 *
 * @param arg Not used.
 * @return NULL.
 */
void* bear_func(void* arg) {
    while (1) {
        printf("Bear is sleeping...\n");
        
        // Wait for a bee to signal that the pot is full
        sem_wait(&pot_full);

        printf("Bear wakes up because the pot is full!\n");
        printf("Bear is eating all the honey...\n");
        sleep(2); // Simulate time to eat

        // The bear has exclusive access now because bees are blocked
        // on the mutex or pot_empty semaphore.
        portions_in_pot = 0;
        printf("Bear has finished eating. The pot is now empty.\n");
        printf("Bear is going back to sleep.\n");

        // Signal ONE waiting bee that the pot is now empty.
        // This bee will then acquire the mutex and add its honey.
        // This prevents a "thundering herd" and ensures fairness.
        sem_post(&pot_empty);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_bees> <pot_capacity>\n", argv[0]);
        return 1;
    }

    int num_bees = atoi(argv[1]);
    H = atoi(argv[2]);

    if (num_bees <= 0 || H <= 0) {
        fprintf(stderr, "Number of bees and pot capacity must be positive.\n");
        return 1;
    }

    srand(time(NULL));

    // Initialize semaphores
    sem_init(&mutex, 0, 1);
    sem_init(&pot_full, 0, 0);
    sem_init(&pot_empty, 0, 0);

    pthread_t bear_thread;
    pthread_t bee_threads[num_bees];

    // Create bear thread
    pthread_create(&bear_thread, NULL, bear_func, NULL);

    // Create bee threads
    for (long i = 0; i < num_bees; i++) {
        pthread_create(&bee_threads[i], NULL, honeybee_func, (void*)i);
    }

    // Join threads
    pthread_join(bear_thread, NULL);
    for (int i = 0; i < num_bees; i++) {
        pthread_join(&bee_threads[i], NULL);
    }

    // Destroy semaphores
    sem_destroy(&mutex);
    sem_destroy(&pot_full);
    sem_destroy(&pot_empty);

    return 0;
}
