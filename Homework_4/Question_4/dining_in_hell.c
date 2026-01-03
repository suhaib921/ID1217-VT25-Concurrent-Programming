/**
 * @file dining_in_hell.c
 * @brief Simulates the "Dining in Hell" problem using a Pthreads monitor.
 *
 * This problem is a variation of the Dining Philosophers. Five people are at a
 * table, but their spoons are too long to feed themselves. They must feed each other.
 * The constraints are:
 * 1. A person cannot feed themself.
 * 2. A person cannot be fed by more than one person at a time.
 * 3. A person cannot feed another person while they are being fed.
 * 4. The solution must be starvation-free.
 *
 * This solution models the problem using a central monitor (`HellishTable`).
 * To ensure starvation-freedom and avoid deadlock, this implementation uses a
 * strategy where a designated `feeder_thread` rotates its turn to feed someone.
 *
 * - The `feeder_thread` (Person 0 in this case) takes turns feeding a hungry person.
 * - The feeder chooses the next available hungry person to their right to feed.
 * - When a person wants to eat, they mark themselves as `HUNGRY` and wait.
 * - When the current feeder is done feeding, they pass the role of feeder to the
 *   next person at the table (implemented by incrementing `current_feeder` index).
 *
 * This round-robin feeder schedule guarantees that every person gets a turn to be
 * the feeder, and since the feeder always picks a hungry person, everyone will
 * eventually be fed.
 *
 * The simulation includes a termination mechanism: once all person threads
 * complete their rounds, the main thread signals the feeder thread to terminate.
 *
 * To compile:
 *   gcc -o dining_in_hell dining_in_hell.c -lpthread -lm
 *
 * To run:
 *   ./dining_in_hell <rounds>
 * Example:
 *   ./dining_in_hell 5
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For usleep
#include <time.h>   // For srand, time

#define NUM_PEOPLE 5

typedef enum { THINKING, HUNGRY, EATING } PersonState;

// --- Monitor for the HellishTable ---
typedef struct {
    pthread_mutex_t mtx;
    PersonState states[NUM_PEOPLE];
    pthread_cond_t cv_person[NUM_PEOPLE]; // Each person waits on their own CV
    
    int current_feeder; // Person whose turn it is to feed someone

    bool terminate_feeder; // Flag to signal feeder thread to stop
    pthread_cond_t cv_feeder_terminate; // CV for feeder to wait on termination signal
} HellishTable;

// Helper function to initialize the table
void table_init(HellishTable* table) {
    pthread_mutex_init(&table->mtx, NULL);
    for (int i = 0; i < NUM_PEOPLE; ++i) {
        table->states[i] = THINKING;
        pthread_cond_init(&table->cv_person[i], NULL);
    }
    table->current_feeder = 0; // Person 0 starts as the initial feeder
    table->terminate_feeder = false;
    pthread_cond_init(&table->cv_feeder_terminate, NULL);
}

// Helper function to destroy the table resources
void table_destroy(HellishTable* table) {
    pthread_mutex_destroy(&table->mtx);
    for (int i = 0; i < NUM_PEOPLE; ++i) {
        pthread_cond_destroy(&table->cv_person[i]);
    }
    pthread_cond_destroy(&table->cv_feeder_terminate);
}

/**
 * @brief The core logic for the feeder. This must be called with the mutex locked.
 * The current feeder looks for a hungry person to feed.
 */
void feeder_action(HellishTable* table) {
    // Find a person to feed. To be fair, we search in order starting from the feeder's right.
    for (int i = 1; i < NUM_PEOPLE; ++i) {
        int person_to_feed = (table->current_feeder + i) % NUM_PEOPLE;
        if (table->states[person_to_feed] == HUNGRY) {
            
            table->states[person_to_feed] = EATING;
            
            printf("--> Person %d is now feeding Person %d.\n", table->current_feeder, person_to_feed);
            
            // Wake up the person being fed so they know they are eating
            pthread_cond_signal(&table->cv_person[person_to_feed]);
            
            // Feeder now waits until the person is done eating.
            // The person being fed will signal the feeder when done.
            pthread_cond_wait(&table->cv_person[table->current_feeder], &table->mtx);
            
            // The meal is over.
            printf("<-- Person %d has finished feeding Person %d.\n", table->current_feeder, person_to_feed);
            
            // Pass the role of feeder to the next person
            table->current_feeder = (table->current_feeder + 1) % NUM_PEOPLE;
            printf("Feeder role passed to Person %d.\n", table->current_feeder);
            
            // Wake everyone up to allow the new feeder to act or others to become hungry.
            for (int j = 0; j < NUM_PEOPLE; ++j) pthread_cond_signal(&table->cv_person[j]);

            return; // Feeder has done their job for this turn. 
        }
    }
    // If no one is hungry, the feeder does nothing and will re-check on the next wakeup.
}

/**
 * @brief A person requests to be fed.
 */
void request_to_eat(HellishTable* table, int id) {
    pthread_mutex_lock(&table->mtx);
    table->states[id] = HUNGRY;
    printf("Person %d is hungry.\n", id);

    // Wake up the current feeder, notifying them that someone is hungry
    pthread_cond_signal(&table->cv_person[table->current_feeder]);
    
    // Wait until my state is changed to EATING by a feeder
    while (table->states[id] != EATING) {
        pthread_cond_wait(&table->cv_person[id], &table->mtx);
    }

    printf("Person %d is being fed.\n", id);
    pthread_mutex_unlock(&table->mtx);
}

/**
 * @brief A person finishes eating.
 */
void finish_eating(HellishTable* table, int id) {
    pthread_mutex_lock(&table->mtx);
    table->states[id] = THINKING;
    printf("Person %d is done eating.\n", id);

    // Find who was feeding me and signal them that I am done.
    // In this model, the feeder is waiting on their own CV.
    // We signal the current designated feeder.
    pthread_cond_signal(&table->cv_person[table->current_feeder]);
    pthread_mutex_unlock(&table->mtx);
}

/**
 * @brief Main loop for the feeder thread.
 * This thread is responsible for matching hungry people with available feeders.
 */
void* feeder_loop_func(void* arg) {
    HellishTable* table = (HellishTable*)arg;
    pthread_mutex_lock(&table->mtx);
    while (!table->terminate_feeder) {
        feeder_action(table);
        // Wait until someone becomes hungry or a meal finishes, or for termination signal
        pthread_cond_wait(&table->cv_feeder_terminate, &table->mtx);
    }
    printf("Feeder thread terminating.\n");
    pthread_mutex_unlock(&table->mtx);
    return NULL;
}

// --- Thread Functions ---
typedef struct {
    int id;
    int rounds;
    HellishTable* table;
    int think_time_min, think_time_max;
    int eat_time_min, eat_time_max;
} PersonArgs;

void* person_thread_func(void* arg) {
    PersonArgs* args = (PersonArgs*)arg;
    int id = args->id;
    int rounds = args->rounds;
    HellishTable* table = args->table;

    for (int i = 0; i < rounds; ++i) {
        // Think
        int think_time = args->think_time_min + (rand() % (args->think_time_max - args->think_time_min + 1));
        printf("Person %d is thinking.\n", id);
        usleep(think_time * 1000); // usleep takes microseconds
        
        // Get hungry and request to be fed
        request_to_eat(table, id);
        
        // Eat (being fed)
        int eat_time = args->eat_time_min + (rand() % (args->eat_time_max - args->eat_time_min + 1));
        usleep(eat_time * 1000); // usleep takes microseconds
        
        // Finish eating
        finish_eating(table, id);
    }
    printf(">> Person %d finished all rounds.\n", id);
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <rounds>\n", argv[0]);
        return 1;
    }
    int rounds = atoi(argv[1]);

    HellishTable table;
    table_init(&table);

    pthread_t feeder_thread;
    pthread_t person_threads[NUM_PEOPLE];
    
    srand(time(NULL)); // Seed random number generator 

    // Create the feeder thread (Person 0 is designated as feeder initialy)
    pthread_create(&feeder_thread, NULL, feeder_loop_func, &table);

    // Create the person threads
    for (int i = 0; i < NUM_PEOPLE; ++i) {
        PersonArgs* args = (PersonArgs*)malloc(sizeof(PersonArgs));
        if (!args) { perror("Failed to allocate person args"); exit(EXIT_FAILURE); }
        args->id = i;
        args->rounds = rounds;
        args->table = &table;
        args->think_time_min = 2; args->think_time_max = 4;
        args->eat_time_min = 2; args->eat_time_max = 3;
        pthread_create(&person_threads[i], NULL, person_thread_func, args);
    }

    // Wait for all person threads to complete their rounds
    for (int i = 0; i < NUM_PEOPLE; ++i) {
        pthread_join(person_threads[i], NULL);
    }

    // Signal the feeder thread to terminate
    pthread_mutex_lock(&table.mtx);
    table.terminate_feeder = true;
    pthread_cond_signal(&table.cv_feeder_terminate); // Wake feeder to check flag
    pthread_mutex_unlock(&table.mtx);

    pthread_join(feeder_thread, NULL); // Wait for feeder thread to terminate

    printf("Simulation finished.\n");
    table_destroy(&table);

    return 0;
}
