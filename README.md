# Lab: Lock-free concurrency

# Purpose

This lab is designed to cover lock-free concurrency in C++. We will implement two synchronization methods, Mutex-based locking ****and CAS-based busy waiting. Then we will compare their performance under different workloads and thread counts to see when and why which one is better. 

Please make sure that all of your answers to questions in these labs come from work done on the Edlab environment - otherwise, they may receive inconsistent results and will not receive points.

The TA present in your lab will give a brief explanation of the various parts of this lab, but you are expected to answer all questions by yourself. Please raise your hand if you have any questions during the lab section. Questions and Parts have a number of points marked next to them to signify their weight in this lab’s final grade.

Please read through this lab and follow the instructions. After you do that, visit Gradescope and complete the questions associated with this lab by the assigned due date.

## Setup

Once you have logged in to Edlab, you can clone this repo using

```bash
git clone https://github.com/umass-cs-377/lock-free-lab.git
```

Then you can use `cd` to open the directory you just cloned:

```bash
cd lock-free-lab
```

This repo includes a Makefile that allows you to locally compile and run all the sample code listed in this tutorial. You can compile them by running `make`. Feel free to modify the source files yourself. After making changes, you can run `make` again to build new binaries from your modified files. You can also use `make clean` to remove all the built files; this command is usually used when something went wrong during the compilation, so that you can start fresh.

# Part 1: Overview

A **lock (mutex)** is a synchronization mechanism that ensures that only one thread can access a shared resource at a time. When a thread acquires a lock, any other thread attempting to access the same resource must wait until the lock is released. This guarantees mutual exclusion and prevents data races. Locks are typically implemented by the operating system using kernel support. When contention occurs, the OS can put waiting threads to sleep and wake them later, which prevents unnecessary CPU use.

Locks are conceptually simple and are appropriate when the protected work inside the critical section is relatively large or involves multiple shared variables. However, excessive locking can cause blocking and context-switching overhead, particularly when many threads compete for the same resource. Misuse of locks can also lead to deadlocks if threads acquire locks in conflicting orders.

In contrast, **lock-free concurrency** avoids using traditional locks. Instead, it relies on hardware-supported atomic operations that guarantee an update happens as an indivisible step. If two threads attempt to update a variable at the same time, only one will succeed, and the other can retry until it succeeds. A common pattern is the compare-and-swap (CAS) loop, which repeatedly attempts to change a value only if it has not been modified since it was last read.

Today, we want you to explore both approaches in practice and observe which one performs better under different conditions. By experimenting with various scenarios, you will see how locks and lock-free mechanisms work.

# Part 2: Mutex - recap

A **mutex** (short for *mutual exclusion*) is a synchronization primitive used to protect shared data from concurrent access. It ensures that only one thread can execute a particular section of code at a time. When a thread calls `pthread_mutex_lock()`, it attempts to acquire ownership of the mutex.

- If the mutex is available, the thread acquires it and proceeds.
- If another thread already holds the mutex, the calling thread is blocked until the mutex becomes available.

When the thread finishes its work in the critical section, it calls `pthread_mutex_unlock()` to release the lock so that another waiting thread can continue.

Below is the portion of our program that implements the **mutex-based** version of the experiment. This version protects access to the shared data using `pthread_mutex_lock()` and `pthread_mutex_unlock()`.

```c
pthread_mutex_t lock;

void *increment_mutex(void *arg) {
    for (int i = 0; i < NUM_ITERS; i++) {
        pthread_mutex_lock(&lock);
        counter_mutex++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}
```

In this implementation, all threads share the same global `pthread_mutex_t lock`. Each iteration of the loop represents one unit of work that must be performed exclusively by a single thread. The calls to `pthread_mutex_lock()` and `pthread_mutex_unlock()` ensure that no two threads increase `counter_mutex` at the same time.

While this approach prevents data races, it also introduces serialization: only one thread can be inside the critical section at once. 

# Part 3: Lock Free with CAS

A **compare-and-swap (CAS)** operation is the foundation of many lock-free algorithms. It is an *atomic* instruction provided by modern processors that compares the value stored in a memory location with an expected value and, only if they match, replaces it with a new one.

This entire check-and-update step occurs as a single, indivisible hardware instruction, so no two threads can interfere during the operation.

Conceptually, CAS does the following:

```c
bool compare_and_swap(int *ptr, int expected, int new_value) {
    if (*ptr == expected) {
        *ptr = new_value;
        return true;   // success: value was updated
    } else {
        return false;  // failure: value changed by another thread
    }
}
```

In a real system, the function above executes as a single atomic instruction (for example, `CMPXCHG` on x86 CPUs). If multiple threads call it at the same time, only one will succeed. The others can retry using the new observed value. This retry loop allows multiple threads to make progress without blocking each other, which is the essence of lock-free programming.

In C11 standard, CAS is provided through the atomic operation `atomic_compare_exchange_weak()` or `atomic_compare_exchange_strong()`. The *weak* version may fail spuriously and is generally used inside a loop that retries until success. The strong version fails only when another thread actually modifies the value. Although it guarantees fewer false failures, it may involve slightly more overhead.  In this lab, we will use the *weak* version.

Here is roughly what happens inside the `atomic_compare_exchange_weak()` (in atomic, uninterruptible form): 

```c
bool atomic_compare_exchange_weak(int *ptr, int *expected, int new_value)
{
    if (*ptr == expected) {
        *ptr = new_value;
        return true; // success: value was updated to the new value
    } else {
        *expected = *ptr;
        return false; // failed: update expected value with the latest value
    }
}
```

Below is the portion of our program that implements the lock-free version:

```c
void *increment_cas(void *arg) {
    for (int i = 0; i < NUM_ITERS; i++) {
        int old = counter_cas;
        while (!atomic_compare_exchange_weak(&counter_cas, &old, old + 1)) {
            // retry until successful
        }
    }
    return NULL;
}
```

The function above uses a CAS loop to safely increment a shared counter without using a mutex. Each thread repeatedly reads the current value of `counter_cas` into a local variable `old`, then calls `atomic_compare_exchange_weak(&counter_cas, &old, old + 1)`, which atomically updates the counter *only if* it still equals `old`. If another thread modified the counter in the meantime, the CAS operation fails, updates `old` with the latest value, and retries. This loop continues until the update succeeds, ensuring that every increment is applied exactly once while allowing all threads to make progress without blocking.

Now it’s time to compile and run the `lock_vs_cas` program:

```c
./lock_vs_cas <num_threads> <iterations>
```

Record the execution time for both the mutex and CAS versions under different numbers of threads (e.g., 1, 2, 4, 8, 16, 32, 200(?)). You can also play with the number of iterations.

Answer the following question on Gradescope:

- How does the execution time of each version (mutex and CAS) change as the number of threads increases?
- Which version is faster under low thread counts? Under high thread counts? Explain the results by discussing factors such as contention, retries, and blocking.

## Part 4: CAS as spin-lock (20 Points)

In our previous experiment, the task was very simple; each thread only needed to update one shared variable. A single atomic instruction was enough to protect the operation, and no additional synchronization was required. 

However, most real-world systems have larger critical sections that involve multiple pieces of shared data. For example, a banking system might need to withdraw from one account and deposit into another at the same time. These two updates must happen together. If one succeeds and the other fails, the system becomes inconsistent. In such cases, a single CAS is no longer enough. 

One way to handle this is to use CAS to implement a simple spin lock, a lightweight lock that uses CAS to control access to a critical section. This spin lock can then protect multiple updates atomically, just like a mutex, but without the overhead of kernel blocking. 

Below is the portion of our program that implements the spinlock version of CAS to protect access to the shared array:

```c
atomic_int spin_flag = 0; // 0 = unlocked, 1 = locked

void *with_cas(void *arg) {
    for (int i = 0; i < NUM_ITERS; i++) {
        int expected = 0; // what we expect: unlocked
        // Try to acquire the lock
        while (!atomic_compare_exchange_weak(&spin_flag, &expected, 1)) {
            expected = 0; // reset expectation before retrying
        }

        do_work(shared_array);  // critical section

        atomic_store(&spin_flag, 0); // release
    }
    return NULL;
}
```

In the code above, we use CAS to implement a simple **lock-free spinlock** that protects access to a shared array. This implementation explicitly checks whether the `spin_flag` is unlocked (`0`) and sets it to locked (`1`) using CAS. Only one thread can succeed at this operation at a time.

- Do you think using CAS as a spinlock is a good idea?

### Let’s begin the experiment!

In this part, you will explore how the number of threads, the number of iterations, and the size of the array affect the performance of both synchronization methods. The goal is to observe how contention, workload size, and available CPU resources influence whether the **mutex** or **CAS** approach performs better.

### Step 1: How to run the Program

You can run the program using command-line arguments:

```c
./lock_vs_spin <num_threads> <num_iters> <array_size>
```

- Number of threads: represents the number of worker threads that will do the work.
- Iterations per thread: represents the number of times each thread will enter the critical section.
- Array size: represents the amount of work inside the critical section.

### Step 2: How to measure the performance

Change one parameter at a time (for example, keep the array size fixed and vary the number of threads).  You might want to make a table like this to collect your results:

| Threads | Iterations | Array Size | Mutex Time (s) | CAS Time (s) | Note |
| --- | --- | --- | --- | --- | --- |
| 1 |  |  |  |  |  |
| 2 |  |  |  |  |  |
| 4 |  |  |  |  |  |
| 8 |  |  |  |  |  |
| … |  |  |  |  |  |
| 200 |  |  |  |  |  |

### Step 3: Analyze your data and answer the questions

Now answer the following questions:

1. How does increasing the number of threads affect performance for each method?
2. How does increasing the number of iterations (the number of times each thread enters the critical section) affect performance for each method?
3. How does increasing the array size (amount of work per critical section) influence the results?
4. What is your conclusion from the experiment?  
    1. When will mutex be faster? 
    2. When will CAS be faster?
5. What is your key takeaway from this experiment?

## Extra Reading

https://lumian2015.github.io/lockFreeProgramming/lock-free-vs-spin-lock.html

 ****https://www.youtube.com/watch?v=c1gO9aB9nbs

http://www.youtube.com/watch?v=ZQFzMfHIxng

https://lumian2015.github.io/lockFreeProgramming/aba-problem.html - problem with lock-free

## Credits:

First, I wanted to thank our **anonymous classmate** for raising this interesting question about lock vs. lock-free concurrency on Piazza, which led to this lab. (Please let us know if you want us to remove the anonymity. We would love to credit you appropriately.)

Second, this is a new lab, so any feedback is very welcome!  You can use our brownie point form to share it.

Lastly, we hope you enjoyed it!