/**
 * @file unisex_bathroom.c
 * @brief Simulates the Unisex Bathroom problem with a fair solution using POSIX semaphores.
 *
 * This program models a synchronization problem where men and women must share a
 * single bathroom. Any number of people of the same gender can use it simultaneously,
 * but men and women cannot be inside at the same time.
 *
 * This solution implements a fair readers-writers pattern using semaphores to
 * ensure:
 * 1. Mutual exclusion between genders: No men and women in the bathroom together.
 * 2. Concurrency for same gender: Multiple men or multiple women can be inside.
 * 3. Deadlock avoidance.
 * 4. Fairness: Prevents starvation of either gender by prioritizing the gender
 *    that has been waiting longest, or by ensuring strict alternation if both
 *    are waiting. A "turnstile" mechanism is used to order arrivals.
 *
 * To compile:
 *   gcc -o unisex_bathroom unisex_bathroom.c -lpthread
 *
 * To run:
 *   ./unisex_bathroom <num_men> <num_women> <num_visits>
 *   Example: ./unisex_bathroom 5 5 3
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

typedef enum { MALE, FEMALE } Gender;

// Shared state
int men_in_bathroom = 0;      // Count of men currently in bathroom
int women_in_bathroom = 0;    // Count of women currently in bathroom
int men_waiting_entry = 0;    // Count of men waiting for entry
int women_waiting_entry = 0;  // Count of women waiting for entry

// Semaphores for fairness and mutual exclusion
sem_t mutex;           // General mutex for shared counters (men_in_bathroom, women_in_bathroom, etc.)
sem_t bathroom_door;   // Controls who enters the bathroom (binary, 1)
sem_t turnstile;       // Ensures alternating access (binary, 1)
sem_t male_queue;      // Men wait here if bathroom occupied by women or priority given to women
sem_t female_queue;    // Women wait here if bathroom occupied by men or priority given to men

/**
 * @brief Simulates a man entering, using, and leaving the bathroom.
 */
void use_bathroom_male(long id) {
    // --- Entry Protocol ---
    printf("Man %ld is waiting to enter the bathroom.\n", id);
    sem_wait(&turnstile); // Acquire turnstile to ensure ordered arrival
    sem_wait(&mutex);     // Lock mutex to access shared counters
    
    men_waiting_entry++;
    // If there are women inside, or if women are waiting and it's their turn
    if (women_in_bathroom > 0 || (men_in_bathroom == 0 && women_waiting_entry > 0)) {
        sem_post(&mutex);     // Release mutex
        sem_post(&turnstile); // Release turnstile
        sem_wait(&male_queue); // Wait for our turn
        sem_wait(&mutex);     // Re-acquire mutex
    }
    men_waiting_entry--;

    men_in_bathroom++;
    if (men_in_bathroom == 1) { // First man, acquire exclusive access to bathroom
        sem_wait(&bathroom_door);
    }
    printf("==> Man %ld entered the bathroom. Men: %d, Women: %d\n", id, men_in_bathroom, women_in_bathroom);

    sem_post(&mutex);     // Release mutex
    sem_post(&turnstile); // Release turnstile

    // --- Critical Section: In the bathroom ---
    sleep((rand() % 2) + 1); // Simulate time in bathroom

    // --- Exit Protocol ---
    sem_wait(&mutex); // Lock mutex to access shared counters

    men_in_bathroom--;
    printf("<== Man %ld left the bathroom. Men: %d, Women: %d\n", id, men_in_bathroom, women_in_bathroom);

    if (men_in_bathroom == 0) { // Last man, release exclusive access
        sem_post(&bathroom_door);
        // Now, if women are waiting, give them priority
        if (women_waiting_entry > 0) {
            for(int i = 0; i < women_waiting_entry; i++) {
                sem_post(&female_queue); // Wake up all waiting women
            }
        }
    }
    sem_post(&mutex); // Release mutex
}

/**
 * @brief Simulates a woman entering, using, and leaving the bathroom.
 */
void use_bathroom_female(long id) {
    // --- Entry Protocol ---
    printf("Woman %ld is waiting to enter the bathroom.\n", id);
    sem_wait(&turnstile); // Acquire turnstile to ensure ordered arrival
    sem_wait(&mutex);     // Lock mutex to access shared counters

    women_waiting_entry++;
    // If there are men inside, or if men are waiting and it's their turn
    if (men_in_bathroom > 0 || (women_in_bathroom == 0 && men_waiting_entry > 0)) {
        sem_post(&mutex);     // Release mutex
        sem_post(&turnstile); // Release turnstile
        sem_wait(&female_queue); // Wait for our turn
        sem_wait(&mutex);     // Re-acquire mutex
    }
    women_waiting_entry--;

    women_in_bathroom++;
    if (women_in_bathroom == 1) { // First woman, acquire exclusive access to bathroom
        sem_wait(&bathroom_door);
    }
    printf("==> Woman %ld entered the bathroom. Men: %d, Women: %d\n", id, men_in_bathroom, women_in_bathroom);

    sem_post(&mutex);     // Release mutex
    sem_post(&turnstile); // Release turnstile

    // --- Critical Section: In the bathroom ---
    sleep((rand() % 2) + 1); // Simulate time in bathroom

    // --- Exit Protocol ---
    sem_wait(&mutex); // Lock mutex to access shared counters

    women_in_bathroom--;
    printf("<== Woman %ld left the bathroom. Men: %d, Women: %d\n", id, men_in_bathroom, women_in_bathroom);

    if (women_in_bathroom == 0) { // Last woman, release exclusive access
        sem_post(&bathroom_door);
        // Now, if men are waiting, give them priority
        if (men_waiting_entry > 0) {
            for(int i = 0; i < men_waiting_entry; i++) {
                sem_post(&male_queue); // Wake up all waiting men
            }
        }
    }
    sem_post(&mutex); // Release mutex
}

/**
 * @brief The main thread function for a person.
 */
void* person_func(void* arg) {
    PersonArgs* args = (PersonArgs*)arg;
    
    for (int i = 0; i < args->visits; i++) {
        // "Work" for a while
        printf("%s %ld is working before visit #%d.\n", args->gender == MALE ? "Man" : "Woman", args->id, i + 1);
        sleep((rand() % 3) + 1);

        if (args->gender == MALE) {
            use_bathroom_male(args->id);
        } else {
            use_bathroom_female(args->id);
        }
    }

    printf("%s %ld has finished all bathroom visits.\n", args->gender == MALE ? "Man" : "Woman", args->id);
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_men> <num_women> <num_visits>\n", argv[0]);
        return 1;
    }

    int num_men = atoi(argv[1]);
    int num_women = atoi(argv[2]);
    int num_visits = atoi(argv[3]);

    if (num_men < 0 || num_women < 0 || num_visits <= 0) {
        fprintf(stderr, "Number of people cannot be negative, and visits must be positive.\n");
        return 1;
    }

    srand(time(NULL));

    // Initialize semaphores
    sem_init(&mutex, 0, 1);
    sem_init(&bathroom_door, 0, 1);
    sem_init(&turnstile, 0, 1);
    sem_init(&male_queue, 0, 0);
    sem_init(&female_queue, 0, 0);

    pthread_t person_threads[num_men + num_women];

    int thread_count = 0;
    // Create men threads
    for (long i = 0; i < num_men; i++) {
        PersonArgs* args = malloc(sizeof(PersonArgs));
        args->id = i + 1; // Person IDs start from 1
        args->gender = MALE;
        args->visits = num_visits;
        pthread_create(&person_threads[thread_count++], NULL, person_func, args);
    }

    // Create women threads
    for (long i = 0; i < num_women; i++) {
        PersonArgs* args = malloc(sizeof(PersonArgs));
        args->id = num_men + i + 1; // Ensure unique IDs
        args->gender = FEMALE;
        args->visits = num_visits;
        pthread_create(&person_threads[thread_count++], NULL, person_func, args);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_men + num_women; i++) {
        pthread_join(person_threads[i], NULL);
    }

    printf("Simulation finished. Everyone is done.\n");

    // Destroy semaphores
    sem_destroy(&mutex);
    sem_destroy(&bathroom_door);
    sem_destroy(&turnstile);
    sem_destroy(&male_queue);
    sem_destroy(&female_queue);

    return 0;
}
