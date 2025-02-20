#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h> 

#define NUM_BABY_BIRDS 6 
#define INITIAL_WORMS 3 

int worms_in_dish = INITIAL_WORMS; 
sem_t mutex;                    
sem_t empty;                      // Semaphore to signal when the dish is empty

int worms_eaten[NUM_BABY_BIRDS] = {0}; // Track number of worms eaten by each bird

void* parentBird(void* arg) {
    while (1) {
        // Wait until the dish is empty
        sem_wait(&empty);

        printf("Parent bird refilling the dish with %d worms.\n\n", INITIAL_WORMS);
        worms_in_dish = INITIAL_WORMS;
    }
}

void* babyBird(void* arg) {
    intptr_t id = (intptr_t)arg; 
    while (1) {
        // Wait until there are worms available
        sem_wait(&mutex);

        if (worms_in_dish > 0){
            worms_in_dish--;
            worms_eaten[id]++; // Increment the counter for this bird
            printf("Baby bird %ld took a worm. Worms left: %d. Total eaten: %d\n", (long)id, worms_in_dish, worms_eaten[id]);
            if (worms_in_dish == 0) {
                sem_post(&empty);
            }
        }

        sem_post(&mutex);


    }
}

int main() {
    pthread_t parent_thread;
    pthread_t baby_threads[NUM_BABY_BIRDS];

    // Initialize semaphores
    sem_init(&mutex, 0, 1); 
    sem_init(&empty, 0, 0); 

    pthread_create(&parent_thread, NULL, parentBird, NULL);
    for (int i = 0; i < NUM_BABY_BIRDS; i++) {
        pthread_create(&baby_threads[i], NULL, babyBird, (void*)(intptr_t)i); // Safely cast integer to pointer
    }

    // Join all threads
    pthread_join(parent_thread, NULL);
    for (int i = 0; i < NUM_BABY_BIRDS; i++) {
        pthread_join(baby_threads[i], NULL);
    }

    // Destroy semaphores
    sem_destroy(&mutex);
    sem_destroy(&empty);
    return 0;
}