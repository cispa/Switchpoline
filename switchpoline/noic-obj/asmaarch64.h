#include <inttypes.h>

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/ADRP--Form-PC-relative-address-to-4KB-page-?lang=en    
// calculate 4 KB page relative to instruction pointer
// reg <- pc + (target << 12)
static uint32_t __attribute__((used)) asm_addrp(int reg, int target){
    uint32_t instr = 0b10010000000000000000000000000000;
    instr |= reg;
    instr |= ((target & (~0b11)) & 0b111111111111111111111) << (5 - 2);
    instr |= (target & 0b11) << 29;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/ADD--immediate---Add--immediate--?lang=en    
// add 12 bit immediate to register. Optional shift by 12 bit
// dest_reg <- src_reg + (value << (shift ? 12 : 0))
static uint32_t __attribute__((used)) asm_add_imm12(int dest_reg, int src_reg, unsigned value, int shift){
    uint32_t instr = 0b10010001000000000000000000000000;    
    instr |= dest_reg;
    instr |= src_reg << 5;
    instr |= value << 10;
    instr |= shift << 22;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/SUB--immediate---Subtract--immediate--?lang=en
// sub 12 bit immediate from register. Optional shift by 12 bit
// dest_reg <- src_reg - (value << (shift ? 12 : 0))
static uint32_t __attribute__((used)) asm_sub_imm12(int dest_reg, int src_reg, unsigned value, int shift){
    uint32_t instr = 0b01010001000000000000000000000000;
    instr |= dest_reg;
    instr |= src_reg << 5;
    instr |= value << 10;
    instr |= shift << 22;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/SUB--shifted-register---Subtract--shifted-register--?lang=en
// add 12 bit immediate to register. Optional shift by 12 bit
// dest_reg <- src_reg_1 - src_reg_2
static uint32_t __attribute__((used)) asm_sub(int dest_reg, int src_reg_1, int src_reg_2){
    uint32_t instr = 0b11001011000000000000000000000000;
    instr |= dest_reg;
    instr |= src_reg_1 << 5;
    instr |= src_reg_2 << 16;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/CMP--shifted-register---Compare--shifted-register---an-alias-of-SUBS--shifted-register--?lang=en
// compare two registers, sets some flags (use with b.cond)
static uint32_t __attribute__((used)) asm_cmp_reg(int reg_a, int reg_b){
    uint32_t instr = 0b11101011000000000000000000011111;
    instr |= reg_a << 5;
    instr |= reg_b << 16;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/CMP--immediate---Compare--immediate---an-alias-of-SUBS--immediate--
// compare a register and an immediate, sets some flags (use with b.cond)
static uint32_t __attribute__((used)) asm_cmp_imm(int reg_a, unsigned value, int shift){
    uint32_t instr = 0b11110001000000000000000000011111;
    instr |= reg_a << 5;
    instr |= value << 10;
    instr |= shift << 22;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/B-cond--Branch-conditionally-?lang=en
// branch to offset if some flags are set that indicate not equal (use with cmp)
// if(was not equal)
//    pc <- offset * 4
// offset is in instructions!!!
static uint32_t __attribute__((used)) asm_bne_imm(int offset){
    uint32_t instr = 0b01010100000000000000000000000001;
    instr |= (offset & 0b1111111111111111111) << 5;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/B-cond--Branch-conditionally-?lang=en
// branch to offset if some flags are set that indicate higher than (use with cmp)
// if(x > y)
//    pc <- offset * 4
// offset is in instructions!!!
static uint32_t __attribute__((used)) asm_bhi_imm(int offset){
    uint32_t instr = 0b01010100000000000000000000001000;
    instr |= (offset & 0b1111111111111111111) << 5;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/B--Branch-?lang=en
// unconditional relative jump
// pc <- offset * 4
// offset is in instructions!!!
static uint32_t __attribute__((used)) asm_b_imm(int offset){
    uint32_t instr = 0b00010100000000000000000000000000;    
    instr |= (offset & 0b11111111111111111111111111);
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/MOV--wide-immediate---Move--wide-immediate---an-alias-of-MOVZ-
// move immediate to register
// dest_reg <- imm
static uint32_t __attribute__((used)) asm_mov_imm(int dest_reg, unsigned imm){
    uint32_t instr = 0b11010010100000000000000000000000;
    instr |= dest_reg;
    instr |= (imm & 0xffff) << 5;
    return instr;
}

// https://developer.arm.com/documentation/ddi0596/2021-12/Base-Instructions/MOVK--Move-wide-with-keep-
// move immediate to register, leaving other bits unchanged
// dest_reg <- dest_reg overwritten by (imm << (16*shift))
static uint32_t __attribute__((used)) asm_movk_imm(int dest_reg, unsigned imm, int shift){
    uint32_t instr = 0b11110010100000000000000000000000;
    instr |= dest_reg;
    instr |= (imm & 0xffff) << 5;
    instr |= (shift / 16) << 21;
    return instr;
}

// brk 1
static uint32_t asm_trap() {
  return 0xd4200020;
}
