/**
 * @file one_lane_bridge.c
 * @brief Simulates the one-lane bridge problem with a fair solution using semaphores.
 *
 * This program models a classic readers-writers problem variant where cars from
 * the north and south want to cross a one-lane bridge. Cars going in the same
 * direction can share the bridge, but opposite directions cannot.
 *
 * This implementation ensures fairness, preventing starvation of cars waiting
 * for one direction while a continuous stream of cars crosses from the other.
 * This is achieved by using a "turnstile" semaphore. Once a car from one
 * direction signals its intent to cross, it passes through the turnstile.
 * The turnstile is then locked, forcing any new arrivals from the opposite
 *
 * direction to wait until the current wave of cars has finished.
 *
 * The solution structure:
 * - `ns_mutex`, `sn_mutex`: Protect counters for northbound and southbound cars.
 * - `bridge`: A semaphore acting as a lock on the bridge for a specific direction.
 * - `turnstile`: A semaphore ensuring that once one direction starts, the
 *   other has to wait for the bridge to be completely clear.
 * - `north_count`, `south_count`: Counters for cars currently on the bridge.
 *
 * To compile:
 *   gcc -o one_lane_bridge one_lane_bridge.c -lpthread
 *
 * To run:
 *   ./one_lane_bridge <num_cars> <num_trips>
 *   Example: ./one_lane_bridge 10 3
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

typedef enum { NORTH, SOUTH } Direction;

// Shared state
int north_count = 0;
int south_count = 0;

// Semaphores
sem_t bridge;       // Controls access to the bridge (binary, 1)
sem_t ns_mutex;     // Mutex for north_count (binary, 1)
sem_t sn_mutex;     // Mutex for south_count (binary, 1)
sem_t turnstile;    // For fairness (binary, 1)

// Car thread arguments
typedef struct {
    long id;
    int trips;
} CarArgs;

/**
 * @brief Simulates a car arriving at and crossing the bridge from the north.
 */
void cross_north(long id) {
    // --- Entry Protocol ---
    sem_wait(&turnstile); // Wait for our turn
    sem_wait(&ns_mutex);
    north_count++;
    if (north_count == 1) {
        // First northbound car, lock the bridge for our direction
        sem_wait(&bridge); 
        printf("Car %ld (North) locked the bridge. North cars on bridge: %d\n", id, north_count);
    }
    sem_post(&ns_mutex);
    sem_post(&turnstile); // Let other cars (from any direction) approach

    // --- Critical Section: On the bridge ---
    printf("-> Car %ld is crossing North. North cars on bridge: %d\n", id, north_count);
    sleep(1); // Simulate crossing time

    // --- Exit Protocol ---
    sem_wait(&ns_mutex);
    north_count--;
    printf("<- Car %ld finished crossing North. North cars on bridge: %d\n", id, north_count);
    if (north_count == 0) {
        // Last northbound car, release the bridge
        printf("Car %ld (North) unlocked the bridge.\n", id);
        sem_post(&bridge);
    }
    sem_post(&ns_mutex);
}

/**
 * @brief Simulates a car arriving at and crossing the bridge from the south.
 */
void cross_south(long id) {
    // --- Entry Protocol ---
    sem_wait(&turnstile);
    sem_wait(&sn_mutex);
    south_count++;
    if (south_count == 1) {
        // First southbound car, lock the bridge for our direction
        sem_wait(&bridge);
        printf("Car %ld (South) locked the bridge. South cars on bridge: %d\n", id, south_count);
    }
    sem_post(&sn_mutex);
    sem_post(&turnstile);

    // --- Critical Section: On the bridge ---
    printf("-> Car %ld is crossing South. South cars on bridge: %d\n", id, south_count);
    sleep(1); // Simulate crossing time

    // --- Exit Protocol ---
    sem_wait(&sn_mutex);
    south_count--;
    printf("<- Car %ld finished crossing South. South cars on bridge: %d\n", id, south_count);
    if (south_count == 0) {
        // Last southbound car, release the bridge
        printf("Car %ld (South) unlocked the bridge.\n", id);
        sem_post(&bridge);
    }
    sem_post(&sn_mutex);
}

/**
 * @brief The main thread function for a car.
 */
void* car_func(void* arg) {
    CarArgs* args = (CarArgs*)arg;
    long id = args->id;
    int trips = args->trips;
    
    // Odd ID starts North, Even ID starts South
    Direction current_direction = (id % 2 != 0) ? NORTH : SOUTH;

    for (int i = 0; i < trips; i++) {
        printf("Car %ld wants to make trip #%d heading %s.\n", id, i + 1, current_direction == NORTH ? "North" : "South");
        
        if (current_direction == NORTH) {
            cross_north(id);
            current_direction = SOUTH; // Switch direction for next trip
        } else {
            cross_south(id);
            current_direction = NORTH; // Switch direction for next trip
        }
        
        // Wait for a while before the next trip
        sleep((rand() % 3) + 1);
    }
    
    printf("Car %ld finished all its trips.\n", id);
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_cars> <num_trips>\n", argv[0]);
        return 1;
    }

    int num_cars = atoi(argv[1]);
    int num_trips = atoi(argv[2]);

    if (num_cars <= 0 || num_trips <= 0) {
        fprintf(stderr, "Number of cars and trips must be positive.\n");
        return 1;
    }

    srand(time(NULL));
    
    // Initialize semaphores
    sem_init(&bridge, 0, 1);
    sem_init(&ns_mutex, 0, 1);
    sem_init(&sn_mutex, 0, 1);
    sem_init(&turnstile, 0, 1);

    pthread_t car_threads[num_cars];

    for (long i = 0; i < num_cars; i++) {
        CarArgs* args = malloc(sizeof(CarArgs));
        args->id = i + 1; // Car IDs start from 1
        args->trips = num_trips;
        pthread_create(&car_threads[i], NULL, car_func, args);
    }

    for (int i = 0; i < num_cars; i++) {
        pthread_join(car_threads[i], NULL);
    }

    printf("All cars have finished their trips. Simulation over.\n");

    // Destroy semaphores
    sem_destroy(&bridge);
    sem_destroy(&ns_mutex);
    sem_destroy(&sn_mutex);
    sem_destroy(&turnstile);
    
    return 0;
}
