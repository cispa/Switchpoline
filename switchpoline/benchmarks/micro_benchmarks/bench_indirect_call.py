# --- settings ---
# maximum amount of possible targets
AMOUNT = 100

# core to pin benchmark to
CORE = 0

# if set to True: run benchmarks
RUN_TESTS = True

# if set to True: plot results (requires matplotlib)
PLOT_RESULTS = True


# --- configurations ---
CC_REFERENCE = "../../sysroots/aarch64-linux-musl-ref/bin/my-clang"
CC_PROTECTED = "../../sysroots/aarch64-linux-musl/bin/my-clang"

# switchpoline enabled compiler
SWITCHPOLINE = (
    CC_PROTECTED,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    'x(0,0,0)',
    AMOUNT
)

# unprotected compiler
REFERENCE = (
    CC_REFERENCE,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    "x(0,0,0)",
    AMOUNT
)

# inline assembly for indirect call (blr instruction)
ASM_CALL = (
    CC_REFERENCE,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    'asm volatile("blr %0" :: "r" (x) : "x30")',
    AMOUNT
)

# inline assembly for indirect call and prefixed barrier (dsb instruction)
DSB = (
    CC_REFERENCE,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    'asm volatile("DSB SY\\nblr %0" :: "r" (x) : "x30")',
    AMOUNT
)

# inline assembly for indirect call and prefixed barrier (dmb instruction)
DMB = (
    CC_REFERENCE,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    'asm volatile("DMB SY\\nblr %0" :: "r" (x) : "x30")',
    AMOUNT
)

# inline assembly for indirect call and prefixed barrier (csdb instruction)
CSDB = (
    CC_REFERENCE,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    'asm volatile("CSDB\\nblr %0" :: "r" (x) : "x30")',
    AMOUNT
)


SUITE = {
    "unprotected" : REFERENCE,
    "call_asm" : ASM_CALL,
    "dsb" : DSB,
    "dmb" : DMB,
    "csdb" : CSDB,
    "protected": SWITCHPOLINE
}

# --- actual code ---
import subprocess

def write_template(amount, call):
    with open("indirect_call.h", "w") as f:
        f.write(f"#define CALL(x) {call}\n")
        f.write('#define FUNC(x) void func_##x(int a, int b, int c){asm volatile("nop");}\n')
        
        for i in range(amount):
            f.write(f"FUNC({i})\n")
        
        f.write("void assign_targets(){\n")
        
        for i in range(256):
            f.write(f"    targets[{i}] = func_{i % amount};\n")
        
        f.write("}\n");
        
def run_benchmark(CC, flags, core):
    subprocess.call([CC, "-o", "test_c"] + flags + ["indirect_call.c"])
    results = list(map(int, subprocess.check_output(["taskset", "-c", f"{core}", "./test_c"]).decode().split("\n")[:-1]))
    return sorted(results)[len(results) // 2]
    
def benchmark(CC, flags, core, call, max_amount):
    results = []
    for i in range(max_amount):
        write_template(i+1, call)
        results.append(run_benchmark(CC, flags, core))
    return results

def run_suit(suites):
    with open("results_c.py", "w") as f:
        for name, suite in suites.items():
            result = benchmark(*suite)
            f.write(f"results__{name} = {result}\n")
            
def plot():
    import results_c
    import matplotlib.pyplot as plt
    for name in results_c.__dict__:
        if "results__" in name:
            res = eval(f"results_c.{name}")
            plt.plot(range(1, len(res) + 1), res, label = name[9:])
    plt.legend()
    plt.savefig('indirect_branches.pdf')

if __name__ == "__main__":
    if RUN_TESTS:
        run_suit(SUITE)
    if PLOT_RESULTS:
        plot()
