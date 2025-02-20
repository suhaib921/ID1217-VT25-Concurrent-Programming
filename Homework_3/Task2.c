#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#define SHARED 1

sem_t bearSem;         
sem_t mutex;           
int H;                 // Pot capacity
int nHoneybees;        
int honeyInPot = 0;    // Current honey in pot

void *bear(void *arg) {
    while (1) { 
        sem_wait(&bearSem); // Wait for pot to be full

        sem_wait(&mutex);
        printf("\nBear: Pot is full! Eating %d portions...\n", H);
        honeyInPot = 0; 
        sleep(2); 
        printf("Bear: Finished eating. Going back to sleep.\n");
        sem_post(&mutex); // Release mutex after eating is done
    }
    return NULL;
}

void *honeybee(void *arg) {
    int id = (intptr_t)arg;
    while (1) {
        sleep((rand() % 2) + 1); 
        sem_wait(&mutex);
        if (honeyInPot < H) { // Add honey if there's space
            honeyInPot++;
            printf("Honeybee %d: Added 1 portion. Total: %d\n", id + 1, honeyInPot);
            if (honeyInPot == H) { // Notify bear if pot is full
                sem_post(&bearSem);
            }
        }
        sem_post(&mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <honeybees> <pot_capacity>\n", argv[0]);
        return EXIT_FAILURE;
    }

    nHoneybees = atoi(argv[1]);
    H = atoi(argv[2]);
    srand(time(NULL));

    // Initialize semaphores
    sem_init(&bearSem, SHARED, 0);
    sem_init(&mutex, SHARED, 1);

    pthread_t bearThread;
    pthread_t bees[nHoneybees];

    pthread_create(&bearThread, NULL, bear, NULL);
    for (int i = 0; i < nHoneybees; i++) {
        pthread_create(&bees[i], NULL, honeybee, (void *)(intptr_t)i);
    }

    pthread_join(bearThread, NULL);
    for (int i = 0; i < nHoneybees; i++) pthread_join(bees[i], NULL);

    // Cleanup 
    sem_destroy(&bearSem);
    sem_destroy(&mutex);

    return 0;
}