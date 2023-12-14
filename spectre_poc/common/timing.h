#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include <time.h>

#include "config.h"
#include "memory.h"

#if TIMER == COUNTER_THREAD
    extern uint64_t timestamp;
    #define timer_read(x) x = timestamp
#elif TIMER == MSR
    #define timer_read(x) asm volatile("MRS %[target], PMCCNTR_EL0" : [target] "=r" (x) :: "memory" );
#elif TIMER == MONOTONIC
#if defined CLOCK_MONOTONIC_RAW
#define CS CLOCK_MONOTONIC_RAW
#else
#define CS CLOCK_MONOTONIC
#endif
    #define timer_read(x) do { struct timespec t1; clock_gettime(CS, &t1); x = t1.tv_sec * 1000 * 1000 * 1000ULL + t1.tv_nsec; } while(0)
#else 
    #error TIMER is set to an invalid value!
#endif /* TIMER */


static inline __attribute__((always_inline)) uint64_t probe(char* address){
    register uint64_t start, end;
    memory_fence();
    timer_read(start);
    memory_fence();
    memory_access(address);
    memory_fence();
    timer_read(end);
    memory_fence();
    return end - start;
}

void timer_start();

void timer_stop();

#endif /* TIMING_H */
