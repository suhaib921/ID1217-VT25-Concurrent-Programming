/**
 * @file diff.c
 * @brief A simplified, multithreaded implementation of the Linux 'diff' command.
 *
 * This program compares two text files line by line and reports the differences.
 * It uses a multithreaded approach where two separate threads read the files
 * concurrently, and the main thread consumes the lines and performs the comparison.
 *
 * - Two reader threads each read a file and push lines into a thread-safe queue.
 * - The main thread pulls one line from each queue, compares them, and prints
 *   both lines if they differ.
 * - If one file is longer, the extra lines are printed.
 *
 * This structure demonstrates a parallel file reading pattern, although the
 * comparison itself remains sequential.
 *
 * To compile:
 *   gcc -o diff diff.c -lpthread
 *
 * To run:
 *   ./diff <file1> <file2>
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_CAPACITY 10
#define MAX_LINE_LENGTH 1024

// A thread-safe queue for passing lines from reader threads to the main thread.
typedef struct {
    char** buffer;
    int capacity;
    int size;
    int head;
    int tail;
    int eof;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} LineQueue;

// Arguments for reader threads
typedef struct {
    FILE* file;
    LineQueue* queue;
} ReaderArgs;


// Function Prototypes
LineQueue* queue_create(int capacity);
void queue_destroy(LineQueue* q);
void queue_push(LineQueue* q, const char* line);
char* queue_pop(LineQueue* q);
void queue_signal_eof(LineQueue* q);
void* reader_thread_func(void* arg);


/**
 * @brief Creates and initializes a new thread-safe line queue.
 */
LineQueue* queue_create(int capacity) {
    LineQueue* q = (LineQueue*)malloc(sizeof(LineQueue));
    q->buffer = (char**)malloc(sizeof(char*) * capacity);
    q->capacity = capacity;
    q->size = 0;
    q->head = 0;
    q->tail = 0;
    q->eof = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return q;
}

/**
 * @brief Destroys a line queue, freeing all allocated memory.
 */
void queue_destroy(LineQueue* q) {
    for (int i = 0; i < q->size; i++) {
        free(q->buffer[(q->head + i) % q->capacity]);
    }
    free(q->buffer);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q);
}

/**
 * @brief Pushes a line onto the queue. Blocks if the queue is full.
 */
void queue_push(LineQueue* q, const char* line) {
    pthread_mutex_lock(&q->mutex);
    while (q->size == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->buffer[q->tail] = strdup(line);
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

/**
 * @brief Pops a line from the queue. Blocks if the queue is empty.
 * @return The line string, or NULL if EOF is reached and the queue is empty.
 */
char* queue_pop(LineQueue* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->size == 0 && !q->eof) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    if (q->size == 0 && q->eof) {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
    char* line = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return line;
}

/**
 * @brief Signals that there are no more lines to be added to the queue.
 */
void queue_signal_eof(LineQueue* q) {
    pthread_mutex_lock(&q->mutex);
    q->eof = 1;
    pthread_cond_broadcast(&q->not_empty); // Wake up any waiting consumers
    pthread_mutex_unlock(&q->mutex);
}

/**
 * @brief The thread function for reading a file.
 * Reads a file line by line and pushes each line onto a shared queue.
 */
void* reader_thread_func(void* arg) {
    ReaderArgs* r_args = (ReaderArgs*)arg;
    FILE* file = r_args->file;
    LineQueue* queue = r_args->queue;
    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file)) {
        queue_push(queue, line);
    }

    queue_signal_eof(queue);
    fclose(file);
    free(r_args);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file1> <file2>\n", argv[0]);
        return 1;
    }

    FILE* file1 = fopen(argv[1], "r");
    if (!file1) {
        perror(argv[1]);
        return 1;
    }

    FILE* file2 = fopen(argv[2], "r");
    if (!file2) {
        perror(argv[2]);
        fclose(file1);
        return 1;
    }

    LineQueue* q1 = queue_create(QUEUE_CAPACITY);
    LineQueue* q2 = queue_create(QUEUE_CAPACITY);

    pthread_t thread1, thread2;
    ReaderArgs *args1 = malloc(sizeof(ReaderArgs));
    args1->file = file1;
    args1->queue = q1;
    ReaderArgs *args2 = malloc(sizeof(ReaderArgs));
    args2->file = file2;
    args2->queue = q2;

    pthread_create(&thread1, NULL, reader_thread_func, args1);
    pthread_create(&thread2, NULL, reader_thread_func, args2);

    int line_num = 1;
    while (1) {
        char* line1 = queue_pop(q1);
        char* line2 = queue_pop(q2);

        if (line1 == NULL && line2 == NULL) {
            break; // Both files are finished
        }

        if (line1 && line2) {
            // Both files have lines, compare them
            if (strcmp(line1, line2) != 0) {
                printf("--- Line %d ---\n", line_num);
                printf("< %s", line1);
                printf("> %s", line2);
                printf("--------------\n");
            }
        } else if (line1) {
            // File2 is shorter, print remaining lines from file1
            printf("--- Extra line in %s (Line %d) ---\n", argv[1], line_num);
            printf("< %s", line1);
            printf("--------------\n");
        } else { // line2 must be non-null
            // File1 is shorter, print remaining lines from file2
            printf("--- Extra line in %s (Line %d) ---\n", argv[2], line_num);
            printf("> %s", line2);
            printf("--------------\n");
        }
        
        free(line1);
        free(line2);
        line_num++;
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    queue_destroy(q1);
    queue_destroy(q2);

    printf("\nComparison finished.\n");

    return 0;
}
