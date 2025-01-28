#include <pthread.h>
#include <stdlib.h> // dynamic memory allocation
#include <stdio.h> //input output
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#define MAX_THREADS 1000
#define THREAD_CREATION_THRESHOLD 1000  // Threshold for creating new threads


/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;
    if( !initialized )
    {
        gettimeofday( &start, NULL );
        initialized = true;
    }
    gettimeofday( &end, NULL );
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

//stores values for each quicksort task.
typedef struct {
    int *array;
    int left;
    int right;
    int depth;

} ThreadData;

pthread_mutex_t thread_count_lock;
int thread_count = 0;

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int partition(int *array, int left, int right) {

    int pivot = array[right];
        int i = left - 1;
        for (int j = left; j < right; j++) {
            if (array[j] < pivot) {
                i++;
                swap(&array[i], &array[j]);
            }
    
        }
        printf("Paritition %d, %d \n", array[i+1], array[right]);
        swap(&array[i + 1], &array[right]);
        return i + 1;
}
//used when threads are unnecassary or thread cration is restricted
void quickSort(int *array, int left, int right) {
    if (left < right) {

        // call partition function to find Partition Index
        int pivot = partition(array, left, right);

        // Recursively call quickSort() for left and right
        // half based on Partition Index
        quickSort(array, left, pivot - 1);
        quickSort(array, pivot + 1, right);
    }
}

//parallel version of quicksort
void *parallel_quicksort(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int left = data->left;
    int right = data->right;
    int *array = data->array;
    int depth = data->depth;
    long myid = (long) arg;

    
    if (left < right) {
        int pivot_index = partition(array, left, right);

        if(depth<THREAD_CREATION_THRESHOLD && (right -left)>1000)
        {
            int pivot_index = partition(array, left, right);

            pthread_t left_thread, right_thread;
            ThreadData left_data = {array, left, pivot_index - 1, data->depth + 1};
            ThreadData right_data = {array, pivot_index + 1, right, data->depth + 1};

            int create_left = 0, create_right = 0;

            // Check if more threads can be created
            pthread_mutex_lock(&thread_count_lock);
            if (thread_count < MAX_THREADS) {
                thread_count++;
                create_left = 1;
            }
            pthread_mutex_unlock(&thread_count_lock);

            if (create_left) {
                pthread_create(&left_thread, NULL, parallel_quicksort, &left_data);
                parallel_quicksort(&left_data);
            }

            printf("worker %ld (pthread id %lu) has started\n", myid, pthread_self());    

            pthread_mutex_lock(&thread_count_lock);
            if (thread_count < MAX_THREADS) {
                thread_count++;
                create_right = 1;
            }
            pthread_mutex_unlock(&thread_count_lock);

            if (create_right) {
                pthread_create(&right_thread, NULL, parallel_quicksort, &right_data);
            } else {
                parallel_quicksort(&right_data);
            }

            if (create_left) pthread_join(left_thread, NULL);
            if (create_right) pthread_join(right_thread, NULL);
        }
        else{
            quickSort(array,left,pivot_index-1);
            quickSort(array,right,pivot_index-1);
        }
    }

    pthread_mutex_lock(&thread_count_lock);
    thread_count--;
    pthread_mutex_unlock(&thread_count_lock);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <number_of_elements>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n = atoi(argv[1]);
    int *array = malloc(n * sizeof(int));

    // Initialize the array with random values
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        array[i] = rand() % 1000; // Random values between 0 and 999
    }

    printf("Unsorted Array:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");

    pthread_mutex_init(&thread_count_lock, NULL);

    // Measure start time
    double start_time = read_timer();

    // Initialize thread data for the entire array
    ThreadData data = {array, 0, n - 1, 0};
    parallel_quicksort(&data);

    // Measure end time
    double end_time = read_timer();

    pthread_mutex_destroy(&thread_count_lock);

    printf("Sorted Array:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");

    printf("Execution Time: %.6f seconds\n", end_time - start_time);

    free(array);
    return 0;
}
