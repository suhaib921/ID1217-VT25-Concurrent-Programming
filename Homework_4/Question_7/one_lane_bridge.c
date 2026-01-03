#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For usleep
#include <time.h>   // For srand, time
#include <sys/time.h> // For gettimeofday

// Enum for direction
typedef enum { NORTH, SOUTH, NONE } Direction;

// Monitor for the Bridge
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t north_queue; // Northbound cars wait here
    pthread_cond_t south_queue; // Southbound cars wait here

    int cars_on_bridge;
    Direction current_direction;
    int north_waiting;
    int south_waiting;
    Direction last_direction; // To implement fairness: remembers which direction last crossed
} Bridge;

// Function to get current timestamp in milliseconds
long long get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + (long long)tv.tv_usec / 1000;
}

// Function to initialize the Bridge monitor
void bridge_init(Bridge* bridge) {
    pthread_mutex_init(&bridge->mtx, NULL);
    pthread_cond_init(&bridge->north_queue, NULL);
    pthread_cond_init(&bridge->south_queue, NULL);

    bridge->cars_on_bridge = 0;
    bridge->current_direction = NONE;
    bridge->north_waiting = 0;
    bridge->south_waiting = 0;
    bridge->last_direction = NONE; // No direction has crossed yet
    printf("[%lld] Bridge initialized.\n", get_timestamp_ms());
}

// Function to destroy bridge resources
void bridge_destroy(Bridge* bridge) {
    pthread_mutex_destroy(&bridge->mtx);
    pthread_cond_destroy(&bridge->north_queue);
    pthread_cond_destroy(&bridge->south_queue);
}

// Helper function to check if a car can enter the bridge based on fairness
bool can_enter(Bridge* bridge, Direction dir) {
    // If bridge is occupied by cars going the opposite direction, cannot enter
    if (bridge->cars_on_bridge > 0 && bridge->current_direction != dir) {
        return false;
    }

    // If bridge is empty
    if (bridge->cars_on_bridge == 0) {
        // Apply fairness policy:
        // If there are cars waiting in the opposite direction, and the *last* direction
        // that crossed was the same as the current car's direction, then let the opposite
        // direction take priority.
        // Or if no one is waiting in the opposite direction.
        if (dir == NORTH) {
            // North car wants to enter. If South cars are waiting AND the last direction was North,
            // then North should wait to give South a turn.
            return bridge->south_waiting == 0 || bridge->last_direction == SOUTH;
        } else { // dir == SOUTH
            // South car wants to enter. If North cars are waiting AND the last direction was South,
            // then South should wait to give North a turn.
            return bridge->north_waiting == 0 || bridge->last_direction == NORTH;
        }
    }

    // If cars are already on the bridge and going the same direction, new car can enter.
    return true;
}

// Car requests to enter the bridge
void bridge_enter(Bridge* bridge, int car_id, Direction dir) {
    pthread_mutex_lock(&bridge->mtx);

    if (dir == NORTH) {
        bridge->north_waiting++;
    } else {
        bridge->south_waiting++;
    }

    printf("[%lld] Car-%d (%s) wants to cross. Waiting: N=%d, S=%d. On bridge: %d (%s).\n",
           get_timestamp_ms(), car_id, (dir == NORTH ? "NORTH" : "SOUTH"),
           bridge->north_waiting, bridge->south_waiting, bridge->cars_on_bridge,
           (bridge->current_direction == NORTH ? "NORTH" : (bridge->current_direction == SOUTH ? "SOUTH" : "NONE")));

    // Wait until conditions are met to enter
    while (!can_enter(bridge, dir)) {
        if (dir == NORTH) {
            pthread_cond_wait(&bridge->north_queue, &bridge->mtx);
        } else {
            pthread_cond_wait(&bridge->south_queue, &bridge->mtx);
        }
    }

    if (dir == NORTH) {
        bridge->north_waiting--;
    } else {
        bridge->south_waiting--;
    }

    if (bridge->cars_on_bridge == 0) { // First car in this direction
        bridge->current_direction = dir;
    }
    bridge->cars_on_bridge++;

    printf("[%lld] Car-%d (%s) entering bridge. Cars on bridge: %d (%s). Waiting: N=%d, S=%d.\n",
           get_timestamp_ms(), car_id, (dir == NORTH ? "NORTH" : "SOUTH"),
           bridge->cars_on_bridge, (bridge->current_direction == NORTH ? "NORTH" : "SOUTH"),
           bridge->north_waiting, bridge->south_waiting);

    pthread_mutex_unlock(&bridge->mtx);
}

// Car exits the bridge
void bridge_exit(Bridge* bridge, int car_id, Direction dir) {
    pthread_mutex_lock(&bridge->mtx);

    bridge->cars_on_bridge--;
    printf("[%lld] Car-%d (%s) exiting bridge. Cars on bridge: %d.\n",
           get_timestamp_ms(), car_id, (dir == NORTH ? "NORTH" : "SOUTH"), bridge->cars_on_bridge);

    if (bridge->cars_on_bridge == 0) { // Last car left the bridge
        bridge->current_direction = NONE;
        bridge->last_direction = dir; // Remember which direction last crossed for fairness

        printf("[%lld] Bridge is clear. Last direction was %s. Signaling waiting cars.\n",
               get_timestamp_ms(), (dir == NORTH ? "NORTH" : "SOUTH"));
        
        // Prioritize the opposite direction if cars are waiting
        if (dir == NORTH) {
            if (bridge->south_waiting > 0) {
                pthread_cond_broadcast(&bridge->south_queue);
            } else {
                pthread_cond_broadcast(&bridge->north_queue); // If no south, let north proceed
            }
        } else { // dir == SOUTH
            if (bridge->north_waiting > 0) {
                pthread_cond_broadcast(&bridge->north_queue);
            } else {
                pthread_cond_broadcast(&bridge->south_queue); // If no north, let south proceed
            }
        }
    } else {
        // If not the last car, signal others going the same direction that might be waiting
        // (e.g., if a fairness policy involving batch limits was in place, which is not fully here)
        // For current logic, signal only relevant queue for entry conditions to be re-evaluated
        if (bridge->current_direction == NORTH) {
            pthread_cond_signal(&bridge->north_queue);
        } else if (bridge->current_direction == SOUTH) {
            pthread_cond_signal(&bridge->south_queue);
        }
    }

    pthread_mutex_unlock(&bridge->mtx);
}

// Car thread arguments
typedef struct {
    int id;
    int trips;
    Direction initial_direction;
    Bridge* bridge;
    int travel_time_min_ms, travel_time_max_ms;
    int cross_time_min_ms, cross_time_max_ms;
} CarArgs;

// Car thread function
void* car_func(void* arg) {
    CarArgs* args = (CarArgs*)arg;
    int id = args->id;
    int trips = args->trips;
    Direction current_dir = args->initial_direction;
    Bridge* bridge = args->bridge;

    for (int i = 0; i < trips; ++i) {
        // Simulate travel to bridge
        usleep((rand() % (args->travel_time_max_ms - args->travel_time_min_ms + 1) + args->travel_time_min_ms) * 1000);

        bridge_enter(bridge, id, current_dir);

        // Simulate crossing time
        usleep((rand() % (args->cross_time_max_ms - args->cross_time_min_ms + 1) + args->cross_time_min_ms) * 1000);

        bridge_exit(bridge, id, current_dir);

        // Switch direction for next trip
        current_dir = (current_dir == NORTH) ? SOUTH : NORTH;
    }
    printf("[%lld] Car-%d finished all %d trips.\n", get_timestamp_ms(), id, trips);
    free(args);
    return NULL;
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <trips> <northCars> <southCars>\n", argv[0]);
        return 1;
    }

    int trips = atoi(argv[1]);
    int north_cars_count = atoi(argv[2]);
    int south_cars_count = atoi(argv[3]);

    if (trips <= 0 || north_cars_count < 0 || south_cars_count < 0) {
        fprintf(stderr, "Invalid arguments: trips must be positive, car counts non-negative.\n");
        return 1;
    }

    Bridge bridge;
    bridge_init(&bridge);

    int total_cars = north_cars_count + south_cars_count;
    pthread_t* car_threads = (pthread_t*)malloc(sizeof(pthread_t) * total_cars);
    if (!car_threads) {
        perror("Failed to allocate car threads");
        return 1;
    }

    srand(time(NULL)); // Seed random number generator 

    int car_idx = 0;
    // Create north-bound cars
    for (int i = 0; i < north_cars_count; ++i) {
        CarArgs* args = (CarArgs*)malloc(sizeof(CarArgs));
        if (!args) { perror("Failed to allocate car args"); return 1; }
        args->id = i + 1;
        args->trips = trips;
        args->initial_direction = NORTH;
        args->bridge = &bridge;
        args->travel_time_min_ms = 100; args->travel_time_max_ms = 500;
        args->cross_time_min_ms = 50; args->cross_time_max_ms = 200;
        pthread_create(&car_threads[car_idx++], NULL, car_func, args);
    }

    // Create south-bound cars
    for (int i = 0; i < south_cars_count; ++i) {
        CarArgs* args = (CarArgs*)malloc(sizeof(CarArgs));
        if (!args) { perror("Failed to allocate car args"); return 1; }
        args->id = north_cars_count + i + 1;
        args->trips = trips;
        args->initial_direction = SOUTH;
        args->bridge = &bridge;
        args->travel_time_min_ms = 100; args->travel_time_max_ms = 500;
        args->cross_time_min_ms = 50; args->cross_time_max_ms = 200;
        pthread_create(&car_threads[car_idx++], NULL, car_func, args);
    }

    // Wait for all cars to finish
    for (int i = 0; i < total_cars; ++i) {
        pthread_join(car_threads[i], NULL);
    }

    printf("[%lld] Simulation finished.\n", get_timestamp_ms());
    bridge_destroy(&bridge);
    free(car_threads);

    return 0;
}
