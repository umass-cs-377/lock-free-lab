#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

int NUM_THREADS = 8;
int NUM_ITERS   = 1000000;

pthread_mutex_t lock;
int counter_mutex = 0;
int counter_cas   = 0;

double now_sec() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* ---------- Mutex version ---------- */
void *increment_mutex(void *arg) {
    for (int i = 0; i < NUM_ITERS; i++) {
        pthread_mutex_lock(&lock);
        counter_mutex++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* ---------- CAS version ---------- */
void *increment_cas(void *arg) {
    for (int i = 0; i < NUM_ITERS; i++) {
        int old = counter_cas;
        while (!atomic_compare_exchange_weak(&counter_cas, &old, old + 1)) {
            // retry until successful
        }
    }
    return NULL;
}

/* ---------- Runner ---------- */
double run(void *(*func)(void *), const char *label) {
    pthread_t *threads = malloc(sizeof(pthread_t) * NUM_THREADS);
    double start = now_sec();

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, func, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    double end = now_sec();
    printf("%-20s took %.6f sec\n", label, end - start);
    free(threads);
    return end - start;
}

int main(int argc, char *argv[]) {
    if (argc > 1) NUM_THREADS = atoi(argv[1]);
    if (argc > 2) NUM_ITERS   = atoi(argv[2]);
    pthread_mutex_init(&lock, NULL);

    printf("Threads: %d, Iterations per thread: %d\n\n", NUM_THREADS, NUM_ITERS);

    run(increment_mutex, "Mutex");
    long expected = (long)NUM_THREADS * NUM_ITERS;
    printf("  Final counter: %d\n\n", counter_mutex);

    run(increment_cas, "CAS (int + weak)");
    printf("  Final counter: %d\n\n", counter_cas);
    printf("Expected value: %ld\n", expected);

    pthread_mutex_destroy(&lock);
    return 0;
}
