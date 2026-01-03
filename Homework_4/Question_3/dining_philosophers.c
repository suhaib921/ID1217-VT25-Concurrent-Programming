/**
 * @file dining_philosophers.c
 * @brief Simulates the Dining Philosophers problem with a fair, FCFS monitor
 *        implemented in C using Pthreads.
 *
 * This program implements a solution to the classic Dining Philosophers problem.
 * It includes an advanced fairness mechanism to ensure philosophers get to eat
 * in a First-Come, First-Served (FCFS) order, as required by the extra credit part
 * of the assignment.
 *
 * The `Table` monitor uses a "ticket lock" system:
 * - When a philosopher calls `getforks`, they draw a unique, increasing ticket number.
 * - They can only proceed to eat when their ticket is the next one to be served
 *   AND both of their neighbors are not eating.
 * - This prevents a philosopher from "jumping the queue" even if their forks become
 *   available, ensuring strict FCFS ordering among waiting philosophers.
 *
 * The state of each philosopher (THINKING, HUNGRY, EATING) is tracked. A philosopher
 * waits on their own condition variable if they cannot eat.
 * When forks are released, `test` is called on neighbors to check if they can now eat.
 *
 * To compile:
 *   gcc -o dining_philosophers dining_philosophers.c -lpthread -lm
 *
 * To run:
 *   ./dining_philosophers <rounds> [think_ms] [eat_ms]
 * Example:
 *   ./dining_philosophers 10
 *   ./dining_philosophers 5 100 50
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For usleep
#include <time.h>   // For srand, time

#define NUM_PHILOSOPHERS 5

typedef enum { THINKING, HUNGRY, EATING } State;

// --- Monitor for the Dining Table ---
typedef struct {
    pthread_mutex_t mtx;
    State states[NUM_PHILOSOPHERS];
    pthread_cond_t self_cond[NUM_PHILOSOPHERS]; // Each philosopher waits on their own CV

    // --- FCFS Fairness Implementation ---
    long next_ticket;   // Next available ticket number
    long serving_now;   // Ticket number currently being served
} Table;

// Helper functions for left and right neighbors
int left(int i) { return (i + NUM_PHILOSOPHERS - 1) % NUM_PHILOSOPHERS; }
int right(int i) { return (i + 1) % NUM_PHILOSOPHERS; }

// Forward declaration
void test(Table* table, int id);

/**
 * @brief Initializes the Table monitor.
 */
void table_init(Table* table) {
    pthread_mutex_init(&table->mtx, NULL);
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        table->states[i] = THINKING;
        pthread_cond_init(&table->self_cond[i], NULL);
    }
    table->next_ticket = 0;
    table->serving_now = 0;
}

/**
 * @brief Destroys the Table monitor.
 */
void table_destroy(Table* table) {
    pthread_mutex_destroy(&table->mtx);
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        pthread_cond_destroy(&table->self_cond[i]);
    }
}

/**
 * @brief Checks if philosopher `id` can eat. If so, their state is changed
 * to EATING and their condition variable is signaled. This must be called
 * with the monitor's mutex locked.
 */
void test(Table* table, int id) {
    // The philosopher can eat if they are hungry, their neighbors are not eating.
    if (table->states[id] == HUNGRY &&
        table->states[left(id)] != EATING &&
        table->states[right(id)] != EATING) {
        
        table->states[id] = EATING;
        pthread_cond_signal(&table->self_cond[id]); // Signal this philosopher to eat
    }
}

/**
 * @brief Philosopher `id` requests to get forks.
 */
void getforks(Table* table, int id, long my_ticket) {
    pthread_mutex_lock(&table->mtx);

    table->states[id] = HUNGRY;
    printf("Philosopher %d (Ticket %ld) is hungry.\n", id, my_ticket);

    // Wait until it's my turn to be considered AND my neighbors are not eating
    while (true) {
        // Only call test if it's my turn in the FCFS queue
        if (my_ticket == table->serving_now) {
            test(table, id); // Try to eat
        }
        
        // If I'm eating, break. If not, wait.
        if (table->states[id] == EATING) {
            break;
        }
        pthread_cond_wait(&table->self_cond[id], &table->mtx);
    }
    
    printf("Philosopher %d (Ticket %ld) starts eating.\n", id, my_ticket);
    pthread_mutex_unlock(&table->mtx);
}

/**
 * @brief Philosopher `id` releases forks.
 */
void relforks(Table* table, int id) {
    pthread_mutex_lock(&table->mtx);

    table->states[id] = THINKING;
    printf("Philosopher %d (Ticket %ld) stops eating and starts thinking.\n", id, table->serving_now);

    // It is now the next ticket holder's turn to be considered.
    table->serving_now++;

    // Signal all philosophers to re-evaluate their state
    // (This includes the new `serving_now` philosopher and potentially neighbors)
    for(int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        pthread_cond_signal(&table->self_cond[i]);
    }

    // Test neighbors to see if they can eat now.
    test(table, left(id));
    test(table, right(id));
    
    pthread_mutex_unlock(&table->mtx);
}

/**
 * @brief Gets a unique ticket number for FCFS ordering.
 */
long get_ticket(Table* table) {
    pthread_mutex_lock(&table->mtx);
    long ticket = table->next_ticket++;
    pthread_mutex_unlock(&table->mtx);
    return ticket;
}

// --- Thread Function ---
typedef struct {
    int id;
    int rounds;
    Table* table;
    int think_ms;
    int eat_ms;
} PhilosopherArgs;

void* philosopher_func(void* arg) {
    PhilosopherArgs* args = (PhilosopherArgs*)arg;
    int id = args->id;
    int rounds = args->rounds;
    Table* table = args->table;
    int think_min = args->think_ms / 2;
    int think_max = args->think_ms * 2;
    int eat_min = args->eat_ms / 2;
    int eat_max = args->eat_ms * 2;

    for (int i = 0; i < rounds; ++i) {
        // Think
        int think_time = think_min + (rand() % (think_max - think_min + 1));
        usleep(think_time * 1000); // usleep takes microseconds

        // Get a ticket *before* calling getforks
        long my_ticket = get_ticket(table);
        
        // Get forks and eat
        getforks(table, id, my_ticket);
        int eat_time = eat_min + (rand() % (eat_max - eat_min + 1));
        usleep(eat_time * 1000); // usleep takes microseconds
        relforks(table, id);
    }
    printf("Philosopher %d finished all rounds.\n", id);
    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "USAGE: %s <rounds> [think_ms] [eat_ms]\n", argv[0]);
        return 1;
    }

    int rounds = atoi(argv[1]);
    int think_ms = (argc > 2) ? atoi(argv[2]) : 100;
    int eat_ms = (argc > 3) ? atoi(argv[3]) : 100;

    Table table;
    table_init(&table);

    pthread_t threads[NUM_PHILOSOPHERS];
    
    srand(time(NULL)); // Seed random number generator

    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        PhilosopherArgs* args = (PhilosopherArgs*)malloc(sizeof(PhilosopherArgs));
        if (!args) { perror("Failed to allocate philosopher args"); exit(EXIT_FAILURE); }
        args->id = i;
        args->rounds = rounds;
        args->table = &table;
        args->think_ms = think_ms;
        args->eat_ms = eat_ms;
        pthread_create(&threads[i], NULL, philosopher_func, args);
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("All philosophers have finished their meals.\n");
    table_destroy(&table);

    return 0;
}
