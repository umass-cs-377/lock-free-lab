#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

int NUM_THREADS = 2;
int NUM_ITERS   = 1000;
int ARRAY_SIZE  = 10000;

int *shared_array;
pthread_mutex_t mtx;
atomic_int spin_flag = 0; // 0 = unlocked, 1 = locked

double now_sec() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

void do_work(int *arr) {
    for (int i = 0; i < ARRAY_SIZE; i++) {
        arr[i] += 1;
    }
}

void *with_mutex(void *arg) {
    for (int i = 0; i < NUM_ITERS; i++) {
        pthread_mutex_lock(&mtx);
        do_work(shared_array);
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

void *with_spin(void *arg) {
    for (int i = 0; i < NUM_ITERS; i++) {
        // while (atomic_exchange(&spin_flag, 1) == 1)
        //     ; // busy wait
        int expected = 0;
        while (!atomic_compare_exchange_weak(&spin_flag, &expected, 1)) {
            expected = 0; // reset expectation before retrying
        }
        do_work(shared_array);
        atomic_store(&spin_flag, 0);
    }
    return NULL;
}

double run(void *(*func)(void *), const char *label) {
    pthread_t *threads = malloc(sizeof(pthread_t) * NUM_THREADS);
    double start = now_sec();

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, func, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    double end = now_sec();
    printf("%s took %.6f sec\n", label, end - start);

    free(threads);
    return end - start;
}

int main(int argc, char *argv[]) {
    if (argc > 1) NUM_THREADS = atoi(argv[1]);
    if (argc > 2) NUM_ITERS   = atoi(argv[2]);
    if (argc > 3) ARRAY_SIZE  = atoi(argv[3]);

    shared_array = calloc(ARRAY_SIZE, sizeof(int));
    pthread_mutex_init(&mtx, NULL);

    printf("Running with %d threads × %d iterations each, array size = %d\n",
           NUM_THREADS, NUM_ITERS, ARRAY_SIZE);

    run(with_mutex, "Mutex-based");
    run(with_spin,  "Spin/CAS-based");

    pthread_mutex_destroy(&mtx);
    free(shared_array);
    return 0;
}
