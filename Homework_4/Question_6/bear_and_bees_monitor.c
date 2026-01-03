/**
 * @file bear_and_bees_monitor.c
 * @brief Simulates the "Bear and Honeybees" problem using a C monitor (Pthreads).
 *
 * This program models a classic producer-consumer synchronization problem:
 * - `n` honeybees (producers) gather honey and add it to a pot.
 * - `1` hungry bear (consumer) eats all honey from the pot when it's full.
 *
 * The `HoneyPot` struct acts as a monitor, encapsulating the shared state (honey portions)
 * and synchronizing access using a mutex and condition variables.
 *
 * The simulation logic is as follows:
 * - The pot is initially empty; its capacity is `H` portions.
 * - Bees call `pot_add_honey()`. They wait if the pot is full (meaning the bear hasn't eaten yet).
 *   The bee who fills the pot signals the bear.
 * - The bear calls `pot_eat_honey()`. It waits until the pot is full. Once full, it eats
 *   all the honey (resets portions to 0) and signals all waiting bees.
 *
 * Fairness: This solution is not strictly fair. When the bear empties the pot
 * and calls `pthread_cond_broadcast()`, the OS and thread scheduler decide which waiting bee
 * gets to run first. However, it is starvation-free as all bees will eventually get a chance.
 *
 * To compile:
 *   gcc -o bear_and_bees_monitor bear_and_bees_monitor.c -lpthread
 *
 * To run:
 *   ./bear_and_bees_monitor <num_bees> <pot_capacity> <num_rounds>
 * Example:
 *   ./bear_and_bees_monitor 10 5 3
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For sleep
#include <time.h>   // For srand, time

// --- Monitor for the HoneyPot ---
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cv_pot_full;  // Bear waits here
    pthread_cond_t cv_pot_empty; // Bees wait here

    int capacity;
    int honey_portions;
} HoneyPot;

/**
 * @brief Initializes the HoneyPot monitor.
 */
void pot_init(HoneyPot* pot, int capacity) {
    pthread_mutex_init(&pot->mtx, NULL);
    pthread_cond_init(&pot->cv_pot_full, NULL);
    pthread_cond_init(&pot->cv_pot_empty, NULL);

    pot->capacity = capacity;
    pot->honey_portions = 0; // Pot is initially empty
}

/**
 * @brief Destroys the HoneyPot monitor resources.
 */
void pot_destroy(HoneyPot* pot) {
    pthread_mutex_destroy(&pot->mtx);
    pthread_cond_destroy(&pot->cv_pot_full);
    pthread_cond_destroy(&pot->cv_pot_empty);
}

/**
 * @brief Called by a honeybee to add one portion of honey to the pot.
 * Bees wait if the pot is full. The bee who fills the pot signals the bear.
 */
void pot_add_honey(HoneyPot* pot, int bee_id) {
    pthread_mutex_lock(&pot->mtx);
    
    // Wait while the pot is full
    while (pot->honey_portions >= pot->capacity) {
        printf("Bee %d finds pot full (%d/%d) and waits.\n", bee_id, pot->honey_portions, pot->capacity);
        pthread_cond_wait(&pot->cv_pot_empty, &pot->mtx);
    }
    
    pot->honey_portions++;
    printf("Bee %d added honey. Pot has %d/%d portions.\n", bee_id, pot->honey_portions, pot->capacity);

    // If I filled the pot, signal the bear
    if (pot->honey_portions == pot->capacity) {
        printf("Bee %d filled the pot! Waking bear.\n", bee_id);
        pthread_cond_signal(&pot->cv_pot_full); // Only signal bear
    }
    pthread_mutex_unlock(&pot->mtx);
}

/**
 * @brief Called by the bear to eat all honey from the pot.
 * The bear waits until the pot is full, then eats all honey and signals bees.
 */
void pot_eat_honey(HoneyPot* pot) {
    pthread_mutex_lock(&pot->mtx);
    
    // Wait until the pot is full
    printf("Bear is sleeping...\n");
    while (pot->honey_portions < pot->capacity) {
        pthread_cond_wait(&pot->cv_pot_full, &pot->mtx);
    }

    printf("Bear woke up! Eating all %d portions...\n", pot->honey_portions);
    // Simulate eating time
    sleep(2); 
    pot->honey_portions = 0; // Reset portions to 0
    printf("Bear finished eating. Pot is empty. Going back to sleep.\n");
    
    // Signal all waiting bees that the pot is now empty
    pthread_cond_broadcast(&pot->cv_pot_empty);
    pthread_mutex_unlock(&pot->mtx);
}

// --- Thread Argument Structures ---
typedef struct {
    int id;
    int num_rounds; // Number of times bee adds honey or bear eats
    HoneyPot* pot;
} CreatureArgs;

// --- Thread Functions ---

void* bee_func(void* arg) {
    CreatureArgs* args = (CreatureArgs*)arg;
    int id = args->id;
    int num_rounds = args->num_rounds;
    HoneyPot* pot = args->pot;

    for (int i = 0; i < num_rounds; ++i) {
        // Gather honey
        sleep((rand() % 2) + 1); // Simulate gathering time
        pot_add_honey(pot, id);
    }
    printf(">> Bee %d finished its rounds.\n", id);
    free(args);
    return NULL;
}

void* bear_func(void* arg) {
    CreatureArgs* args = (CreatureArgs*)arg;
    int num_rounds = args->num_rounds; // Number of times bear eats

    for (int i = 0; i < num_rounds; ++i) {
        pot_eat_honey(args->pot);
        // Simulate digestion time
        sleep((rand() % 3) + 1);
    }
    printf(">> Bear finished its rounds.\n");
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_bees> <pot_capacity> <num_rounds>\n", argv[0]);
        return 1;
    }

    int num_bees = atoi(argv[1]);
    int pot_capacity = atoi(argv[2]);
    int num_rounds = atoi(argv[3]);
    
    if (num_bees <= 0 || pot_capacity <= 0 || num_rounds <= 0) {
        fprintf(stderr, "All arguments must be positive integers.\n");
        return 1;
    }

    HoneyPot pot;
    pot_init(&pot, pot_capacity);

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * (num_bees + 1));
    if (!threads) { perror("Failed to allocate threads"); return 1; }
    int thread_idx = 0;

    srand(time(NULL)); // Seed random number generator

    // Create bear thread
    CreatureArgs* bear_args = (CreatureArgs*)malloc(sizeof(CreatureArgs));
    if (!bear_args) { perror("Failed to allocate bear args"); return 1; }
    bear_args->id = 0; // Bear has ID 0
    bear_args->num_rounds = num_rounds;
    bear_args->pot = &pot;
    pthread_create(&threads[thread_idx++], NULL, bear_func, bear_args);

    // Create bee threads
    for (int i = 0; i < num_bees; ++i) {
        CreatureArgs* args = (CreatureArgs*)malloc(sizeof(CreatureArgs));
        if (!args) { perror("Failed to allocate args"); return 1; }
        args->id = i + 1; // Bee IDs start from 1
        args->num_rounds = num_rounds * pot_capacity; // Each bee adds honey "num_rounds * capacity" times in total
        args->pot = &pot;
        pthread_create(&threads[thread_idx++], NULL, bee_func, args);
    }

    // Wait for all threads to complete
    for (int i = 0; i < (num_bees + 1); ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("Simulation finished.\n");
    pot_destroy(&pot);
    free(threads);

    return 0;
}
