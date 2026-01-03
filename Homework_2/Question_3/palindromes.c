/**
 * @file palindromes.c
 * @brief Finds all palindromes and semordnilaps in a dictionary file using OpenMP.
 *
 * This program reads a list of words from a dictionary file. It then uses
 * a parallel algorithm with OpenMP to identify two types of special words:
 * - Palindromes: Words that read the same forwards and backward (e.g., "radar").
 * - Semordnilaps: Words that form a different valid word when reversed
 *   (e.g., "draw" and "ward").
 *
 * To achieve efficient lookups (`O(1)` average), the program first builds a
 * hash set of all words. The main parallel loop then iterates through each
 * word. Each thread collects its own findings in a private list to avoid
 * synchronization overhead. These lists are then merged sequentially after the
 * parallel section.
 *
 * To compile:
 *   gcc -o palindromes palindromes.c -fopenmp
 *
 * To run:
 *   export OMP_NUM_THREADS=<number_of_threads>
 *   ./palindromes <dictionary_file> <output_file>
 *   Example:
 *   export OMP_NUM_THREADS=4
 *   ./palindromes /usr/share/dict/words results.txt
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORD_LEN 100
#define MAX_WORDS 500000
#define MAX_THREADS 64

// --- Basic Hash Set Implementation ---
typedef struct Node {
    char* key;
    struct Node* next;
} Node;

typedef struct HashSet {
    Node** buckets;
    int size;
} HashSet;

unsigned long hash_function(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

HashSet* hash_set_create(int size) {
    HashSet* set = (HashSet*)malloc(sizeof(HashSet));
    set->size = size;
    set->buckets = (Node**)calloc(size, sizeof(Node*));
    return set;
}

void hash_set_insert(HashSet* set, const char* key) {
    unsigned long index = hash_function(key) % set->size;
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->key = strdup(key);
    newNode->next = set->buckets[index];
    set->buckets[index] = newNode;
}

int hash_set_contains(HashSet* set, const char* key) {
    unsigned long index = hash_function(key) % set->size;
    Node* current = set->buckets[index];
    while (current) {
        if (strcmp(current->key, key) == 0) return 1;
        current = current->next;
    }
    return 0;
}

void hash_set_destroy(HashSet* set) {
    if (!set) return;
    for (int i = 0; i < set->size; i++) {
        Node* current = set->buckets[i];
        while (current) {
            Node* temp = current;
            current = current->next;
            free(temp->key);
            free(temp);
        }
    }
    free(set->buckets);
    free(set);
}

// --- Main Program Logic ---
typedef struct {
    char** words;
    int count;
    int capacity;
} WordList;

void list_init(WordList* list, int initial_capacity) {
    list->count = 0;
    list->capacity = initial_capacity > 0 ? initial_capacity : 10;
    list->words = (char**)malloc(list->capacity * sizeof(char*));
}

void list_add(WordList* list, char* word) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->words = (char**)realloc(list->words, list->capacity * sizeof(char*));
    }
    list->words[list->count++] = word;
}

void list_destroy(WordList* list) {
    if (list && list->words) {
        free(list->words);
        list->words = NULL;
    }
}

// Global dictionary and hash set
char* all_words[MAX_WORDS];
int all_words_count = 0;
HashSet* word_set = NULL;

void reverse_string(const char* src, char* dest) {
    int len = strlen(src);
    for (int i = 0; i < len; i++) {
        dest[i] = src[len - 1 - i];
    }
    dest[len] = '\0';
}

int read_dictionary(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return 0;
    
    char buffer[MAX_WORD_LEN];
    while (fgets(buffer, sizeof(buffer), file) && all_words_count < MAX_WORDS) {
        buffer[strcspn(buffer, "\n\r")] = 0;
        for(int i = 0; buffer[i]; i++) buffer[i] = tolower(buffer[i]);
        if (strlen(buffer) > 0) {
            all_words[all_words_count++] = strdup(buffer);
        }
    }
    fclose(file);
    return 1;
}

void cleanup() {
    hash_set_destroy(word_set);
    for(int i = 0; i < all_words_count; i++) {
        if(all_words[i]) free(all_words[i]);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dictionary_file> <output_file>\n", argv[0]);
        return 1;
    }
    
    atexit(cleanup);

    // --- Sequential Part: Input ---
    if (!read_dictionary(argv[1])) {
        perror("fopen dictionary");
        return 1;
    }
    printf("Read %d words.\n", all_words_count);

    word_set = hash_set_create(all_words_count * 2);
    for (int i = 0; i < all_words_count; i++) {
        hash_set_insert(word_set, all_words[i]);
    }

    // Per-thread result lists
    WordList per_thread_palindromes[MAX_THREADS];
    WordList per_thread_semordnilaps[MAX_THREADS];

    // --- Parallel Part: Computation ---
    printf("Finding palindromes and semordnilaps...\n");
    double start_time = omp_get_wtime();

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        list_init(&per_thread_palindromes[tid], 100);
        list_init(&per_thread_semordnilaps[tid], 100);

        #pragma omp for schedule(dynamic, 100)
        for (int i = 0; i < all_words_count; i++) {
            char reversed_word[MAX_WORD_LEN];
            reverse_string(all_words[i], reversed_word);

            if (strcmp(all_words[i], reversed_word) == 0) {
                list_add(&per_thread_palindromes[tid], all_words[i]);
            } else if (strcmp(all_words[i], reversed_word) < 0 && hash_set_contains(word_set, reversed_word)) {
                list_add(&per_thread_semordnilaps[tid], all_words[i]);
            }
        }
    }

    double end_time = omp_get_wtime();
    printf("Computation finished in %f seconds.\n", end_time - start_time);

    // --- Sequential Part: Output ---
    FILE* outfile = fopen(argv[2], "w");
    if (!outfile) {
        perror("fopen output");
        return 1;
    }

    int total_palindromes = 0;
    int total_semordnilaps = 0;
    int max_threads = omp_get_max_threads();

    for (int i = 0; i < max_threads; i++) {
        total_palindromes += per_thread_palindromes[i].count;
        total_semordnilaps += per_thread_semordnilaps[i].count;
    }

    fprintf(outfile, "--- PALINDROMES (%d) ---\n", total_palindromes);
    for (int i = 0; i < max_threads; i++) {
        for (int j = 0; j < per_thread_palindromes[i].count; j++) {
            fprintf(outfile, "%s\n", per_thread_palindromes[i].words[j]);
        }
    }

    fprintf(outfile, "\n--- SEMORDNILAPS (%d pairs) ---\n", total_semordnilaps);
    for (int i = 0; i < max_threads; i++) {
        for (int j = 0; j < per_thread_semordnilaps[i].count; j++) {
            char reversed_word[MAX_WORD_LEN];
            reverse_string(per_thread_semordnilaps[i].words[j], reversed_word);
            fprintf(outfile, "%s <--> %s\n", per_thread_semordnilaps[i].words[j], reversed_word);
        }
    }
    
    fclose(outfile);
    printf("Done. Wrote %d palindromes and %d semordnilap pairs to %s.\n", total_palindromes, total_semordnilaps, argv[2]);

    // --- Local Cleanup ---
    for(int i = 0; i < max_threads; i++) {
        list_destroy(&per_thread_palindromes[i]);
        list_destroy(&per_thread_semordnilaps[i]);
    }

    return 0;
}
