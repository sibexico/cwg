**C WaitGroup (cwg)**

The concept of golang-style WaitGroup in C. 

**Usage Example**

To use, just include cwg.h in your C source file.

example.c
```C
#include <stdio.h>
#include <time.h>
#include "cwg.h" // Include the header

// The function that runs in a new thread
int worker(void *arg) {
    int worker_id = *(int *)arg;
    printf("Worker %d: Starting...\n", worker_id);

    // Simulate work
    thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL); 

    printf("Worker %d: Finished.\n", worker_id);
    return 0;
}

int main() {
    cwg_t wg;

    // Initialize the WaitGroup
    if (!cwg_init(&wg)) {
        fprintf(stderr, "Failed to init cwg.\n");
        return 1;
    }

    printf("Main: Starting 3 concurrent workers...\n");

    int id1 = 1, id2 = 2, id3 = 3;

    // Launch tasks using cwg_go
    cwg_go(&wg, worker, &id1);
    cwg_go(&wg, worker, &id2);
    cwg_go(&wg, worker, &id3);

    printf("Main: Waiting for all workers to finish...\n");
    
    // Block until all tasks call cwg_done()
    cwg_wait(&wg);

    printf("Main: All tasks complete. Shutting down.\n");

    // Clean up
    cwg_destroy(&wg);

    return 0;
}

```

**Compilation**

```Bash
gcc -std=c23 -o example example.c -pthread
./example
```