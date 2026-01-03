/**
 * @file hungry_birds_monitor.c
 * @brief Simulates the "Hungry Birds" problem using a C monitor (Pthreads).
 *
 * This program models a classic producer-consumer scenario with one producer
 * (the parent bird) and multiple consumers (the baby birds), using a monitor
 * for synchronization instead of semaphores.
 *
 * The simulation logic is as follows:
 * - A `Dish` struct acts as a monitor, encapsulating the shared state: the number of worms.
 * - Baby birds (threads) call the `dish_eat_worm()` function on the monitor.
 *   - Inside `dish_eat_worm()`, a baby waits on a condition variable (`cv_worm_available`)
 *     until the dish is not empty.
 *   - After taking a worm, the baby who takes the last one notifies the parent
 *     via another condition variable (`cv_dish_empty`).
 * - The parent bird (thread) calls the `dish_refill_dish()` function.
 *   - Inside `dish_refill_dish()`, it waits on `cv_dish_empty`.
 *   - When woken, it refills the dish and notifies all waiting baby birds.
 *
 * Fairness: This solution is not strictly fair. When the parent refills the dish
 * and calls `pthread_cond_broadcast()`, the OS and thread scheduler decide which waiting baby
 * gets to run first. It's possible for some babies to get worms multiple times
 * before another gets one, but it is starvation-free as all waiting babies will
 * eventually get a chance to run and eat.
 *
 * To compile:
 *   gcc -o hungry_birds_monitor hungry_birds_monitor.c -lpthread
 *
 * To run:
 *   ./hungry_birds_monitor <num_babies> <num_worms> <num_rounds>
 * Example:
 *   ./hungry_birds_monitor 5 10 3
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For sleep
#include <time.h>   // For srand, time

// --- Monitor for the Dish ---
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cv_worm_available; // Babies wait on this
    pthread_cond_t cv_dish_empty;     // Parent waits on this

    int capacity;
    int worms;
} Dish;

/**
 * @brief Initializes the Dish monitor.
 */
void dish_init(Dish* dish, int initial_worms) {
    pthread_mutex_init(&dish->mtx, NULL);
    pthread_cond_init(&dish->cv_worm_available, NULL);
    pthread_cond_init(&dish->cv_dish_empty, NULL);

    dish->capacity = initial_worms;
    dish->worms = initial_worms;
}

/**
 * @brief Destroys the Dish monitor resources.
 */
void dish_destroy(Dish* dish) {
    pthread_mutex_destroy(&dish->mtx);
    pthread_cond_destroy(&dish->cv_worm_available);
    pthread_cond_destroy(&dish->cv_dish_empty);
}

/**
 * @brief Called by a baby bird to eat a worm.
 * Waits if the dish is empty. If it takes the last worm, it signals the parent.
 */
void dish_eat_worm(Dish* dish, int id) {
    pthread_mutex_lock(&dish->mtx);
    
    // Wait while the dish is empty
    while (dish->worms <= 0) {
        pthread_cond_wait(&dish->cv_worm_available, &dish->mtx);
    }
    
    dish->worms--;
    printf("Baby %d ate a worm. %d worms left.\n", id, dish->worms);

    // If I took the last worm, wake up the parent
    if (dish->worms == 0) {
        printf("Baby %d sees the dish is empty and chirps!\n", id);
        pthread_cond_signal(&dish->cv_dish_empty);
    }
    pthread_mutex_unlock(&dish->mtx);
}

/**
 * @brief Called by the parent bird to refill the dish.
 * Waits until signaled that the dish is empty, then refills and notifies babies.
 */
void dish_refill_dish(Dish* dish) {
    pthread_mutex_lock(&dish->mtx);
    
    // Wait until a baby signals the dish is empty
    while (dish->worms > 0) {
        pthread_cond_wait(&dish->cv_dish_empty, &dish->mtx);
    }

    printf("Parent is awake and refilling the dish with %d worms.\n", dish->capacity);
    dish->worms = dish->capacity;
    
    // Dish is refilled, notify all waiting baby birds
    pthread_cond_broadcast(&dish->cv_worm_available);
    pthread_mutex_unlock(&dish->mtx);
}

// --- Thread Argument Structures ---
typedef struct {
    int id;
    int rounds;
    Dish* dish;
} BirdArgs;

// --- Thread Functions ---

void* baby_bird_func(void* arg) {
    BirdArgs* args = (BirdArgs*)arg;
    int id = args->id;
    int rounds = args->rounds;
    Dish* dish = args->dish;

    for (int i = 0; i < rounds; ++i) {
        // Play for a bit
        sleep((rand() % 3) + 1);
        
        printf("Baby %d is hungry.\n", id);
        dish_eat_worm(dish, id);
    }
    printf(">> Baby %d is full and finished.\n", id);
    free(args);
    return NULL;
}

void* parent_bird_func(void* arg) {
    BirdArgs* args = (BirdArgs*)arg;
    int num_babies = args->id; // Using id field to pass num_babies
    int rounds = args->rounds;
    Dish* dish = args->dish;

    // The parent needs to refill enough times for all babies to finish
    int total_eats = num_babies * rounds;
    int refills_needed = (total_eats + (dish->capacity-1)) / dish->capacity;
    
    // We run one fewer refill loop because the initial state has worms.
    for (int i = 0; i < refills_needed -1; ++i) { // -1 because parent is waiting for dish to become empty AFTER initial state
        dish_refill_dish(dish);
    }
    printf(">> Parent has finished its duties.\n");
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_babies> <num_worms> <num_rounds>\n", argv[0]);
        return 1;
    }

    int num_babies = atoi(argv[1]);
    int num_worms = atoi(argv[2]);
    int num_rounds = atoi(argv[3]);
    
    if (num_babies <= 0 || num_worms <= 0 || num_rounds <= 0) {
        fprintf(stderr, "All arguments must be positive integers.\n");
        return 1;
    }

    Dish dish;
    dish_init(&dish, num_worms);

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * (num_babies + 1));
    if (!threads) { perror("Failed to allocate threads"); return 1; }
    int thread_idx = 0;

    srand(time(NULL)); // Seed random number generator

    // Parent thread args
    BirdArgs* parent_args = (BirdArgs*)malloc(sizeof(BirdArgs));
    if (!parent_args) { perror("Failed to allocate parent args"); return 1; }
    parent_args->id = num_babies; // Using id field to pass num_babies count
    parent_args->rounds = num_rounds;
    parent_args->dish = &dish;
    pthread_create(&threads[thread_idx++], NULL, parent_bird_func, parent_args);


    for (int i = 0; i < num_babies; ++i) {
        BirdArgs* args = (BirdArgs*)malloc(sizeof(BirdArgs));
        if (!args) { perror("Failed to allocate args"); return 1; }
        args->id = i + 1;
        args->rounds = num_rounds;
        args->dish = &dish;
        pthread_create(&threads[thread_idx++], NULL, baby_bird_func, args);
    }

    for (int i = 0; i < (num_babies + 1); ++i) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Simulation finished.\n");
    dish_destroy(&dish);
    free(threads);

    return 0;
}
