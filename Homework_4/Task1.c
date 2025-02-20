/*gcc -std=c99 -o Task1 Task1.c -lpthread
./Task1
*/
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>


typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t dock_cond;
    pthread_cond_t fuel_cond;
    pthread_cond_t supply_cond;
    int docks_available;
    int nitrogen;
    int quantum;
    int max_docks;
    int max_nitrogen;
    int max_quantum;
} StationMonitor;

typedef struct {
    int id;
    StationMonitor *monitor;
    int is_supply;
    int arrival_count;
    int req_n;
    int req_q;
    int supply_n;
    int supply_q;
    int req_supply_n;
    int req_supply_q;
} VehicleParams;

void* regular_vehicle_thread(void *arg) {
    VehicleParams *params = (VehicleParams*) arg;
    for (int i = 0; i < params->arrival_count; i++) {
        usleep(rand() % 1000000);
        printf("Regular vehicle %d arrives at station (iteration %d)\n", params->id, i+1);

        int success = 0;
        while (!success) {
            pthread_mutex_lock(&params->monitor->mutex);
            while (params->monitor->docks_available == 0) {
                pthread_cond_wait(&params->monitor->dock_cond, &params->monitor->mutex);
            }
            params->monitor->docks_available--;
            printf("Regular vehicle %d docked\n", params->id);

            if (params->monitor->nitrogen >= params->req_n && params->monitor->quantum >= params->req_q) {
                params->monitor->nitrogen -= params->req_n;
                params->monitor->quantum -= params->req_q;
                printf("Regular vehicle %d took %d N and %d Q\n", params->id, params->req_n, params->req_q);
                success = 1;
                params->monitor->docks_available++;
                pthread_cond_signal(&params->monitor->dock_cond);
                pthread_cond_broadcast(&params->monitor->supply_cond);
            } else {
                params->monitor->docks_available++;
                printf("Regular vehicle %d undocked (waiting for fuel)\n", params->id);
                pthread_cond_signal(&params->monitor->dock_cond);
                while (params->monitor->nitrogen < params->req_n || params->monitor->quantum < params->req_q) {
                    pthread_cond_wait(&params->monitor->fuel_cond, &params->monitor->mutex);
                }
            }
            pthread_mutex_unlock(&params->monitor->mutex);
        }
        usleep(rand() % 500000);
    }
    return NULL;
}

void* supply_vehicle_thread(void *arg) {
    VehicleParams *params = (VehicleParams*) arg;
    for (int i = 0; i < params->arrival_count; i++) {
        usleep(rand() % 1000000);
        printf("Supply vehicle %d arrives at station (iteration %d)\n", params->id, i+1);

        pthread_mutex_lock(&params->monitor->mutex);
        while (params->monitor->docks_available == 0) {
            pthread_cond_wait(&params->monitor->dock_cond, &params->monitor->mutex);
        }
        params->monitor->docks_available--;
        printf("Supply vehicle %d docked\n", params->id);

        while (params->monitor->nitrogen + params->supply_n > params->monitor->max_nitrogen ||
               params->monitor->quantum + params->supply_q > params->monitor->max_quantum) {
            pthread_cond_wait(&params->monitor->supply_cond, &params->monitor->mutex);
        }
        params->monitor->nitrogen += params->supply_n;
        params->monitor->quantum += params->supply_q;
        printf("Supply vehicle %d deposited %d N and %d Q\n", params->id, params->supply_n, params->supply_q);
        pthread_cond_broadcast(&params->monitor->fuel_cond);

        int success = 0;
        while (!success) {
            if (params->monitor->nitrogen >= params->req_supply_n && params->monitor->quantum >= params->req_supply_q) {
                params->monitor->nitrogen -= params->req_supply_n;
                params->monitor->quantum -= params->req_supply_q;
                printf("Supply vehicle %d took %d N and %d Q for return\n", params->id, params->req_supply_n, params->req_supply_q);
                success = 1;
                params->monitor->docks_available++;
                pthread_cond_signal(&params->monitor->dock_cond);
            } else {
                params->monitor->docks_available++;
                printf("Supply vehicle %d undocked (waiting for fuel)\n", params->id);
                pthread_cond_signal(&params->monitor->dock_cond);
                while (params->monitor->nitrogen < params->req_supply_n || params->monitor->quantum < params->req_supply_q) {
                    pthread_cond_wait(&params->monitor->fuel_cond, &params->monitor->mutex);
                }
                while (params->monitor->docks_available == 0) {
                    pthread_cond_wait(&params->monitor->dock_cond, &params->monitor->mutex);
                }
                params->monitor->docks_available--;
                printf("Supply vehicle %d redocked\n", params->id);
            }
        }
        pthread_mutex_unlock(&params->monitor->mutex);
        usleep(rand() % 500000);
    }
    return NULL;
}

int main() {
    srand(time(NULL));
    StationMonitor monitor = {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .dock_cond = PTHREAD_COND_INITIALIZER,
        .fuel_cond = PTHREAD_COND_INITIALIZER,
        .supply_cond = PTHREAD_COND_INITIALIZER,
        .docks_available = 2,
        .nitrogen = 0,
        .quantum = 0,
        .max_docks = 2,
        .max_nitrogen = 1000,
        .max_quantum = 1000
    };

    VehicleParams regular1 = {1, &monitor, 0, 3, 100, 200, 0, 0, 0, 0};
    VehicleParams regular2 = {2, &monitor, 0, 3, 150, 50, 0, 0, 0, 0};
    VehicleParams supply1 = {3, &monitor, 1, 2, 0, 0, 500, 500, 100, 100};

    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, regular_vehicle_thread, &regular1);
    pthread_create(&t2, NULL, regular_vehicle_thread, &regular2);
    pthread_create(&t3, NULL, supply_vehicle_thread, &supply1);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return 0;
}