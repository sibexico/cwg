#ifndef CWG_H
#define CWG_H

#include <stdlib.h>
#include <stdbool.h>

// Atomic operations support - check for C11 atomics
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
    #include <stdatomic.h>
    #define CWG_HAS_ATOMICS
#elif defined(_MSC_VER)
    // MSVC doesn't support C11 atomics, use Windows Interlocked functions
    #include <windows.h>
    typedef volatile LONG cwg_atomic_int;
    typedef volatile LONG cwg_atomic_bool;
    #define atomic_init(obj, val) (*(obj) = (val))
    #define atomic_load(obj) InterlockedCompareExchange((LONG*)(obj), 0, 0)
    #define atomic_load_explicit(obj, order) atomic_load(obj)
    #define atomic_store(obj, val) InterlockedExchange((LONG*)(obj), (val))
    #define atomic_store_explicit(obj, val, order) atomic_store(obj, val)
    #define atomic_fetch_add(obj, val) InterlockedExchangeAdd((LONG*)(obj), (val))
    #define atomic_fetch_add_explicit(obj, val, order) atomic_fetch_add(obj, val)
    #define atomic_fetch_sub(obj, val) InterlockedExchangeAdd((LONG*)(obj), -(val))
    #define atomic_fetch_sub_explicit(obj, val, order) atomic_fetch_sub(obj, val)
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang builtin atomics
    typedef volatile int cwg_atomic_int;
    typedef volatile int cwg_atomic_bool;
    #define atomic_init(obj, val) (*(obj) = (val))
    #define atomic_load(obj) __atomic_load_n(obj, __ATOMIC_SEQ_CST)
    #define atomic_load_explicit(obj, order) __atomic_load_n(obj, __ATOMIC_SEQ_CST)
    #define atomic_store(obj, val) __atomic_store_n(obj, val, __ATOMIC_SEQ_CST)
    #define atomic_store_explicit(obj, val, order) __atomic_store_n(obj, val, __ATOMIC_SEQ_CST)
    #define atomic_fetch_add(obj, val) __atomic_fetch_add(obj, val, __ATOMIC_SEQ_CST)
    #define atomic_fetch_add_explicit(obj, val, order) __atomic_fetch_add(obj, val, __ATOMIC_SEQ_CST)
    #define atomic_fetch_sub(obj, val) __atomic_fetch_sub(obj, val, __ATOMIC_SEQ_CST)
    #define atomic_fetch_sub_explicit(obj, val, order) __atomic_fetch_sub(obj, val, __ATOMIC_SEQ_CST)
#else
    #error "No atomic operations support available"
#endif

// Define atomic types if not using C11 atomics
#ifdef CWG_HAS_ATOMICS
    typedef atomic_int cwg_atomic_int;
    typedef atomic_bool cwg_atomic_bool;
#endif

// Memory order compatibility (for C11 atomics)
#ifndef memory_order_acquire
    #define memory_order_acquire 0
#endif
#ifndef memory_order_release
    #define memory_order_release 0
#endif
#ifndef memory_order_acq_rel
    #define memory_order_acq_rel 0
#endif

// Cross-platform threading support
#if defined(_WIN32) || defined(_WIN64)
    // Use native Windows threads if C11 threads not available
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
        #include <threads.h>
        #define CWG_USE_C11_THREADS
    #else
        #include <windows.h>
        #define CWG_USE_WIN32_THREADS
    #endif
#else
    // Linux/Unix: Try C11 threads first, fallback to pthreads
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
        #include <threads.h>
        #define CWG_USE_C11_THREADS
    #else
        #include <pthread.h>
        #define CWG_USE_PTHREADS
    #endif
#endif

// Cross-platform thread types
#ifdef CWG_USE_C11_THREADS
    typedef mtx_t cwg_mutex_t;
    typedef cnd_t cwg_cond_t;
    typedef thrd_t cwg_thread_t;
#elif defined(CWG_USE_WIN32_THREADS)
    typedef struct {
        CRITICAL_SECTION cs;
    } cwg_mutex_t;
    typedef struct {
        CONDITION_VARIABLE cv;
    } cwg_cond_t;
    typedef HANDLE cwg_thread_t;
#elif defined(CWG_USE_PTHREADS)
    typedef pthread_mutex_t cwg_mutex_t;
    typedef pthread_cond_t cwg_cond_t;
    typedef pthread_t cwg_thread_t;
#endif

// WaitGroup structure: syncs threads until a counter reaches zero.
// Go's sync.WaitGroup behavior.
typedef struct {
    cwg_atomic_int counter;
    cwg_mutex_t mtx;          // Mutex for condition variable
    cwg_cond_t cnd;           // Condition variable
    cwg_atomic_bool waiting;  // Track if someone is waiting
} cwg_t;

// Job wrapper to pass data and the function to the new thread.
typedef struct {
    cwg_t *wg;
    void *arg;
    int (*coro_func)(void *);
} cwg_job_t;

void cwg_done(cwg_t *wg);

// Cross-platform mutex operations
static inline bool _cwg_mutex_init(cwg_mutex_t *mtx) {
#ifdef CWG_USE_C11_THREADS
    return mtx_init(mtx, mtx_plain) == thrd_success;
#elif defined(CWG_USE_WIN32_THREADS)
    InitializeCriticalSection(&mtx->cs);
    return true;
#elif defined(CWG_USE_PTHREADS)
    return pthread_mutex_init(mtx, NULL) == 0;
#endif
}

static inline void _cwg_mutex_destroy(cwg_mutex_t *mtx) {
#ifdef CWG_USE_C11_THREADS
    mtx_destroy(mtx);
#elif defined(CWG_USE_WIN32_THREADS)
    DeleteCriticalSection(&mtx->cs);
#elif defined(CWG_USE_PTHREADS)
    pthread_mutex_destroy(mtx);
#endif
}

static inline void _cwg_mutex_lock(cwg_mutex_t *mtx) {
#ifdef CWG_USE_C11_THREADS
    mtx_lock(mtx);
#elif defined(CWG_USE_WIN32_THREADS)
    EnterCriticalSection(&mtx->cs);
#elif defined(CWG_USE_PTHREADS)
    pthread_mutex_lock(mtx);
#endif
}

static inline void _cwg_mutex_unlock(cwg_mutex_t *mtx) {
#ifdef CWG_USE_C11_THREADS
    mtx_unlock(mtx);
#elif defined(CWG_USE_WIN32_THREADS)
    LeaveCriticalSection(&mtx->cs);
#elif defined(CWG_USE_PTHREADS)
    pthread_mutex_unlock(mtx);
#endif
}

// Cross-platform
static inline bool _cwg_cond_init(cwg_cond_t *cnd) {
#ifdef CWG_USE_C11_THREADS
    return cnd_init(cnd) == thrd_success;
#elif defined(CWG_USE_WIN32_THREADS)
    InitializeConditionVariable(&cnd->cv);
    return true;
#elif defined(CWG_USE_PTHREADS)
    return pthread_cond_init(cnd, NULL) == 0;
#endif
}

static inline void _cwg_cond_destroy(cwg_cond_t *cnd) {
#ifdef CWG_USE_C11_THREADS
    cnd_destroy(cnd);
#elif defined(CWG_USE_WIN32_THREADS)
    // No destroy needed for Windows
    (void)cnd;
#elif defined(CWG_USE_PTHREADS)
    pthread_cond_destroy(cnd);
#endif
}

static inline void _cwg_cond_wait(cwg_cond_t *cnd, cwg_mutex_t *mtx) {
#ifdef CWG_USE_C11_THREADS
    cnd_wait(cnd, mtx);
#elif defined(CWG_USE_WIN32_THREADS)
    SleepConditionVariableCS(&cnd->cv, &mtx->cs, INFINITE);
#elif defined(CWG_USE_PTHREADS)
    pthread_cond_wait(cnd, mtx);
#endif
}

static inline void _cwg_cond_broadcast(cwg_cond_t *cnd) {
#ifdef CWG_USE_C11_THREADS
    cnd_broadcast(cnd);
#elif defined(CWG_USE_WIN32_THREADS)
    WakeAllConditionVariable(&cnd->cv);
#elif defined(CWG_USE_PTHREADS)
    pthread_cond_broadcast(cnd);
#endif
}

// Cross-platform thread operations
#ifdef CWG_USE_WIN32_THREADS
static DWORD WINAPI _cwg_thread_entry_win32(LPVOID arg) {
    cwg_job_t *job = (cwg_job_t *)arg;
    
    // Run the actual user function
    int result = job->coro_func(job->arg);

    // Signal completion
    cwg_done(job->wg);
    
    // Automatic memory management for the job struct
    free(job); 

    return (DWORD)result;
}
#endif

// Executes the user function, then calls cwg_done(), and frees the job memory.
static int _cwg_thread_entry(void *arg) {
    cwg_job_t *job = (cwg_job_t *)arg;
    int result = job->coro_func(job->arg);
    cwg_done(job->wg);
    free(job); 
    return result;
}

// Initialize the WaitGroup. Must be called before use.
bool cwg_init(cwg_t *wg) {
    if (!wg) return false;
    
    atomic_init(&wg->counter, 0);
    atomic_init(&wg->waiting, false);
    
    if (!_cwg_mutex_init(&wg->mtx)) {
        return false;
    }
    
    if (!_cwg_cond_init(&wg->cnd)) {
        _cwg_mutex_destroy(&wg->mtx);
        return false;
    }
    
    return true;
}

// Clean up the WaitGroup. Must be called after cwg_wait().
void cwg_destroy(cwg_t *wg) {
    if (!wg) return;
    
    _cwg_cond_destroy(&wg->cnd);
    _cwg_mutex_destroy(&wg->mtx);
}

// Increment the counter by 'delta'. Call before starting work.
bool cwg_add(cwg_t *wg, int delta) {
    if (!wg) return false;
    
    // Prevent adding while someone is waiting (matches Go's behavior)
    if (atomic_load(&wg->waiting)) {
        return false; // In Go, this would panic
    }
    
    if (delta == 0) return true;
    
    // In Go, negative counter causes panic
    int old_val = atomic_fetch_add_explicit(&wg->counter, delta, memory_order_release);
    int new_val = old_val + delta;
    
    // Detect negative counter (invalid state)
    if (new_val < 0) {
        // Restore the counter and fail
        atomic_fetch_sub(&wg->counter, delta);
        return false; // In Go, this would panic
    }
    
    // If counter reached zero due to negative delta, signal waiters
    if (new_val == 0 && old_val > 0) {
        _cwg_mutex_lock(&wg->mtx);
        _cwg_cond_broadcast(&wg->cnd);
        _cwg_mutex_unlock(&wg->mtx);
    }
    
    return true;
}

// Decrement the counter by 1. Call when a task is finished.
// Equivalent to cwg_add(wg, -1).
void cwg_done(cwg_t *wg) {
    if (!wg) return;
    int old_count = atomic_fetch_sub_explicit(&wg->counter, 1, memory_order_acq_rel);
    if (old_count == 1) {
        _cwg_mutex_lock(&wg->mtx);
        _cwg_cond_broadcast(&wg->cnd);
        _cwg_mutex_unlock(&wg->mtx);
    }
}

// Block until the counter reaches zero.
void cwg_wait(cwg_t *wg) {
    if (!wg) return;
    _cwg_mutex_lock(&wg->mtx);
    atomic_store(&wg->waiting, true);
    while (atomic_load_explicit(&wg->counter, memory_order_acquire) > 0) {
        _cwg_cond_wait(&wg->cnd, &wg->mtx);
    }
    atomic_store(&wg->waiting, false);
    _cwg_mutex_unlock(&wg->mtx);
}

// Starts a new concurrent task (like Golang's 'go' keyword).
bool cwg_go(cwg_t *wg, int (*func)(void *), void *arg) {
    if (!wg || !func) return false;
    cwg_job_t *job = (cwg_job_t *)malloc(sizeof(cwg_job_t));
    if (!job) return false;
    job->wg = wg;
    job->arg = arg;
    job->coro_func = func;

    // Must be added before thread creation to prevent race condition
    if (!cwg_add(wg, 1)) {
        free(job);
        return false;
    }

#ifdef CWG_USE_C11_THREADS
    thrd_t thread_id;
    int res = thrd_create(&thread_id, _cwg_thread_entry, job);

    if (res != thrd_success) {
        cwg_done(wg);
        free(job);
        return false;
    }
    
    thrd_detach(thread_id);
#elif defined(CWG_USE_WIN32_THREADS)
    HANDLE thread_handle = CreateThread(NULL, 0, _cwg_thread_entry_win32, job, 0, NULL);
    
    if (thread_handle == NULL) {
        cwg_done(wg);
        free(job);
        return false;
    }
    
    CloseHandle(thread_handle); // Detach by closing handle
#elif defined(CWG_USE_PTHREADS)
    pthread_t thread_id;
    int res = pthread_create(&thread_id, NULL, (void *(*)(void *))_cwg_thread_entry, job);
    
    if (res != 0) {
        cwg_done(wg);
        free(job);
        return false;
    }
    
    pthread_detach(thread_id);
#endif

    return true;
}

// Get current counter value (for debugging/testing)
// The value may change immediately after reading
int cwg_count(cwg_t *wg) {
    if (!wg) return -1;
    return atomic_load_explicit(&wg->counter, memory_order_acquire);
}

#endif
