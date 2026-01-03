/**
 * @file repair_station.c
 * @brief Simulates a multi-type vehicle repair station using C monitors (Pthreads).
 *
 * This program models a concurrent repair station with complex capacity constraints:
 * - It can repair a maximum of `MAX_TYPE_A`, `MAX_TYPE_B`, and `MAX_TYPE_C` vehicles
 *   of types 'A', 'B', and 'C' respectively.
 * - It can repair a maximum of `MAX_TOTAL_VEHICLES` vehicles in total, regardless of type.
 *
 * Vehicles (threads) arrive and request repairs. If the station is at capacity for
 * their specific type OR at total capacity, they must wait.
 *
 * The `RepairStation` struct acts as a monitor, managing the counts of vehicles
 * currently being repaired. A single mutex protects all shared data, and a single
 * condition variable is used to wake up waiting threads. When a thread wakes up, it
 * must re-check if the conditions are met to proceed. This is a common and robust
 * pattern for handling complex wait conditions.
 *
 * Fairness: This solution is not inherently fair. If there is a constant stream of
 * vehicles of one type, another type might wait for a long time if its specific
 * slot is never free. A more complex queuing system would be needed for true fairness,
 * but that is beyond the scope of this implementation.
 *
 * To compile:
 *   gcc -o repair_station repair_station.c -lpthread
 *
 * To run:
 *   ./repair_station <num_type_A> <num_type_B> <num_type_C> <num_trips>
 * Example:
 *   ./repair_station 4 5 3 2
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For sleep
#include <time.h>   // For srand, time

// --- Configuration ---
const int MAX_TYPE_A = 3;
const int MAX_TYPE_B = 2;
const int MAX_TYPE_C = 4;
const int MAX_TOTAL_VEHICLES = 7; // Must be >= the max of any single type

typedef enum { TYPE_A, TYPE_B, TYPE_C } VehicleType;

// --- Monitor for the Repair Station ---
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cv_space_available;

    // Counters for vehicles currently being repaired
    int count_A;
    int count_B;
    int count_C;
} RepairStation;

/**
 * @brief Initializes the RepairStation monitor.
 */
void repair_station_init(RepairStation* rs) {
    pthread_mutex_init(&rs->mtx, NULL);
    pthread_cond_init(&rs->cv_space_available, NULL);

    rs->count_A = 0;
    rs->count_B = 0;
    rs->count_C = 0;
    printf("Repair station online. Capacities: Total=%d, Type A=%d, Type B=%d, Type C=%d\n",
           MAX_TOTAL_VEHICLES, MAX_TYPE_A, MAX_TYPE_B, MAX_TYPE_C);
}

/**
 * @brief Destroys the RepairStation monitor resources.
 */
void repair_station_destroy(RepairStation* rs) {
    pthread_mutex_destroy(&rs->mtx);
    pthread_cond_destroy(&rs->cv_space_available);
}

/**
 * @brief Checks if a vehicle of a given type can enter based on current station load.
 * This function must be called with the monitor's mutex locked.
 */
bool can_enter(RepairStation* rs, VehicleType type) {
    int total_vehicles = rs->count_A + rs->count_B + rs->count_C;
    if (total_vehicles >= MAX_TOTAL_VEHICLES) return false;

    switch(type) {
        case TYPE_A: return rs->count_A < MAX_TYPE_A;
        case TYPE_B: return rs->count_B < MAX_TYPE_B;
        case TYPE_C: return rs->count_C < MAX_TYPE_C;
    }
    return false; // Should not be reached
}

/**
 * @brief A vehicle requests to enter the station for repairs.
 * The vehicle will wait if its type-specific slot is full or if the total
 * vehicle capacity is reached.
 */
void rs_request_repair(RepairStation* rs, int id, VehicleType type) {
    pthread_mutex_lock(&rs->mtx);
    
    printf("[Vehicle %d (Type %c)]: Arrived, requesting repair.\n", id, (char)('A' + type));

    while (!can_enter(rs, type)) {
        pthread_cond_wait(&rs->cv_space_available, &rs->mtx);
    }

    // Granted entry, update counts
    switch(type) {
        case TYPE_A: rs->count_A++; break;
        case TYPE_B: rs->count_B++; break;
        case TYPE_C: rs->count_C++; break;
    }
    
    printf("[Vehicle %d (Type %c)]: Entered for repair. Station load: "
           "A=%d, B=%d, C=%d, Total=%d\n", id, (char)('A' + type), rs->count_A, rs->count_B, rs->count_C,
           rs->count_A + rs->count_B + rs->count_C);
    pthread_mutex_unlock(&rs->mtx);
}

/**
 * @brief A vehicle signals that its repair is finished.
 * This frees up a slot, and all waiting threads are notified to re-check conditions.
 */
void rs_release_from_repair(RepairStation* rs, int id, VehicleType type) {
    pthread_mutex_lock(&rs->mtx);
    
    switch(type) {
        case TYPE_A: rs->count_A--; break;
        case TYPE_B: rs->count_B--; break;
        case TYPE_C: rs->count_C--; break;
    }

    printf("[Vehicle %d (Type %c)]: Repair finished, leaving. Station load: "
           "A=%d, B=%d, C=%d, Total=%d\n", id, (char)('A' + type), rs->count_A, rs->count_B, rs->count_C,
           rs->count_A + rs->count_B + rs->count_C);

    // Notify all waiting threads because a slot of any type might become available
    // for a waiting vehicle if the total count was the limiting factor.
    pthread_cond_broadcast(&rs->cv_space_available);
    pthread_mutex_unlock(&rs->mtx);
}

// --- Thread Argument Structures ---
typedef struct {
    int id;
    VehicleType type;
    int num_trips;
    RepairStation* station;
    int travel_time_min, travel_time_max;
    int repair_time_min, repair_time_max;
} VehicleArgs;

// --- Thread Function ---

void* vehicle_thread_func(void* arg) {
    VehicleArgs* args = (VehicleArgs*)arg;
    int id = args->id;
    VehicleType type = args->type;
    int num_trips = args->num_trips;
    RepairStation* station = args->station;

    for (int i = 0; i < num_trips; ++i) {
        // Travel time
        usleep((rand() % (args->travel_time_max - args->travel_time_min + 1) + args->travel_time_min) * 1000000);
        
        rs_request_repair(station, id, type);
        
        // Repair time
        usleep((rand() % (args->repair_time_max - args->repair_time_min + 1) + args->repair_time_min) * 1000000);
        
        rs_release_from_repair(station, id, type);
    }
    printf(">> Vehicle %d finished all trips.\n", id);
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <num_type_A> <num_type_B> <num_type_C> <num_trips>\n", argv[0]);
        return 1;
    }

    int num_A = atoi(argv[1]);
    int num_B = atoi(argv[2]);
    int num_C = atoi(argv[3]);
    int num_trips = atoi(argv[4]);

    if (num_A < 0 || num_B < 0 || num_C < 0 || num_trips <= 0) {
        fprintf(stderr, "Invalid arguments: counts must be non-negative, trips must be positive.\n");
        return 1;
    }

    RepairStation station;
    repair_station_init(&station);

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * (num_A + num_B + num_C));
    if (!threads) { perror("Failed to allocate threads"); return 1; }
    int thread_idx = 0;
    int vehicle_id = 1;

    srand(time(NULL)); // Seed random number generator

    for (int i = 0; i < num_A; ++i, ++vehicle_id) {
        VehicleArgs* args = (VehicleArgs*)malloc(sizeof(VehicleArgs));
        if (!args) { perror("Failed to allocate args"); return 1; }
        args->id = vehicle_id; args->type = TYPE_A; args->num_trips = num_trips; args->station = &station;
        args->travel_time_min = 3; args->travel_time_max = 5;
        args->repair_time_min = 2; args->repair_time_max = 4;
        pthread_create(&threads[thread_idx++], NULL, vehicle_thread_func, args);
    }
    for (int i = 0; i < num_B; ++i, ++vehicle_id) {
        VehicleArgs* args = (VehicleArgs*)malloc(sizeof(VehicleArgs));
        if (!args) { perror("Failed to allocate args"); return 1; }
        args->id = vehicle_id; args->type = TYPE_B; args->num_trips = num_trips; args->station = &station;
        args->travel_time_min = 3; args->travel_time_max = 5;
        args->repair_time_min = 2; args->repair_time_max = 4;
        pthread_create(&threads[thread_idx++], NULL, vehicle_thread_func, args);
    }
    for (int i = 0; i < num_C; ++i, ++vehicle_id) {
        VehicleArgs* args = (VehicleArgs*)malloc(sizeof(VehicleArgs));
        if (!args) { perror("Failed to allocate args"); return 1; }
        args->id = vehicle_id; args->type = TYPE_C; args->num_trips = num_trips; args->station = &station;
        args->travel_time_min = 3; args->travel_time_max = 5;
        args->repair_time_min = 2; args->repair_time_max = 4;
        pthread_create(&threads[thread_idx++], NULL, vehicle_thread_func, args);
    }

    for (int i = 0; i < (num_A + num_B + num_C); ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("Simulation finished.\n");
    repair_station_destroy(&station);
    free(threads);

    return 0;
}
