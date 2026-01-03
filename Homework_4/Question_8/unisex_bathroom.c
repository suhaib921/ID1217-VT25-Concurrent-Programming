#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // For usleep
#include <time.h>   // For srand, time
#include <sys/time.h> // For gettimeofday

// Enum for gender
typedef enum { MAN, WOMAN, NONE_GENDER } Gender;

// Monitor for the Unisex Bathroom
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t men_queue;   // Men wait here
    pthread_cond_t women_queue; // Women wait here

    int men_inside;
    int women_inside;
    int men_waiting;
    int women_waiting;
    Gender last_gender; // To implement fairness: remembers which gender last used the bathroom
} UnisexBathroom;

// Function to get current timestamp in milliseconds
long long get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + (long long)tv.tv_usec / 1000;
}

// Function to initialize the UnisexBathroom monitor
void bathroom_init(UnisexBathroom* ub) {
    pthread_mutex_init(&ub->mtx, NULL);
    pthread_cond_init(&ub->men_queue, NULL);
    pthread_cond_init(&ub->women_queue, NULL);

    ub->men_inside = 0;
    ub->women_inside = 0;
    ub->men_waiting = 0;
    ub->women_waiting = 0;
    ub->last_gender = NONE_GENDER; // No one has used it yet
    printf("[%lld] Unisex Bathroom initialized.\n", get_timestamp_ms());
}

// Function to destroy bathroom resources
void bathroom_destroy(UnisexBathroom* ub) {
    pthread_mutex_destroy(&ub->mtx);
    pthread_cond_destroy(&ub->men_queue);
    pthread_cond_destroy(&ub->women_queue);
}

// Helper function to check if a person can enter the bathroom based on fairness
bool can_enter(UnisexBathroom* ub, Gender gender) {
    // If opposite gender is inside, cannot enter
    if ((gender == MAN && ub->women_inside > 0) || (gender == WOMAN && ub->men_inside > 0)) {
        return false;
    }

    // If bathroom is empty
    if (ub->men_inside == 0 && ub->women_inside == 0) {
        // Apply fairness policy:
        // If there are people waiting of the opposite gender, and the *last* gender
        // that used the bathroom was the same as the current person's gender, then
        // let the opposite gender take priority.
        if (gender == MAN) {
            // Man wants to enter. If Women are waiting AND the last gender was MAN,
            // then Man should wait to give Women a turn.
            return ub->women_waiting == 0 || ub->last_gender == WOMAN;
        } else { // gender == WOMAN
            // Woman wants to enter. If Men are waiting AND the last gender was WOMAN,
            // then Woman should wait to give Men a turn.
            return ub->men_waiting == 0 || ub->last_gender == MAN;
        }
    }

    // If people of the same gender are already inside, new person can enter.
    return true;
}

// Man requests to enter the bathroom
void man_enter(UnisexBathroom* ub, int id) {
    pthread_mutex_lock(&ub->mtx);

    ub->men_waiting++;
    printf("[%lld] Man-%d wants to enter. Waiting: M=%d, W=%d. Inside: M=%d, W=%d.\n",
           get_timestamp_ms(), id, ub->men_waiting, ub->women_waiting, ub->men_inside, ub->women_inside);

    while (!can_enter(ub, MAN)) {
        pthread_cond_wait(&ub->men_queue, &ub->mtx);
    }

    ub->men_waiting--;
    ub->men_inside++;
    printf("[%lld] Man-%d entered. Inside: M=%d, W=%d. Waiting: M=%d, W=%d.\n",
           get_timestamp_ms(), id, ub->men_inside, ub->women_inside, ub->men_waiting, ub->women_waiting);

    pthread_mutex_unlock(&ub->mtx);
}

// Man exits the bathroom
void man_exit(UnisexBathroom* ub, int id) {
    pthread_mutex_lock(&ub->mtx);

    ub->men_inside--;
    printf("[%lld] Man-%d exited. Inside: M=%d, W=%d.\n",
           get_timestamp_ms(), id, ub->men_inside, ub->women_inside);

    if (ub->men_inside == 0 && ub->women_inside == 0) { // Bathroom is empty
        ub->last_gender = MAN; // Remember which gender last used it
        printf("[%lld] Bathroom is empty. Last gender was MAN. Signaling waiting people.\n", get_timestamp_ms());
        
        // Prioritize the opposite gender if people are waiting
        if (ub->women_waiting > 0) {
            pthread_cond_broadcast(&ub->women_queue);
        } else {
            pthread_cond_broadcast(&ub->men_queue); // If no women, let men proceed
        }
    } else if (ub->men_inside > 0) { // Other men still inside, signal them if waiting
         pthread_cond_broadcast(&ub->men_queue);
    }

    pthread_mutex_unlock(&ub->mtx);
}

// Woman requests to enter the bathroom
void woman_enter(UnisexBathroom* ub, int id) {
    pthread_mutex_lock(&ub->mtx);

    ub->women_waiting++;
    printf("[%lld] Woman-%d wants to enter. Waiting: M=%d, W=%d. Inside: M=%d, W=%d.\n",
           get_timestamp_ms(), id, ub->men_waiting, ub->women_waiting, ub->men_inside, ub->women_inside);

    while (!can_enter(ub, WOMAN)) {
        pthread_cond_wait(&ub->women_queue, &ub->mtx);
    }

    ub->women_waiting--;
    ub->women_inside++;
    printf("[%lld] Woman-%d entered. Inside: M=%d, W=%d. Waiting: M=%d, W=%d.\n",
           get_timestamp_ms(), id, ub->men_inside, ub->women_inside, ub->men_waiting, ub->women_waiting);

    pthread_mutex_unlock(&ub->mtx);
}

// Woman exits the bathroom
void woman_exit(UnisexBathroom* ub, int id) {
    pthread_mutex_lock(&ub->mtx);

    ub->women_inside--;
    printf("[%lld] Woman-%d exited. Inside: M=%d, W=%d.\n",
           get_timestamp_ms(), id, ub->men_inside, ub->women_inside);

    if (ub->men_inside == 0 && ub->women_inside == 0) { // Bathroom is empty
        ub->last_gender = WOMAN; // Remember which gender last used it
        printf("[%lld] Bathroom is empty. Last gender was WOMAN. Signaling waiting people.\n", get_timestamp_ms());

        // Prioritize the opposite gender if people are waiting
        if (ub->men_waiting > 0) {
            pthread_cond_broadcast(&ub->men_queue);
        } else {
            pthread_cond_broadcast(&ub->women_queue); // If no men, let women proceed
        }
    } else if (ub->women_inside > 0) { // Other women still inside, signal them if waiting
        pthread_cond_broadcast(&ub->women_queue);
    }

    pthread_mutex_unlock(&ub->mtx);
}

// Person thread arguments
typedef struct {
    int id;
    Gender gender;
    int num_visits;
    UnisexBathroom* bathroom;
    int outside_time_min_ms, outside_time_max_ms;
    int inside_time_min_ms, inside_time_max_ms;
} PersonArgs;

// Man thread function
void* man_func(void* arg) {
    PersonArgs* args = (PersonArgs*)arg;
    int id = args->id;
    int num_visits = args->num_visits;
    UnisexBathroom* bathroom = args->bathroom;

    for (int i = 0; i < num_visits; ++i) {
        // Simulate time outside bathroom
        usleep((rand() % (args->outside_time_max_ms - args->outside_time_min_ms + 1) + args->outside_time_min_ms) * 1000);
        
        man_enter(bathroom, id);
        
        // Simulate time inside bathroom
        usleep((rand() % (args->inside_time_max_ms - args->inside_time_min_ms + 1) + args->inside_time_min_ms) * 1000);
        
        man_exit(bathroom, id);
    }
    printf("[%lld] Man-%d finished all %d visits.\n", get_timestamp_ms(), id, num_visits);
    free(args);
    return NULL;
}

// Woman thread function
void* woman_func(void* arg) {
    PersonArgs* args = (PersonArgs*)arg;
    int id = args->id;
    int num_visits = args->num_visits;
    UnisexBathroom* bathroom = args->bathroom;

    for (int i = 0; i < num_visits; ++i) {
        // Simulate time outside bathroom
        usleep((rand() % (args->outside_time_max_ms - args->outside_time_min_ms + 1) + args->outside_time_min_ms) * 1000);
        
        woman_enter(bathroom, id);
        
        // Simulate time inside bathroom
        usleep((rand() % (args->inside_time_max_ms - args->inside_time_min_ms + 1) + args->inside_time_min_ms) * 1000);
        
        woman_exit(bathroom, id);
    }
    printf("[%lld] Woman-%d finished all %d visits.\n", get_timestamp_ms(), id, num_visits);
    free(args);
    return NULL;
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_visits> <num_men> <num_women>\n", argv[0]);
        return 1;
    }

    int num_visits = atoi(argv[1]);
    int num_men = atoi(argv[2]);
    int num_women = atoi(argv[3]);

    if (num_visits <= 0 || num_men < 0 || num_women < 0) {
        fprintf(stderr, "Invalid arguments: visits must be positive, counts non-negative.\n");
        return 1;
    }

    UnisexBathroom bathroom;
    bathroom_init(&bathroom);

    int total_people = num_men + num_women;
    pthread_t* people_threads = (pthread_t*)malloc(sizeof(pthread_t) * total_people);
    if (!people_threads) {
        perror("Failed to allocate people threads");
        return 1;
    }

    srand(time(NULL)); // Seed random number generator 

    int thread_idx = 0;
    // Create man threads
    for (int i = 0; i < num_men; ++i) {
        PersonArgs* args = (PersonArgs*)malloc(sizeof(PersonArgs));
        if (!args) { perror("Failed to allocate man args"); return 1; }
        args->id = i + 1;
        args->gender = MAN;
        args->num_visits = num_visits;
        args->bathroom = &bathroom;
        args->outside_time_min_ms = 100; args->outside_time_max_ms = 500;
        args->inside_time_min_ms = 50; args->inside_time_max_ms = 200;
        pthread_create(&people_threads[thread_idx++], NULL, man_func, args);
    }

    // Create woman threads
    for (int i = 0; i < num_women; ++i) {
        PersonArgs* args = (PersonArgs*)malloc(sizeof(PersonArgs));
        if (!args) { perror("Failed to allocate woman args"); return 1; }
        args->id = num_men + i + 1;
        args->gender = WOMAN;
        args->num_visits = num_visits;
        args->bathroom = &bathroom;
        args->outside_time_min_ms = 100; args->outside_time_max_ms = 500;
        args->inside_time_min_ms = 50; args->inside_time_max_ms = 200;
        pthread_create(&people_threads[thread_idx++], NULL, woman_func, args);
    }

    // Wait for all people to finish
    for (int i = 0; i < total_people; ++i) {
        pthread_join(people_threads[i], NULL);
    }

    printf("[%lld] Simulation finished.\n", get_timestamp_ms());
    bathroom_destroy(&bathroom);
    free(people_threads);

    return 0;
}
