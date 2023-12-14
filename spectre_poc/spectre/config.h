#ifndef CONFIG_H
#define CONFIG_H

/*
 * constants
 */

#define EVICTION 0
#define FLUSHING 1

#define MSR 0
#define COUNTER_THREAD 1
#define MONOTONIC 2


/*
 * generic configuration
 */
 
//higher value = more output
#define VERBOSITY 0

// use flushing by default (if unavailable change to EVICTION)
#define CACHE FLUSHING

// use counter thread (should be fine)
#define TIMER COUNTER_THREAD

// page size (Apple M1 has 16KB pages. Otherwise change it back to 4KB)
#define PAGE_SIZE (4096 * 4)


/*
 * eviction configuration
 */
     
#ifdef EVICTION

    // threshold for finding eviction sets
    // comment out for automatic calculation
    #define EVICTION_THRESHOLD 570

    // eviction set size
    #define EVICTION_SET_SIZE 32

    // size of memory to alloc
    #define EVICTION_MEMORY_SIZE 20 * 1024 * 1024

#endif /* EVICTION */


/*
 * spectre configuration
 */

// ~4 should be most efficient since it is a good ratio of eviction calls per bits leaked
// However, 1 seems most consistent
#define BITS 1

// entry size in array2
#define ENTRY_SIZE 1024

// Amount of measurement steps per leaked index.
// If you want higher leakage rate, you can reduce this.
#define ITERATIONS 4

// Amount of calls to the victim function per measuerement step (ITERATIONS)
// you can probably decrease this to increase leakage rate, but this should work :)
#define VICTIM_CALLS 160

// amount of training calls per out-of-bound call to the victim function.
// VICTIM_CALLS should be divisible by TRAINING + 1
// You can probably decrease this to increase leakage rate, but this should work :)
#define TRAINING 39

// Threshold to distinguish cache miss from cache hit.
// Differs by device. You can set VERBOSITY to a high value, then run the PoC.
// This will give you timings for cache hit and miss.
// Then, you can choose a nice threshold in between.
#define THRESHOLD 120

// Set to 1 to run a benchmark
#define BENCHMARK 0

/* --- calculated automatically --- */

// amount of offsets per byte
#define OFFSETS_PER_BYTE (8 / BITS)

// amount of values in array2
#define VALUES (1 << BITS)


/*
 * Mitigation
 */

// mitigation to use (uncomment to insert before indirect branches)
// #define MITIGATION_ASM "DSB"

#endif /* CONFIG_H */
