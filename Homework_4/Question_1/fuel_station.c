/**
 * @file fuel_station.c
 * @brief Simulates a space fuel station using C monitors (Pthreads mutexes and condition variables).
 *
 * This program models a concurrent fuel station in space with the following properties:
 * - A limited number of parallel docking spots (V).
 * - Finite storage for two fuel types: nitrogen (N liters) and quantum fluid (Q liters).
 * - Two types of vehicles (threads):
 *   1. Ordinary Vehicles: Arrive, request a docking spot, request specific amounts of fuel,
 *      and wait if resources (docking spot or fuel) are unavailable.
 *   2. Supply Vehicles: Arrive, request a docking spot, deposit fuel (waiting if there's
 *      not enough storage space), and then act like ordinary vehicles to take fuel for their return trip.
 *
 * The `FuelStation` struct acts as a monitor, encapsulating all shared state (fuel levels,
 * docking spots) and synchronizing access to it using a mutex and multiple condition variables.
 * This ensures that vehicle threads wait without blocking others when resources are scarce.
 *
 * To compile:
 *   gcc -o fuel_station fuel_station.c -lpthread
 *
 * To run:
 *   ./fuel_station <num_ordinary_vehicles> <num_supply_vehicles> <num_trips>
 * Example:
 *   ./fuel_station 5 2 3
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For sleep
#include <time.h>   // For srand, time

// --- Configuration ---
const int MAX_NITROGEN = 5000;
const int MAX_QUANTUM_FLUID = 2000;
const int MAX_DOCKING_SPOTS = 5;
const int SUPPLY_VEHICLE_DELIVERY_AMOUNT = 1000;

// --- Monitor for the Fuel Station ---
typedef struct {
    pthread_mutex_t mtx;
    // Condition variables to handle different waiting reasons
    pthread_cond_t cv_dock_available;
    pthread_cond_t cv_fuel_available;
    pthread_cond_t cv_storage_available;

    // Shared resources
    int available_nitrogen;
    int available_quantum_fluid;
    int available_docks;
} FuelStation;

/**
 * @brief Initializes the FuelStation monitor.
 */
void fuel_station_init(FuelStation* fs) {
    pthread_mutex_init(&fs->mtx, NULL);
    pthread_cond_init(&fs->cv_dock_available, NULL);
    pthread_cond_init(&fs->cv_fuel_available, NULL);
    pthread_cond_init(&fs->cv_storage_available, NULL);

    fs->available_nitrogen = MAX_NITROGEN;
    fs->available_quantum_fluid = MAX_QUANTUM_FLUID;
    fs->available_docks = MAX_DOCKING_SPOTS;
    printf("Fuel station online. Docks: %d, Nitrogen: %dL, Quantum Fluid: %dL\n",
           fs->available_docks, fs->available_nitrogen, fs->available_quantum_fluid);
}

/**
 * @brief Destroys the FuelStation monitor resources.
 */
void fuel_station_destroy(FuelStation* fs) {
    pthread_mutex_destroy(&fs->mtx);
    pthread_cond_destroy(&fs->cv_dock_available);
    pthread_cond_destroy(&fs->cv_fuel_available);
    pthread_cond_destroy(&fs->cv_storage_available);
}

/**
 * @brief Called by a vehicle to request a docking spot.
 */
void fs_arrive(FuelStation* fs, int id) {
    pthread_mutex_lock(&fs->mtx);
    printf("[Vehicle %d]: Arrived at station, waiting for a dock.\n", id);
    while (fs->available_docks <= 0) {
        pthread_cond_wait(&fs->cv_dock_available, &fs->mtx);
    }
    fs->available_docks--;
    printf("[Vehicle %d]: Docked. Docks available: %d\n", id, fs->available_docks);
    pthread_mutex_unlock(&fs->mtx);
}

/**
 * @brief Called by a vehicle to leave its docking spot.
 */
void fs_leave(FuelStation* fs, int id) {
    pthread_mutex_lock(&fs->mtx);
    fs->available_docks++;
    printf("[Vehicle %d]: Leaving station. Docks available: %d\n", id, fs->available_docks);
    pthread_cond_signal(&fs->cv_dock_available); // Notify one waiting vehicle for a dock
    pthread_mutex_unlock(&fs->mtx);
}

/**
 * @brief Called by ordinary and supply vehicles to get fuel.
 */
void fs_get_fuel(FuelStation* fs, int id, int requested_nitrogen, int requested_quantum) {
    pthread_mutex_lock(&fs->mtx);
    printf("[Vehicle %d]: Requesting %dL N2, %dL QF.\n", id, requested_nitrogen, requested_quantum);
    
    while (fs->available_nitrogen < requested_nitrogen || fs->available_quantum_fluid < requested_quantum) {
        pthread_cond_wait(&fs->cv_fuel_available, &fs->mtx);
    }

    fs->available_nitrogen -= requested_nitrogen;
    fs->available_quantum_fluid -= requested_quantum;

    printf("[Vehicle %d]: Got fuel. Station levels: N2=%d, QF=%d\n", id, fs->available_nitrogen, fs->available_quantum_fluid);
    
    // We took fuel, so there might be storage space now.
    pthread_cond_broadcast(&fs->cv_storage_available); // Notify all storage waiters
    pthread_mutex_unlock(&fs->mtx);
}

/**
 * @brief Called by supply vehicles to deposit fuel.
 */
void fs_deposit_fuel(FuelStation* fs, int id, int deposit_nitrogen, int deposit_quantum) {
    pthread_mutex_lock(&fs->mtx);
    printf("[Supply %d]: Wants to deposit %dL N2, %dL QF.\n", id, deposit_nitrogen, deposit_quantum);

    while ((fs->available_nitrogen + deposit_nitrogen > MAX_NITROGEN) ||
           (fs->available_quantum_fluid + deposit_quantum > MAX_QUANTUM_FLUID)) {
        pthread_cond_wait(&fs->cv_storage_available, &fs->mtx);
    }
    
    fs->available_nitrogen += deposit_nitrogen;
    fs->available_quantum_fluid += deposit_quantum;

    printf("[Supply %d]: Deposited fuel. Station levels: N2=%d, QF=%d\n", id, fs->available_nitrogen, fs->available_quantum_fluid);
    
    // We added fuel, so other vehicles might be able to get some now.
    pthread_cond_broadcast(&fs->cv_fuel_available); // Notify all fuel waiters
    pthread_mutex_unlock(&fs->mtx);
}

// --- Thread Argument Structures ---
typedef struct {
    int id;
    int num_trips;
    FuelStation* station;
    int travel_time_min, travel_time_max;
    int action_time_min, action_time_max;
    int fuel_request_min, fuel_request_max;
} VehicleArgs;

// --- Thread Functions ---

void* ordinary_vehicle_func(void* arg) {
    VehicleArgs* args = (VehicleArgs*)arg;
    int id = args->id;
    int num_trips = args->num_trips;
    FuelStation* station = args->station;

    for (int i = 0; i < num_trips; ++i) {
        usleep((rand() % (args->travel_time_max - args->travel_time_min + 1) + args->travel_time_min) * 1000000); // Travel time
        
        fs_arrive(station, id);
        usleep((rand() % (args->action_time_max - args->action_time_min + 1) + args->action_time_min) * 1000000); // Action time
        
        int n2 = rand() % (args->fuel_request_max - args->fuel_request_min + 1) + args->fuel_request_min;
        int qf = rand() % (args->fuel_request_max - args->fuel_request_min + 1) + args->fuel_request_min;
        fs_get_fuel(station, id, n2, qf);
        
        usleep((rand() % (args->action_time_max - args->action_time_min + 1) + args->action_time_min) * 1000000); // Action time
        fs_leave(station, id);
    }
    printf(">> Ordinary Vehicle %d finished all trips.\n", id);
    free(args);
    return NULL;
}

void* supply_vehicle_func(void* arg) {
    VehicleArgs* args = (VehicleArgs*)arg;
    int id = args->id;
    int num_trips = args->num_trips;
    FuelStation* station = args->station;

    for (int i = 0; i < num_trips; ++i) {
        usleep((rand() % (args->travel_time_max - args->travel_time_min + 1) + args->travel_time_min) * 1000000); // Travel time
        
        fs_arrive(station, id);
        usleep((rand() % (args->action_time_max - args->action_time_min + 1) + args->action_time_min) * 1000000); // Action time

        // Deposit fuel
        int n2_deposit = (rand() % 2) * SUPPLY_VEHICLE_DELIVERY_AMOUNT; // 0 or amount
        int qf_deposit = (n2_deposit == 0) ? SUPPLY_VEHICLE_DELIVERY_AMOUNT : 0; // If N2 not deposited, deposit QF
        fs_deposit_fuel(station, id, n2_deposit, qf_deposit);

        // Take fuel for return trip
        usleep((rand() % (args->action_time_max - args->action_time_min + 1) + args->action_time_min) * 1000000); // Action time
        int n2_take = (rand() % (args->fuel_request_max - args->fuel_request_min + 1) + args->fuel_request_min) / 2;
        int qf_take = (rand() % (args->fuel_request_max - args->fuel_request_min + 1) + args->fuel_request_min) / 2;
        fs_get_fuel(station, id, n2_take, qf_take);
        
        usleep((rand() % (args->action_time_max - args->action_time_min + 1) + args->action_time_min) * 1000000); // Action time
        fs_leave(station, id);
    }
    printf(">> Supply Vehicle %d finished all trips.\n", id);
    free(args);
    return NULL;
}

// --- Main ---
int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_ordinary_vehicles> <num_supply_vehicles> <num_trips>\n", argv[0]);
        return 1;
    }

    int num_ordinary = atoi(argv[1]);
    int num_supply = atoi(argv[2]);
    int num_trips = atoi(argv[3]);

    if (num_ordinary < 0 || num_supply < 0 || num_trips <= 0) {
        fprintf(stderr, "Invalid arguments: counts must be non-negative, trips must be positive.\n");
        return 1;
    }

    FuelStation station;
    fuel_station_init(&station);

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * (num_ordinary + num_supply));
    if (!threads) { perror("Failed to allocate threads"); return 1; }
    int thread_idx = 0;

    srand(time(NULL)); // Seed random number generator

    for (int i = 0; i < num_ordinary; ++i) {
        VehicleArgs* args = (VehicleArgs*)malloc(sizeof(VehicleArgs));
        if (!args) { perror("Failed to allocate args"); return 1; }
        args->id = i + 1;
        args->num_trips = num_trips;
        args->station = &station;
        args->travel_time_min = 3; args->travel_time_max = 6;
        args->action_time_min = 1; args->action_time_max = 2;
        args->fuel_request_min = 100; args->fuel_request_max = 500;
        pthread_create(&threads[thread_idx++], NULL, ordinary_vehicle_func, args);
    }

    for (int i = 0; i < num_supply; ++i) {
        VehicleArgs* args = (VehicleArgs*)malloc(sizeof(VehicleArgs));
        if (!args) { perror("Failed to allocate args"); return 1; }
        args->id = num_ordinary + i + 1;
        args->num_trips = num_trips;
        args->station = &station;
        args->travel_time_min = 3; args->travel_time_max = 6;
        args->action_time_min = 1; args->action_time_max = 2;
        args->fuel_request_min = 100; args->fuel_request_max = 500;
        pthread_create(&threads[thread_idx++], NULL, supply_vehicle_func, args);
    }

    for (int i = 0; i < (num_ordinary + num_supply); ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("Simulation finished.\n");
    fuel_station_destroy(&station);
    free(threads);

    return 0;
}
