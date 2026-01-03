/**
 * @file hungry_birds.c
 * @brief Simulates the "Hungry Birds" problem using POSIX semaphores.
 *
 * This program models a classic producer-consumer scenario with one producer
 * (the parent bird) and multiple consumers (the baby birds).
 *
 * The simulation logic is as follows:
 * - A dish contains W worms, represented by `current_worms_in_dish` protected by a mutex,
 *   and a counting semaphore `dish_worms_consumer` for baby birds to acquire worms.
 * - Baby birds (threads) repeatedly sleep and then try to eat by calling
 *   `sem_wait()` on the `dish_worms_consumer` semaphore.
 * - After a baby takes a worm, it checks if it took the last one. If so, it signals
 *   the parent bird using `wake_parent` and then waits for the parent to refill the dish
 *   (by waiting on `parent_refilled` semaphore).
 * - The parent bird (thread) waits on `wake_parent`. When woken, it refills the dish
 *   atomically and signals the waiting baby bird (`parent_refilled`) before
 *   releasing the remaining worms (`dish_worms_consumer`).
 *
 * To compile:
 *   gcc -o hungry_birds hungry_birds.c -lpthread
 *
 * To run:
 *   ./hungry_birds <num_babies> <num_worms>
 *   Example: ./hungry_birds 5 10
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// Global variables
int W; // Worms per refill
int current_worms_in_dish; // Track actual worms, protected by mutex

// Semaphores
sem_t dish_worms_consumer;   // Counting semaphore for baby birds to consume worms
sem_t mutex;                 // Binary semaphore to protect current_worms_in_dish
sem_t wake_parent;           // Semaphore to signal parent that dish is empty
sem_t parent_refilled;       // Semaphore for parent to signal it has refilled the dish

/**
 * @brief The thread function for baby birds.
 *
 * Each baby bird thread loops, sleeping for a random time and then attempting
 * to eat a worm from the dish.
 *
 * @param arg A void pointer to the baby bird's ID (long).
 * @return NULL.
 */
void* baby_bird_func(void* arg) {
    long id = (long)arg;

    while (1) {
        // Baby bird is doing other things (playing, sleeping, etc.)
        sleep((rand() % 3) + 1);

        printf("Baby %ld is hungry.\n", id);
        
        // Try to get a worm from the dish. This will block if no worms.
        sem_wait(&dish_worms_consumer);

        // --- Critical Section to safely decrement worm count and check if last worm ---
        sem_wait(&mutex);
        current_worms_in_dish--;
        printf("Baby %ld got a worm. %d worms left.\n", id, current_worms_in_dish);

        // If I took the last worm, I must wake the parent.
        if (current_worms_in_dish == 0) {
            printf("Baby %ld sees the dish is empty and chirps to wake the parent!\n", id);
            sem_post(&wake_parent);     // Signal parent
            sem_post(&mutex);           // Release mutex BEFORE waiting
            sem_wait(&parent_refilled); // Wait for parent to refill
            printf("Baby %ld sees the dish has been refilled.\n", id);
        } else {
            sem_post(&mutex); // Release mutex
        }
        // --- End Critical Section ---
    }
    return NULL;
}

/**
 * @brief The thread function for the parent bird.
 *
 * The parent bird waits until a baby signals that the dish is empty. It then
 * refills the dish with W worms and signals the waiting baby.
 *
 * @param arg Not used.
 * @return NULL.
 */
void* parent_bird_func(void* arg) {
    while (1) {
        // Wait for a baby to signal that the dish is empty
        sem_wait(&wake_parent);

        printf("Parent bird is awake and flying to get worms...\n");
        sleep(2); // Simulate time to get worms

        printf("Parent bird is back and refilling the dish with %d worms.\n", W);

        // Refill the dish. This should be atomic with checking its state.
        sem_wait(&mutex);
        current_worms_in_dish = W;
        sem_post(&mutex);

        // Signal the waiting baby bird that the dish is refilled
        sem_post(&parent_refilled);
        // Add the remaining W-1 worms to the dish_worms_consumer semaphore (one already "given" to the chirping baby)
        for (int i = 0; i < W - 1; i++) { 
            sem_post(&dish_worms_consumer);
        }
        
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_babies> <num_worms>\n", argv[0]);
        return 1;
    }

    int num_babies = atoi(argv[1]);
    W = atoi(argv[2]);

    if (num_babies <= 0 || W <= 0) {
        fprintf(stderr, "Number of babies and worms must be positive.\n");
        return 1;
    }

    srand(time(NULL));
    
    current_worms_in_dish = W; // Initialize actual worm count

    // Initialize semaphores
    sem_init(&dish_worms_consumer, 0, W); // Initial worms for consumption
    sem_init(&mutex, 0, 1);               // Protects current_worms_in_dish
    sem_init(&wake_parent, 0, 0);         // Parent waits for this
    sem_init(&parent_refilled, 0, 0);     // Baby waits for this

    pthread_t parent_thread;
    pthread_t baby_threads[num_babies];

    // Create parent thread
    pthread_create(&parent_thread, NULL, parent_bird_func, NULL);

    // Create baby threads
    for (long i = 0; i < num_babies; i++) {
        pthread_create(&baby_threads[i], NULL, baby_bird_func, (void*)(i + 1)); // IDs from 1
    }
    
    // Join threads (this program runs indefinitely, so this is for cleanup)
    pthread_join(parent_thread, NULL);
    for (int i = 0; i < num_babies; i++) {
        pthread_join(baby_threads[i], NULL);
    }

    // Destroy semaphores
    sem_destroy(&dish_worms_consumer);
    sem_destroy(&mutex);
    sem_destroy(&wake_parent);
    sem_destroy(&parent_refilled);
    
    return 0;
}
