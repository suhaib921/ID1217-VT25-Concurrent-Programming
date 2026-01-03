/* Palindromes and Semordnilaps finder using pthreads */

#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h> // For strlen, strcmp, strcpy
#include <time.h>   // For time, srand
#include <sys/time.h> // For gettimeofday

#define MAX_WORD_LEN 100 // Maximum length of a word
#define MAXWORKERS 10    // Maximum number of workers
#define MAX_WORDS_DICT 500000 // Max words in a large dictionary, adjust as needed
#define INITIAL_RESULTS_CAPACITY 2000 // Initial capacity for palindrome/semordnilap lists per worker

// Structure to hold results from each worker
typedef struct {
    int palindrome_count;
    int semordnilap_count;
    char **palindromes;     // Dynamically allocated array of strings
    char **semordnilaps;    // Dynamically allocated array of strings
    int palindromes_capacity; // Current capacity of palindromes array
    int semordnilaps_capacity; // Current capacity of semordnilaps array
} WorkerResult;

// Global variables
double start_time, end_time;    // start and end times
int numWorkers;                 // number of workers

char **dictionary;              // Array of strings to store dictionary words
int num_words_in_dict;          // Total number of words in the dictionary

WorkerResult worker_results[MAXWORKERS]; // Results for each worker

// --- String utility functions ---
// Function to reverse a string
char* reverse_word(const char* word) {
    int len = strlen(word);
    char* reversed = (char*)malloc(len + 1);
    if (!reversed) {
        perror("Failed to allocate memory for reversed word");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < len; i++) {
        reversed[i] = word[len - 1 - i];
    }
    reversed[len] = '\0';
    return reversed;
}

// Function to check if a word is a palindrome
bool is_palindrome(const char* word) {
    int len = strlen(word);
    for (int i = 0; i < len / 2; i++) {
        if (word[i] != word[len - 1 - i]) {
            return false;
        }
    }
    return true;
}

// --- Dictionary management ---
// Comparison function for qsort and bsearch
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

// Function to load dictionary from file
int load_dictionary(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open dictionary file");
        return 0;
    }

    // Allocate initial capacity for dictionary
    dictionary = (char**)malloc(INITIAL_RESULTS_CAPACITY * sizeof(char*));
    if (!dictionary) {
        perror("Failed to allocate dictionary array");
        fclose(file);
        return 0;
    }
    int current_dict_capacity = INITIAL_RESULTS_CAPACITY;


    char buffer[MAX_WORD_LEN];
    num_words_in_dict = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove newline character if present
        buffer[strcspn(buffer, "\n")] = 0;
        int len = strlen(buffer);
        if (len > 0) { // Only store non-empty lines
            if (num_words_in_dict >= current_dict_capacity) {
                current_dict_capacity *= 2;
                dictionary = (char**)realloc(dictionary, current_dict_capacity * sizeof(char*));
                if (!dictionary) {
                    perror("Failed to reallocate dictionary array");
                    // Clean up and exit if realloc fails
                    for (int i = 0; i < num_words_in_dict; ++i) free(dictionary[i]);
                    fclose(file);
                    exit(EXIT_FAILURE);
                }
            }

            dictionary[num_words_in_dict] = (char*)malloc(len + 1);
            if (!dictionary[num_words_in_dict]) {
                perror("Failed to allocate word string");
                // Clean up previously allocated words
                for (int i = 0; i < num_words_in_dict; ++i) free(dictionary[i]);
                free(dictionary);
                fclose(file);
                return 0;
            }
            strcpy(dictionary[num_words_in_dict], buffer);
            num_words_in_dict++;
        }
    }
    fclose(file);

    // Reallocate dictionary to exact size
    dictionary = (char**)realloc(dictionary, num_words_in_dict * sizeof(char*));
    if (!dictionary && num_words_in_dict > 0) { 
         perror("Failed to reallocate dictionary to exact size");
         exit(EXIT_FAILURE);
    }

    // Sort the dictionary for efficient bsearch lookup
    qsort(dictionary, num_words_in_dict, sizeof(char*), compare_strings);

    return num_words_in_dict;
}

// Function to free dictionary memory
void free_dictionary() {
    for (int i = 0; i < num_words_in_dict; ++i) {
        free(dictionary[i]);
    }
    free(dictionary);
}

/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;
    if (!initialized) {
        gettimeofday(&start, NULL);
        initialized = true;
    }
    gettimeofday(&end, NULL);
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

// Worker function prototype
void *Worker(void *);

int main(int argc, char *argv[]) {
    long l; // use long in case of a 64-bit system
    pthread_attr_t attr;
    pthread_t workerid[MAXWORKERS];

    /* set global thread attributes */
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    /* read command line args */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <dictionary_file> <numWorkers>\n", argv[0]);
        exit(1);
    }

    const char* dict_filename = argv[1];
    numWorkers = atoi(argv[2]);

    if (numWorkers <= 0 || numWorkers > MAXWORKERS) {
        fprintf(stderr, "Number of workers must be between 1 and %d (MAXWORKERS).\n", MAXWORKERS);
        exit(1);
    }

    // Input Phase (Sequential)
    printf("Loading dictionary from '%s'வைக்...");
    num_words_in_dict = load_dictionary(dict_filename);
    if (num_words_in_dict == 0) {
        fprintf(stderr, "Failed to load dictionary or dictionary is empty.\n");
        exit(1);
    }
    printf("Loaded %d words.\n", num_words_in_dict);

    // Initialize worker_results and their dynamic arrays
    for (int i = 0; i < numWorkers; ++i) {
        worker_results[i].palindrome_count = 0;
        worker_results[i].semordnilap_count = 0;
        worker_results[i].palindromes_capacity = INITIAL_RESULTS_CAPACITY;
        worker_results[i].semordnilaps_capacity = INITIAL_RESULTS_CAPACITY;
        worker_results[i].palindromes = (char**)malloc(worker_results[i].palindromes_capacity * sizeof(char*));
        worker_results[i].semordnilaps = (char**)malloc(worker_results[i].semordnilaps_capacity * sizeof(char*));
        if (!worker_results[i].palindromes || !worker_results[i].semordnilaps) {
            perror("Failed to allocate worker result arrays");
            exit(EXIT_FAILURE);
        }
    }

    printf("Starting parallel computation with %d workers...\n", numWorkers);
    /* do the parallel work: create the workers */
    start_time = read_timer(); // Start timer after initialization and before thread creation

    for (l = 0; l < numWorkers; l++) {
        pthread_create(&workerid[l], &attr, Worker, (void *)l);
    }

    // Wait for all workers to finish
    for (l = 0; l < numWorkers; l++) {
        pthread_join(workerid[l], NULL);
    }

    // Output Phase (Sequential)
    end_time = read_timer(); // End timer after all workers complete

    int total_palindromes = 0;
    int total_semordnilaps = 0;

    FILE *output_file = fopen("results.txt", "w");
    if (!output_file) {
        perror("Failed to open results.txt for writing");
        exit(EXIT_FAILURE);
    }

    fprintf(output_file, "--- Palindromes ---\n");
    for (int i = 0; i < numWorkers; ++i) {
        total_palindromes += worker_results[i].palindrome_count;
        for (int j = 0; j < worker_results[i].palindrome_count; ++j) {
            fprintf(output_file, "%s\n", worker_results[i].palindromes[j]);
        }
    }

    fprintf(output_file, "\n--- Semordnilaps ---\n");
    for (int i = 0; i < numWorkers; ++i) {
        total_semordnilaps += worker_results[i].semordnilap_count;
        for (int j = 0; j < worker_results[i].semordnilap_count; ++j) {
            fprintf(output_file, "%s\n", worker_results[i].semordnilaps[j]);
        }
    }
    fclose(output_file);

    printf("\n--- Summary ---\n");
    printf("Total Palindromes found: %d\n", total_palindromes);
    printf("Total Semordnilaps found: %d\n", total_semordnilaps);
    printf("Execution time: %g sec\n", end_time - start_time);

    for (int i = 0; i < numWorkers; ++i) {
        printf("Worker %d: Palindromes=%d, Semordnilaps=%d\n",
               i, worker_results[i].palindrome_count, worker_results[i].semordnilap_count);
        // Free worker result arrays
        for (int j = 0; j < worker_results[i].palindrome_count; ++j) free(worker_results[i].palindromes[j]);
        free(worker_results[i].palindromes);
        for (int j = 0; j < worker_results[i].semordnilap_count; ++j) free(worker_results[i].semordnilaps[j]);
        free(worker_results[i].semordnilaps);
    }

    free_dictionary();
    pthread_attr_destroy(&attr); // Destroy attributes

    return 0; // Exit main thread cleanly
}

/* Each worker finds palindromes and semordnilaps in its assigned portion of the dictionary */
void *Worker(void *arg) {
    long myid = (long)arg;
    int start_index;
    int end_index;
    int words_per_worker;

    words_per_worker = num_words_in_dict / numWorkers;
    start_index = myid * words_per_worker;
    end_index = (myid == numWorkers - 1) ? num_words_in_dict : (start_index + words_per_worker);

    // Pointers for current worker's result lists
    WorkerResult* my_results = &worker_results[myid];

    for (int i = start_index; i < end_index; ++i) {
        const char* current_word = dictionary[i];
        if (strlen(current_word) == 0) continue; // Skip empty strings

        if (is_palindrome(current_word)) {
            // Add to palindromes list
            if (my_results->palindrome_count >= my_results->palindromes_capacity) {
                my_results->palindromes_capacity *= 2;
                my_results->palindromes = (char**)realloc(my_results->palindromes, my_results->palindromes_capacity * sizeof(char*));
                if (!my_results->palindromes) {
                    perror("Failed to reallocate palindromes array");
                    exit(EXIT_FAILURE);
                }
            }
            my_results->palindromes[my_results->palindrome_count] = strdup(current_word);
            my_results->palindrome_count++;
        } else {
            // Check for semordnilap
            char* reversed_word = reverse_word(current_word);
            // Use bsearch for efficient lookup in the sorted dictionary
            // bsearch expects a pointer to the key.
            char** found_reversed_ptr = (char**)bsearch(&reversed_word, dictionary, num_words_in_dict, sizeof(char*), compare_strings);

            if (found_reversed_ptr) { // If reversed word is found in dictionary
                const char* found_word = *found_reversed_ptr;
                // It's a semordnilap pair (original word and its reverse are different valid words)
                if (strcmp(current_word, found_word) != 0) {
                    // Add to semordnilaps list
                    if (my_results->semordnilap_count >= my_results->semordnilaps_capacity) {
                        my_results->semordnilaps_capacity *= 2;
                        my_results->semordnilaps = (char**)realloc(my_results->semordnilaps, my_results->semordnilaps_capacity * sizeof(char*));
                        if (!my_results->semordnilaps) {
                            perror("Failed to reallocate semordnilaps array");
                            exit(EXIT_FAILURE);
                        }
                    }
                    // Store the original word as the semordnilap entry
                    my_results->semordnilaps[my_results->semordnilap_count] = strdup(current_word);
                    my_results->semordnilap_count++;
                }
            }
            free(reversed_word); // Free memory allocated for reversed word
        }
    }

    return NULL;
}