#ifndef CWG_H
#define CWG_H

#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>

// WaitGroup structure: syncs threads until a counter reaches zero.
typedef struct {
    atomic_int counter;
    mtx_t mtx;          // Mutex for condition variable
    cnd_t cnd;          // Condition variable
} cwg_t;

// Job wrapper to pass data and the function to the new thread.
typedef struct {
    cwg_t *wg;
    void *arg;
    int (*coro_func)(void *); // Function pointer
} cwg_job_t;

// Executes the user function, then calls cwg_done(), and frees the job memory.
static int _cwg_thread_entry(void *arg) {
    cwg_job_t *job = (cwg_job_t *)arg;
    
    // Run the actual user function
    job->coro_func(job->arg);

    // Signal completion
    cwg_done(job->wg);
    
    // Automatic memory management for the job struct
    free(job); 

    return 0;
}

// Initialize the WaitGroup. Must be called before use.
bool cwg_init(cwg_t *wg) {
    atomic_init(&wg->counter, 0);
    if (mtx_init(&wg->mtx, mtx_plain) != thrd_success) return false;
    if (cnd_init(&wg->cnd) != thrd_success) { mtx_destroy(&wg->mtx); return false; }
    return true;
}

// Clean up the WaitGroup resources. Must be called after cwg_wait().
void cwg_destroy(cwg_t *wg) {
    cnd_destroy(&wg->cnd);
    mtx_destroy(&wg->mtx);
}

// Increment the counter by 'delta'. Call before starting work.
void cwg_add(cwg_t *wg, int delta) {
    atomic_fetch_add(&wg->counter, delta);
}

// Decrement the counter by 1. Call when a task is finished.
void cwg_done(cwg_t *wg) {
    int old_count = atomic_fetch_sub(&wg->counter, 1);
    
    // If the last task finished, notify all waiters.
    if (old_count == 1) {
        mtx_lock(&wg->mtx);
        cnd_broadcast(&wg->cnd);
        mtx_unlock(&wg->mtx);
    }
}

// Block until the counter reaches zero.
void cwg_wait(cwg_t *wg) {
    mtx_lock(&wg->mtx);
    while (atomic_load(&wg->counter) > 0) {
        cnd_wait(&wg->cnd, &wg->mtx);
    }
    mtx_unlock(&wg->mtx);
}

// Starts a new concurrent task (like Golang's 'go' keyword).
// Handles the cwg_add() call and job memory.
bool cwg_go(cwg_t *wg, int (*func)(void *), void *arg) {
    cwg_job_t *job = malloc(sizeof(cwg_job_t));
    if (!job) return false;

    job->wg = wg;
    job->arg = arg;
    job->coro_func = func;

    // Must be added before thread creation
    cwg_add(wg, 1); 

    thrd_t thread_id;
    int res = thrd_create(&thread_id, _cwg_thread_entry, job);

    if (res != thrd_success) {
        // Cleanup if thread creation fails
        cwg_done(wg);
        free(job);
        return false;
    }
    
    // Detach the thread so we don't need to thrd_join() later
    thrd_detach(thread_id); 

    return true;
}

#endif
