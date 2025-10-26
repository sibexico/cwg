**C WaitGroup (cwg)**

A Golang-style WaitGroup implementation in C with cross-platform support for Linux and Windows. 

## API Reference

- `bool cwg_init(cwg_t *wg)` - Initialize a WaitGroup (must call before use)
- `void cwg_destroy(cwg_t *wg)` - Clean up WaitGroup resources
- `bool cwg_add(cwg_t *wg, int delta)` - Add delta to the counter (returns false on error)
- `void cwg_done(cwg_t *wg)` - Decrement counter by 1 (call when task completes)
- `void cwg_wait(cwg_t *wg)` - Block until counter reaches zero
- `bool cwg_go(cwg_t *wg, int (*func)(void *), void *arg)` - Start a goroutine-style task
- `int cwg_count(cwg_t *wg)` - Get current counter value (for debugging)

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

    printf("Starting 3 concurrent workers...\n");

    int id1 = 1, id2 = 2, id3 = 3;

    // Launch tasks using cwg_go
    cwg_go(&wg, worker, &id1);
    cwg_go(&wg, worker, &id2);
    cwg_go(&wg, worker, &id3);

    printf("Waiting for all workers to finish...\n");
    
    // Block until all tasks call cwg_done()
    cwg_wait(&wg);

    printf("All tasks complete. Shutting down.\n");

    // Clean up
    cwg_destroy(&wg);

    return 0;
}

```

**Compilation**

### Linux/Unix
```bash
gcc -std=c11 -o example example.c -pthread
./example
```

### Windows
```bash
gcc -std=c11 -o example.exe example.c
example.exe
```