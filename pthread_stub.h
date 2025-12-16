#ifndef PTHREAD_STUB_H
#define PTHREAD_STUB_H

/*
 * pthread stub for DOS/DJGPP
 * 
 * This provides a minimal pthread-compatible interface for DOS.
 * Since DOS doesn't support threads, this is a single-threaded implementation
 * that simulates the pthread API.
 * 
 * All mutex operations are no-ops (mutexes aren't needed in single-threaded DOS)
 * Thread creation is not supported (but we can fake it with function calls)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>  /* For fmin, fmax which may be used alongside pthreads */

#ifdef __cplusplus
extern "C" {
#endif

/* Type definitions */
typedef int pthread_mutex_t;
typedef int pthread_t;
typedef int pthread_mutexattr_t;
typedef int pthread_attr_t;

/* Mutex constants */
#define PTHREAD_MUTEX_INITIALIZER 0

/* Function prototypes */

/* Mutex operations - all no-ops for single-threaded DOS */
static inline int pthread_mutex_init(pthread_mutex_t *mutex, 
                                     const pthread_mutexattr_t *attr) {
    (void)attr;  /* unused */
    *mutex = 0;
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    *mutex = 0;
    return 0;
}

/* These are no-ops in single-threaded environment */
static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
    (void)mutex;  /* unused - no actual locking needed */
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    (void)mutex;  /* unused - no actual unlocking needed */
    return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    (void)mutex;  /* unused */
    return 0;  /* Always succeeds */
}

/* Thread operations */

/* Global storage for the thread function and arguments */
static void *(*__dos_thread_func)(void *) = NULL;
static void *__dos_thread_arg = NULL;

static inline int pthread_create(pthread_t *thread,
                                const pthread_attr_t *attr,
                                void *(*start_routine)(void *),
                                void *arg) {
    (void)attr;  /* unused */
    
    /* In DOS, we can't actually create a thread.
     * Instead, we store the function and argument for later execution.
     * The main program should call the thread function directly when needed.
     */
    *thread = 1;  /* Return a non-zero thread ID */
    __dos_thread_func = start_routine;
    __dos_thread_arg = arg;
    
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    (void)thread;    /* unused */
    (void)retval;    /* unused */
    
    /* In single-threaded DOS, joining is a no-op.
     * The thread function should have already completed
     * when called from the main thread.
     */
    return 0;
}

static inline int pthread_detach(pthread_t thread) {
    (void)thread;  /* unused */
    return 0;
}

static inline int pthread_cancel(pthread_t thread) {
    (void)thread;  /* unused */
    /* In DOS, we can't actually cancel a thread.
     * Since we don't have real threads, this is a no-op.
     * The thread function should check for cancellation points
     * and exit gracefully if needed.
     */
    return 0;
}

static inline pthread_t pthread_self(void) {
    return 1;  /* Always return main thread ID */
}

static inline int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2;
}

/* Attribute operations */
static inline int pthread_attr_init(pthread_attr_t *attr) {
    *attr = 0;
    return 0;
}

static inline int pthread_attr_destroy(pthread_attr_t *attr) {
    *attr = 0;
    return 0;
}

/* Helper function to execute the stored thread function */
static inline void __dos_run_thread_func(void) {
    if (__dos_thread_func != NULL) {
        __dos_thread_func(__dos_thread_arg);
    }
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* PTHREAD_STUB_H */
