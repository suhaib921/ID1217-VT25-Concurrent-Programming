#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#define TASK_CUTOFF 100  // Adjusted cutoff for switching to sequential QuickSort
#define NUM_ITERATIONS 10  

// Function prototypes
void swap(int* a, int* b);
int partition(int arr[], int low, int high);
int medianOfThree(int arr[], int low, int high);
void parallelQuicksort(int arr[], int low, int high);
void sequentialQuicksort(int arr[], int low, int high);
double measureSequentialTime(void (*sortFunction)(int[], int, int), int arr[], int size);
double measureParallelTime(void (*sortFunction)(int[], int, int), int arr[], int size);
void generateRandomData(int data[], int size);
double calculateMedian(double times[], int count);

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int medianOfThree(int arr[], int low, int high) {
    int mid = low + (high - low) / 2;
    if (arr[low] > arr[mid])
        swap(&arr[low], &arr[mid]);
    if (arr[low] > arr[high])
        swap(&arr[low], &arr[high]);
    if (arr[mid] > arr[high])
        swap(&arr[mid], &arr[high]);
    return mid;
}

int partition(int arr[], int low, int high) {
    int pivotIndex = medianOfThree(arr, low, high); 
    swap(&arr[pivotIndex], &arr[high]); // Move pivot to the end
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j <= high - 1; j++) {
        if (arr[j] <= pivot) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return i + 1;
}

// Parallel Quicksort function using OpenMP tasks
void parallelQuicksort(int arr[], int low, int high) {
    if (low < high) {
        int pivotIndex = partition(arr, low, high);
        if (high - low > TASK_CUTOFF) {
            #pragma omp task firstprivate(arr, low, pivotIndex) final((pivotIndex - 1 - low) <= TASK_CUTOFF)
            parallelQuicksort(arr, low, pivotIndex - 1);

            #pragma omp task firstprivate(arr, pivotIndex, high) final((high - pivotIndex - 1) <= TASK_CUTOFF)
            parallelQuicksort(arr, pivotIndex + 1, high);

            #pragma omp taskwait
        } else {
            sequentialQuicksort(arr, low, pivotIndex - 1);
            sequentialQuicksort(arr, pivotIndex + 1, high);
        }
    }
}

// Sequential Quicksort function for comparison
void sequentialQuicksort(int arr[], int low, int high) {
    if (low < high) {
        int pivotIndex = partition(arr, low, high);
        sequentialQuicksort(arr, low, pivotIndex - 1);
        sequentialQuicksort(arr, pivotIndex + 1, high);
    }
}

void generateRandomData(int data[], int size) {
    for (int i = 0; i < size; ++i) {
        data[i] = rand() % 10000; 
    }
}

double measureSequentialTime(void (*sortFunction)(int[], int, int), int arr[], int size) {
    double start_time = omp_get_wtime();
    sortFunction(arr, 0, size - 1);
    double end_time = omp_get_wtime();
    return end_time - start_time;
}

double measureParallelTime(void (*sortFunction)(int[], int, int), int arr[], int size) {
    double start_time = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single nowait
        {
            sortFunction(arr, 0, size - 1);
        }
    }
    double end_time = omp_get_wtime();
    return end_time - start_time;
}

// Function to calculate the median of an array of times (using bubble sort for simplicity)
double calculateMedian(double times[], int count) {
    if (count == 0) return 0.0;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (times[j] > times[j + 1]) {
                double temp = times[j];
                times[j] = times[j + 1];
                times[j + 1] = temp;
            }
        }
    }
    return times[count / 2]; // Median is the middle value
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num_threads>\n", argv[0]);
        return 1;
    }

    int numThreads = atoi(argv[1]);   // Number of threads
    int sizes[] = {100000, 200000, 300000, 400000, 500000, 600000}; 
    int numSizes = sizeof(sizes) / sizeof(sizes[0]);

    // Set number of OpenMP threads
    omp_set_num_threads(numThreads);

    // Initialize random seed (do this once)
    srand(time(0));

    printf("Workers: %d\n", numThreads);
    printf("Array Size | Sequential Time (s) | Parallel Time (s) | Speedup\n");
    printf("--------------------------------------------------------------\n");

    for (int i = 0; i < numSizes; i++) {
        int size = sizes[i];
        double seqTimes[NUM_ITERATIONS];
        double parTimes[NUM_ITERATIONS];

        // Allocate memory once for all iterations
        int* data = (int*)malloc(size * sizeof(int));
        int* dataSeq = (int*)malloc(size * sizeof(int));
        int* dataPar = (int*)malloc(size * sizeof(int));

        if (data == NULL || dataSeq == NULL || dataPar == NULL) {
            fprintf(stderr, "Memory allocation failed!\n");
            return 1;
        }

        generateRandomData(data, size);

        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
            // Copy data for sequential and parallel sorting
            for (int j = 0; j < size; j++) {
                dataSeq[j] = data[j];
                dataPar[j] = data[j];
            }

            seqTimes[iter] = measureSequentialTime(sequentialQuicksort, dataSeq, size);

            parTimes[iter] = measureParallelTime(parallelQuicksort, dataPar, size);
        }

        // Calculate median times
        double seqTime = calculateMedian(seqTimes, NUM_ITERATIONS);
        double parTime = calculateMedian(parTimes, NUM_ITERATIONS);

        double speedup = seqTime / parTime;

        printf("%10d | %19.3f | %17.3f | %7.1f\n", size, seqTime, parTime, speedup);

        // Free allocated memory
        free(data);
        free(dataSeq);
        free(dataPar);
    }

    return 0;
}
