#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

// Start/stop for .plt - custom compiler extension!
extern char __start_plt;
extern char __stop_plt;

//#define BENCHMARK
#define WORKAROUND_DEBUG_MAX 0

#ifdef BENCHMARK
#include <stdio.h>
#endif
#if WORKAROUND_DEBUG_MAX > 0
#include <stdio.h>

static struct {
    const char *plt;
    uint64_t target;
} workaroundBranches[WORKAROUND_DEBUG_MAX];
static int workaroundBranchesCount = 0;

static inline void reportWorkaroundBranches() {
  if (WORKAROUND_DEBUG_MAX > 0 && workaroundBranchesCount > 0) {
    fprintf(stderr, "[DYNLINK] %d GOT entries needed workaround\n", workaroundBranchesCount);
    for (int i = 0; i < workaroundBranchesCount && i < WORKAROUND_DEBUG_MAX; i++) {
      if (workaroundBranches[i].target == 0) {
        fprintf(stderr, "[DYNLINK] Workaround used: plt %p has no target in GOT\n", workaroundBranches[i].plt);
      } else {
        uint64_t target_offset = workaroundBranches[i].target - (uint64_t) workaroundBranches[i].plt;
        fprintf(stderr, "[DYNLINK] Workaround used: plt %p targets 0x%lx (offset %ld = %lx)\n",
                workaroundBranches[i].plt, workaroundBranches[i].target, target_offset, target_offset);
      }
    }
  }
}
#endif


#if defined(__x86_64__) || defined(_M_X64)
// ===================================== x86_64 =====================================

#define PLT_HEADER_SIZE 16
#define PLT_ENTRY_SIZE 16
#define PAGE_SIZE 4096

static inline __attribute__((always_inline)) long syscall3(long n, long a1, long a2, long a3) {
  unsigned long ret;
  __asm__ __volatile__ ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
  "d"(a3) : "rcx", "r11", "memory");
  return ret;
}

__attribute__((visibility("hidden")))
__attribute__((used))
void __noic_patch_plt() {
  // make PLT writeable
  syscall3(SYS_mprotect, (long) &__start_plt, PAGE_SIZE, PROT_READ | PROT_WRITE);
  // patch PLT
  for (char *plt = &__start_plt + PLT_HEADER_SIZE; plt < &__stop_plt; plt += PLT_ENTRY_SIZE) {
    char* got_address = *((uint32_t *) (plt + 2)) + plt + 6;
    uint64_t target = *(uint64_t*) got_address;
    // x86 PoC
    plt[0] = 0x49;
    plt[1] = 0xbb;
    *((uint64_t*) &plt[2]) = target;
    plt[10] = 0x41;
    plt[11] = 0xff;
    plt[12] = 0xe3;
    // debug when aarch64 branch width is not enough
#if WORKAROUND_DEBUG_MAX > 0
    uint64_t target_offset = target - (uint64_t) plt;
    if (!(target_offset <= 0x7ffffff || target_offset > 0xfffffff800000000)) {
      if (workaroundBranchesCount < WORKAROUND_DEBUG_MAX) {
        workaroundBranches[workaroundBranchesCount].plt = plt;
        workaroundBranches[workaroundBranchesCount].target = target;
        workaroundBranchesCount++;
      }
    }
#endif
  }
  // make PLT executable again
  syscall3(SYS_mprotect, (long) &__start_plt, PAGE_SIZE, PROT_READ | PROT_EXEC);
  // debug stuff
#if WORKAROUND_DEBUG_MAX > 0
  reportWorkaroundBranches();
#endif
}

#elif defined(__aarch64__) || defined(_M_ARM64)
// ===================================== aarch64 =====================================

#define PLT_HEADER_SIZE 32
#define PLT_ENTRY_SIZE 16
#define PAGE_SIZE 16384  // at least on the M1

#define __asm_syscall(...) do { \
	__asm__ __volatile__ ( "svc 0" \
	: "=r"(r0) : __VA_ARGS__ : "memory"); \
	return r0; \
	} while (0)

static inline __attribute__((always_inline)) long syscall3(long n, long a, long b, long c) {
	register long r7 __asm__("x8") = n;
	register long r0 __asm__("x0") = a;
	register long r1 __asm__("x1") = b;
	register long r2 __asm__("x2") = c;
	__asm_syscall("r"(r7), "0"(r0), "r"(r1), "r"(r2));
}

static inline void assert_syscall(long result) {
  if (result != 0) {
    syscall3(SYS_write, 0, (long) (const char*) "mprotect failed\n", 16);
    syscall3(SYS_exit, result, 0, 0);
  }
}

#ifdef BENCHMARK
static inline uint64_t timestamp(){
  uint64_t value;
  __asm__ volatile("MRS %[target], CNTVCT_EL0" : [target] "=r" (value) : : );
  return value;
}
#endif

__attribute__((visibility("hidden")))
__attribute__((used))
void __noic_patch_plt() {
#ifdef BENCHMARK
  uint64_t ts = timestamp();
#endif
  // make PLT writeable
  assert_syscall(syscall3(SYS_mprotect, (long) &__start_plt, PAGE_SIZE, PROT_READ | PROT_WRITE));
  // patch PLT
  for (char *plt = &__start_plt + PLT_HEADER_SIZE; plt < &__stop_plt; plt += PLT_ENTRY_SIZE) {
    uint32_t *instructions = (uint32_t *) plt;
    uint32_t page_offset = (((instructions[0] >> 5) & 0x3ffff) << 2) | ((instructions[0] >> 29) & 3);
    uint32_t got_offset = (instructions[2] >> 10) & 0xfff;

    char* got_address = (char *) ((((uint64_t) plt) & 0xfffffffffffff000) + (page_offset << 12) + got_offset);
    uint64_t target = *(uint64_t*) got_address;
    uint64_t target_offset = target - (uint64_t) plt;
    if (target == 0) {
      // TODO ignore for now
    } else if (target_offset <= 0x7ffffff || target_offset > 0xfffffff800000000) {
      instructions[0] = 0x14000000 | ((target_offset >> 2) & 0x3ffffff);
    } else {
      // TODO hacky workaround
      instructions[1] = 0x91000210 | (got_offset << 10);
      instructions[2] = 0xd65f0200; // ret x16 - the workaround for now if memory alignment is broken
#if WORKAROUND_DEBUG_MAX > 0
      if (workaroundBranchesCount < WORKAROUND_DEBUG_MAX) {
        workaroundBranches[workaroundBranchesCount].plt = plt;
        workaroundBranches[workaroundBranchesCount].target = target;
        workaroundBranchesCount++;
      }
#endif
    }
  }
  // make PLT executable again
  assert_syscall(syscall3(SYS_mprotect, (long) &__start_plt, PAGE_SIZE, PROT_READ | PROT_EXEC));
  // debug stuff
#ifdef BENCHMARK
  uint64_t ts2 = timestamp();
  printf("%lu\n", ts2 - ts);
  syscall3(SYS_exit_group, 0, 0, 0);
#endif
#if WORKAROUND_DEBUG_MAX > 0
  reportWorkaroundBranches();
#endif
}


#else
#error "Unsupported architecture!"
#endif