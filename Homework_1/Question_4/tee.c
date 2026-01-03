/**
 * @file tee.c
 * @brief A multithreaded implementation of the Linux 'tee' command using Pthreads.
 *
 * This program reads from standard input and writes to both standard output
 * and a specified file concurrently. It uses three threads and a pipeline
 * architecture with two separate, thread-safe queues.
 *
 * Architecture:
 * 1. A reader thread: Reads chunks from stdin.
 * 2. Two writer threads: One for stdout and one for the output file.
 *
 * Data Flow:
 * - The reader reads a chunk of data and places a copy of it onto two
 *   separate queues: one for the stdout writer and one for the file writer.
 * - Each writer independently dequeues from its own queue and writes to its
 *   destination. This decouples the writers, so a slow file write will not
 *   block a fast stdout write, and vice-versa.
 * - EOF is signaled by placing a special zero-size buffer onto each queue.
 *
 * To compile:
 *   gcc -o tee tee.c -lpthread
 *
 * To run:
 *   ./tee <output_filename>
 *   Then type into stdin. Press Ctrl+D to signal EOF.
 *   Example: ls -l | ./tee file.txt
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096
#define QUEUE_CAPACITY 8

// A single buffer entry in the queue
typedef struct {
    char data[BUFFER_SIZE];
    size_t size;
} Buffer;

// A thread-safe queue for passing buffers
typedef struct {
    Buffer queue[QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    bool eof;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} BufferQueue;

// --- Queue Management Functions ---
void queue_init(BufferQueue* q) {
    q->head = q->tail = q->count = 0;
    q->eof = false;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(BufferQueue* q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

// Enqueues a buffer. Blocks if the queue is full.
void queue_enqueue(BufferQueue* q, Buffer* buf) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == QUEUE_CAPACITY) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->queue[q->tail] = *buf;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

// Dequeues a buffer. Returns false if EOF and queue is empty.
bool queue_dequeue(BufferQueue* q, Buffer* buf) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0 && !q->eof) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if (q->count == 0 && q->eof) {
        pthread_mutex_unlock(&q->mutex);
        return false; // No more data
    }

    *buf = q->queue[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

// Signals that no more data will be enqueued.
void queue_signal_eof(BufferQueue* q) {
    pthread_mutex_lock(&q->mutex);
    q->eof = true;
    pthread_cond_broadcast(&q->not_empty); // Wake up any waiting consumers
    pthread_mutex_unlock(&q->mutex);
}


// --- Global Queues ---
BufferQueue stdout_queue;
BufferQueue file_queue;

// --- Thread Functions ---

/**
 * @brief The reader thread function (Producer).
 * Reads data from stdin and enqueues it for both writer threads.
 */
void* reader_thread_func(void* arg) {
    Buffer buf;
    ssize_t bytes_read;
    while ((bytes_read = read(STDIN_FILENO, buf.data, BUFFER_SIZE)) > 0) {
        buf.size = bytes_read;
        queue_enqueue(&stdout_queue, &buf);
        queue_enqueue(&file_queue, &buf);
    }

    // Signal end-of-file to both writer queues by sending a zero-size buffer
    buf.size = 0;
    queue_enqueue(&stdout_queue, &buf);
    queue_enqueue(&file_queue, &buf);

    return NULL;
}

/**
 * @brief The writer thread function (Consumer).
 * Dequeues buffers and writes them to the specified file descriptor.
 */
void* writer_thread_func(void* arg) {
    BufferQueue* q = (BufferQueue*)arg;
    int fd;

    // Determine output file descriptor based on which queue this is
    if (q == &stdout_queue) {
        fd = STDOUT_FILENO;
    } else {
        fd = *(int*)((void**)arg)[1]; // A bit of casting to get the fd
    }

    Buffer buf;
    while (queue_dequeue(q, &buf) && buf.size > 0) {
        ssize_t total_written = 0;
        while (total_written < buf.size) {
            ssize_t written = write(fd, buf.data + total_written, buf.size - total_written);
            if (written < 0) {
                perror("write");
                return NULL; // Exit thread on error
            }
            total_written += written;
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_filename>\n", argv[0]);
        return 1;
    }

    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("open");
        return 1;
    }

    // Initialize the two queues
    queue_init(&stdout_queue);
    queue_init(&file_queue);

    pthread_t reader_thread, stdout_writer_thread, file_writer_thread;

    // Arguments for the file writer thread
    void* file_writer_args[] = {&file_queue, &file_fd};

    // Create the three threads
    pthread_create(&reader_thread, NULL, reader_thread_func, NULL);
    pthread_create(&stdout_writer_thread, NULL, writer_thread_func, &stdout_queue);
    pthread_create(&file_writer_thread, NULL, writer_thread_func, &file_writer_args);

    // Wait for all threads to complete
    pthread_join(reader_thread, NULL);
    pthread_join(stdout_writer_thread, NULL);
    pthread_join(file_writer_thread, NULL);

    // Clean up resources
    close(file_fd);
    queue_destroy(&stdout_queue);
    queue_destroy(&file_queue);

    printf("\n'tee' command finished.\n");

    return 0;
}
