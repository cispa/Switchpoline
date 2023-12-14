#include <stdlib.h>
#include <stdio.h>

// jit only works on aarch64
#ifdef __aarch64__

#include <inttypes.h>
#include <sys/mman.h>

// functions to construct some instructions
#include "asmaarch64.h"

// amount of instructions in branch sled
#define SLED_SIZE 3071
#define SLED_SIZE_NOVT 1023

// amount of instructions per branch sled entry
#define ENTRY_SIZE 6

// pointer to branch sled (uint32_t pointer so we can write 4 byte instructions easily)
static uint32_t *BRANCH_SLED = NULL;
static uint32_t *BRANCH_SLED_NOVT = NULL;

// current offset in branch sled
static uint32_t sled_offset = 0;
static uint32_t sled_offset_novt = 0;

// get offset (in bytes) from branch sled entry at sled_idx
static int64_t calculate_offset(uint32_t sled_idx, void* target) {
    uint64_t _target = (uint64_t)target;
    uint64_t _instruction = (uint64_t)(void*)(&BRANCH_SLED[sled_idx]);
    return _target - _instruction;
}

static int64_t calculate_offset_novt(uint32_t sled_idx, void* target) {
    uint64_t _target = (uint64_t)target;
    uint64_t _instruction = (uint64_t)(void*)(&BRANCH_SLED_NOVT[sled_idx]);
    return _target - _instruction;
}

static int64_t calculate_offset_for_adrp(uint32_t sled_idx, void* target) {
    uint64_t _target = (uint64_t)target;
    uint64_t _instruction = ((uint64_t)(void*)(&BRANCH_SLED[sled_idx])) & 0xfffffffffffff000;
    return _target - _instruction;
}

// write instruction to next offset in sled
static void write_instruction(uint32_t instruction) {
    if(sled_offset >= SLED_SIZE){
        // patch code is broken and does not wrap around!
        fprintf(stderr, "patch code is broken: Did not wrap around even though sled is full!\n");
        exit(53);
    }
    BRANCH_SLED[sled_offset ++] = instruction;
}

static void write_instruction_novt(uint32_t instruction) {
    if(sled_offset_novt >= SLED_SIZE_NOVT){
        // patch code is broken and does not wrap around!
        fprintf(stderr, "patch code is broken: Did not wrap around even though sled is full!\n");
        exit(53);
    }
    BRANCH_SLED_NOVT[sled_offset_novt ++] = instruction;
}

static void write_instruction_for_constant_load(int reg, uint64_t constant) {
    write_instruction(asm_mov_imm(reg, constant & 0xffff));
    if (((constant >> 16) & 0xffff) != 0)
        write_instruction(asm_movk_imm(reg, (constant >> 16) & 0xffff, 16));
    if (((constant >> 32) & 0xffff) != 0)
        write_instruction(asm_movk_imm(reg, (constant >> 32) & 0xffff, 32));
    if (((constant >> 48) & 0xffff) != 0)
        write_instruction(asm_movk_imm(reg, (constant >> 48) & 0xffff, 48));
}

static void write_instruction_for_constant_load_novt(int reg, uint64_t constant) {
    write_instruction_novt(asm_mov_imm(reg, constant & 0xffff));
    if (((constant >> 16) & 0xffff) != 0)
        write_instruction_novt(asm_movk_imm(reg, (constant >> 16) & 0xffff, 16));
    if (((constant >> 32) & 0xffff) != 0)
        write_instruction_novt(asm_movk_imm(reg, (constant >> 32) & 0xffff, 32));
    if (((constant >> 48) & 0xffff) != 0)
        write_instruction_novt(asm_movk_imm(reg, (constant >> 48) & 0xffff, 48));
}

// add single entry to branch sled
static void __attribute__((used)) noic_patch_entry(uint64_t target){
    // if true, we don't need to append default case jump
    static int wrapped_around = 0;
    
    // make sure there is enough space available in sled, otherwise patch the first entry
    if (sled_offset >= SLED_SIZE - ENTRY_SIZE){
        // don't append jump to default case anymore (allows to re-use other jump targets)
        wrapped_around = 1;
        // wrap around
        sled_offset = 0;
    }
    int sled_offset_start = sled_offset;
    
    /* ; One entry looks as follows:
     * ; 1. load this entry's target address as immediate
     * ;    we need two instructions to load a big enough immediate
     *   adrp x15, <relative page>
     *   add  x15, <relative page offset>
     * ; 2. compare actual branch target with entry's target
     *   cmp x13, x15
     * ; 3. if not equal, go to next sled entry
     *   b.ne 2
     * ; 4. if equal, direct branch to target
     *   b <offset>        ; <offset> is relative to instruction here!
     * ; 5. if not wrapped around, add branch to patch case (optional)
     *   b __noic_default_handler_patch
     */
     
    if(sled_offset > 0 && !wrapped_around){
        sled_offset --;
    }
    
    // offset of target from first instruction (used for comparison)
    int64_t cmp_offset = calculate_offset_for_adrp(sled_offset, (void*)target);
    // 1. load this entry's target address as immediate
    write_instruction(asm_addrp(15, cmp_offset >> 12));
    write_instruction(asm_add_imm12(15, 15, target & 0b111111111111, 0));
    // 2. compare actual branch target with entry's target
    write_instruction(asm_cmp_reg(13, 15));
    // 3. if not equal, go to next sled entry
    write_instruction(asm_bne_imm(2));
    // 4. if equal, direct branch to entry's address (= actual branch target)
    write_instruction(asm_b_imm(calculate_offset(sled_offset, (void*)target) / 4));
    
    if (!wrapped_around) {
        // 5. add branch to patch case
        write_instruction(asm_b_imm(SLED_SIZE - sled_offset + 1 + SLED_SIZE_NOVT + 1));
    }
    
    // TODO: spectre V1 safety!!

    // Clear cache
    __clear_cache(&BRANCH_SLED[sled_offset_start-1], &BRANCH_SLED[sled_offset+1]);
}

// patch the branch sled
static void __attribute__((noinline, used)) noic_patch_sled(uint64_t target){
    if(!BRANCH_SLED){
        // first call; BRANCH_SLED is not setup. Make sure it points at the start of the actual sled
        //asm volatile("ldr %0, =__noic_default_handler_start" : "=r"(BRANCH_SLED));
        asm volatile("adrp %0, __noic_default_handler_start\nadd %0, %0, :lo12:__noic_default_handler_start" : "=r"(BRANCH_SLED));
    }

    // map branch sled as RW- (not X)
    size_t size = (SLED_SIZE+1) * 4;
    size = (size + 0x3fff) & ~0x3fff;
    mprotect(BRANCH_SLED, size, PROT_READ | PROT_WRITE);

    // add checks for the registered handler function
    noic_patch_entry(target);

    // map branch sled as R-X (not W)
    mprotect(BRANCH_SLED, size, PROT_READ | PROT_EXEC);
}

/**
 * Register a handler function for function IDs. Numbers between min_id and max_id must be sent to the given handler.
 */
void __attribute__((used)) __noic_register_handler(void *handler, uint64_t min_id, uint64_t max_id) {
    if(!BRANCH_SLED){
        // first call; BRANCH_SLED is not setup. Make sure it points at the start of the actual sled
        //asm volatile("ldr %0, =__noic_default_handler_start" : "=r"(BRANCH_SLED));
        asm volatile("nop\nadrp %0, __noic_default_handler_start\nadd %0, %0, :lo12:__noic_default_handler_start\nnop" : "=r"(BRANCH_SLED));
    }

    // map branch sled as RW- (not X)
    size_t size = (SLED_SIZE+1) * 4;
    size = (size + 0x3fff) & ~0x3fff;
    mprotect(BRANCH_SLED, size, PROT_READ | PROT_WRITE);

    //TODO handle wrapping
    if (sled_offset > 0)
        sled_offset--;
    int sled_offset_start = sled_offset;
    // we want x14 = x13 - min_id
    if (min_id <= 0xffffff) {
        // sub x14, x13, <imm>
        write_instruction(asm_sub_imm12(14, 13, min_id & 0xfff, 0));
        if (min_id > 0xfff) {
            write_instruction(asm_sub_imm12(14, 14, (min_id >> 12) & 0xfff, 1));
        }
    } else {
        // 1. load min_id in x15
        write_instruction_for_constant_load(15, min_id);
        // 2. x14 = x13 - x15  (sub x14, x13, x15)
        write_instruction(asm_sub(14, 13, 15));
    }
    // 3. if x14 > (max_id-min_id) branch
    if (max_id - min_id < 4096) {
        write_instruction(asm_cmp_imm(14, max_id - min_id, 0));
    } else {
        write_instruction_for_constant_load(15, max_id - min_id);
        write_instruction(asm_cmp_reg(14, 15));
    }
    write_instruction(asm_bhi_imm(2));
    // 4. if in range, direct branch to handler's address
    write_instruction(asm_b_imm(calculate_offset(sled_offset, handler) / 4));
    // 5. add branch to patch case
    write_instruction(asm_b_imm(SLED_SIZE - sled_offset + 1 + SLED_SIZE_NOVT + 1));


    // map branch sled as R-X (not W)
    mprotect(BRANCH_SLED, size, PROT_READ | PROT_EXEC);

    // Clear cache
    __clear_cache(&BRANCH_SLED[sled_offset_start-1], &BRANCH_SLED[sled_offset+1]);
}



/**
 * Register a handler function for C++ class IDs. Numbers between min_id and max_id must be sent to the given handler.
 * Number comes as usual in r13, while r14 and r15 are reserved for scratch space.
 */
void __attribute__((used)) __novt_register_handler(void *handler, uint64_t min_id, uint64_t max_id) {
    if (!BRANCH_SLED_NOVT) {
        // first call; BRANCH_SLED_NOVT is not setup. Make sure it points at the start of the actual sled
        asm volatile("adrp %0, __novt_default_handler_start\nadd %0, %0, :lo12:__novt_default_handler_start" : "=r"(BRANCH_SLED_NOVT));
    }

    // map branch sled as RW- (not X)
    uintptr_t page_start = ((uintptr_t) BRANCH_SLED_NOVT) & ~0x3fff;
    size_t size = ((uintptr_t) BRANCH_SLED_NOVT) - page_start + (SLED_SIZE_NOVT+1) * 4;
    size = (size + 0x3fff) & ~0x3fff;
    mprotect(page_start, size, PROT_READ | PROT_WRITE);

    //TODO handle wrapping
    if (sled_offset_novt > 0)
        sled_offset_novt--;
    int sled_offset_start = sled_offset_novt;
    // we want x14 = x13 - min_id
    if (min_id <= 0xffffff) {
        // sub x14, x13, <imm>
        write_instruction_novt(asm_sub_imm12(14, 13, min_id & 0xfff, 0));
        if (min_id > 0xfff) {
            write_instruction_novt(asm_sub_imm12(14, 14, (min_id >> 12) & 0xfff, 1));
        }
    } else {
        // 1. load min_id in x15
        write_instruction_for_constant_load_novt(15, min_id);
        // 2. x14 = x13 - x15  (sub x14, x13, x15)
        write_instruction_novt(asm_sub(14, 13, 15));
    }
    // 3. if x14 > (max_id-min_id) branch
    if (max_id - min_id < 4096) {
        write_instruction_novt(asm_cmp_imm(14, max_id - min_id, 0));
    } else {
        write_instruction_for_constant_load_novt(15, max_id - min_id);
        write_instruction_novt(asm_cmp_reg(14, 15));
    }
    write_instruction_novt(asm_bhi_imm(2));
    // 4. if in range, direct branch to handler's address
    write_instruction_novt(asm_b_imm(calculate_offset_novt(sled_offset_novt, handler) / 4));
    // 5. terminate otherwise
    write_instruction_novt(asm_trap());


    // map branch sled as R-X (not W)
    mprotect(page_start, size, PROT_READ | PROT_EXEC);

    // Clear cache
    __clear_cache(&BRANCH_SLED_NOVT[sled_offset_start-1], &BRANCH_SLED_NOVT[sled_offset_novt+1]);
}


// wrapper function for branch sled
// to avoid any function prologue / epiloge, the actual branch sled (__noic_default_handler) is wrapped
void static __attribute__((noinline, used, section(".text.__noic_default_handler")))  __wrap__noic_default_handler() {

    #define STR_INDR(x) #x
    #define STR(x) STR_INDR(x)

    asm volatile(

        // align to page boundary so mprotect works (Apple has 16KB pages)
        ".align 14\n"

        // make sure the branch sled label is accessible
        ".global __noic_default_handler;\n"
        ".type __noic_default_handler, %function;\n"
        
        // actual branch sled
        // when this is invoked, the target address is in x13.
        "__noic_default_handler:\n"
        "__noic_default_handler_start:\n"  // same address, but local (=PLT-less) symbol

        // start with an empty sled that instantly calls the patch procedure
        "b __noic_default_handler_patch\n"
        
        // space for all the entries
        ".space " STR(SLED_SIZE) "\n"
        ".space " STR(SLED_SIZE) "\n"
        ".space " STR(SLED_SIZE) "\n"
        ".space " STR(SLED_SIZE) "\n"

        // Same for NoVT
        ".global __novt_default_handler;\n"
        ".type __novt_default_handler, %function;\n"
        "__novt_default_handler:\n"
        "__novt_default_handler_start:\n"
        "brk 1\n"
        ".space " STR(SLED_SIZE_NOVT) "\n"
        ".space " STR(SLED_SIZE_NOVT) "\n"
        ".space " STR(SLED_SIZE_NOVT) "\n"
        ".space " STR(SLED_SIZE_NOVT) "\n"

        // this will be invoked, if address wasn't found.
        // We save all registers, call the function that patches the sled and then restore registers
        "__noic_default_handler_patch:\n"
        
        // save registers (yeah, there is probably a shorter way and not all need to be saved ...)
        "sub sp, sp, 256\n"
        "str x0, [sp, 0]\n"
        "str x1, [sp, 8]\n"
        "str x2, [sp, 16]\n"
        "str x3, [sp, 24]\n"
        "str x4, [sp, 32]\n"
        "str x5, [sp, 40]\n"
        "str x6, [sp, 48]\n"
        "str x7, [sp, 56]\n"
        "str x8, [sp, 64]\n"
        "str x9, [sp, 72]\n"
        "str x10, [sp, 80]\n"
        "str x11, [sp, 88]\n"
        "str x12, [sp, 96]\n"
        "str x13, [sp, 104]\n"
        "str x14, [sp, 112]\n"
        "str x15, [sp, 120]\n"
        "str x16, [sp, 128]\n"
        "str x17, [sp, 136]\n"
        "str x18, [sp, 144]\n"
        "str x19, [sp, 152]\n"
        "str x20, [sp, 160]\n"
        "str x21, [sp, 168]\n"
        "str x22, [sp, 176]\n"
        "str x23, [sp, 184]\n"
        "str x24, [sp, 192]\n"
        "str x25, [sp, 200]\n"
        "str x26, [sp, 208]\n"
        "str x27, [sp, 216]\n"
        "str x28, [sp, 224]\n"
        "str x29, [sp, 232]\n"
        "str x30, [sp, 240]\n"

        // patch function expects target as first argument
        "mov x0, x13\n"
        
        // call sled patching function
        "bl noic_patch_sled\n"
        
        // restore saved registers (again, there is probably a better way, but it works well enough)
        "ldr x0, [sp, 0]\n"
        "ldr x1, [sp, 8]\n"
        "ldr x2, [sp, 16]\n"
        "ldr x3, [sp, 24]\n"
        "ldr x4, [sp, 32]\n"
        "ldr x5, [sp, 40]\n"
        "ldr x6, [sp, 48]\n"
        "ldr x7, [sp, 56]\n"
        "ldr x8, [sp, 64]\n"
        "ldr x9, [sp, 72]\n"
        "ldr x10, [sp, 80]\n"
        "ldr x11, [sp, 88]\n"
        "ldr x12, [sp, 96]\n"
        "ldr x13, [sp, 104]\n"
        "ldr x14, [sp, 112]\n"
        "ldr x15, [sp, 120]\n"
        "ldr x16, [sp, 128]\n"
        "ldr x17, [sp, 136]\n"
        "ldr x18, [sp, 144]\n"
        "ldr x19, [sp, 152]\n"
        "ldr x20, [sp, 160]\n"
        "ldr x21, [sp, 168]\n"
        "ldr x22, [sp, 176]\n"
        "ldr x23, [sp, 184]\n"
        "ldr x24, [sp, 192]\n"
        "ldr x25, [sp, 200]\n"
        "ldr x26, [sp, 208]\n"
        "ldr x27, [sp, 216]\n"
        "ldr x28, [sp, 224]\n"
        "ldr x29, [sp, 232]\n"
        "ldr x30, [sp, 240]\n"
        "add sp, sp, 256\n"

        // now the entry should have been added and we can jump into the sled again!
        "b __noic_default_handler_start\n"

        // additional wrapper for locations that must serve the function pointer in x1
        ".global __noic_default_handler_for_tlsdesc\n"
        "__noic_default_handler_for_tlsdesc:"
        "stp x13, x14, [sp,#-16]!\n"  // preserve registers necessary for dispatch
        "stp x15, x30, [sp,#-16]!\n"
        "bl __noic_default_handler\n"  // dispatch
        "ldp x15, x30, [sp],#16\n"  // restore registers
        "ldp x13, x14, [sp],#16\n"
        "ret x30\n"
    );
}

#else /* not __aarch64__ */

// not aarch64 (probably x86)
void __attribute__((noinline, used, section(".text.__noic_default_handler")))  __noic_default_handler() {
    fputs("JIT does not work on this platform!", stderr);
    exit(666);
}

void __attribute__((noinline, used, section(".text.__novt_default_handler")))  __novt_default_handler() {
  fputs("JIT does not work on this platform!", stderr);
  exit(666);
}

void __attribute__((noinline, used, section(".text.__noic_default_handler"))) __noic_register_handler() {}
void __attribute__((noinline, used, section(".text.__novt_default_handler"))) __novt_register_handler() {}

#endif /* __aarch64__ */


