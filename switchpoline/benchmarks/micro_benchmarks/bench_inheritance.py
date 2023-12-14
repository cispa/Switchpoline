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
CXX_REFERENCE = "../../sysroots/aarch64-linux-musl-ref/bin/my-clang++"
CXX_PROTECTED = "../../sysroots/aarch64-linux-musl/bin/my-clang++"

# switchpoline enabled compiler
SWITCHPOLINE = (
    CXX_PROTECTED,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    'x->func()',
    AMOUNT
)

# unprotected compiler
REFERENCE = (
    CXX_REFERENCE,
    ["-O3", "--target=aarch64-linux-musl"],
    CORE,
    "x->func()",
    AMOUNT
)

SUITE = {
    "protected": SWITCHPOLINE,
    "unprotected": REFERENCE
}

# --- actual code ---
import subprocess

def write_template(amount, call):
    with open("indirect_call.hpp", "w") as f:
        f.write(f"#define CALL(x) {call}\n")
        f.write('#define CLASS(x) class Base_##x : public Base{ virtual void func() {asm volatile("nop");}}\n')
        
        for i in range(amount):
            f.write(f"CLASS({i});\n")
        
        f.write("void assign_targets(){\n")
        
        for i in range(256):
            f.write(f"    targets[{i}] = new Base_{i % amount}();\n")
        
        f.write("}\n");
        
def run_benchmark(CC, flags, core):
    subprocess.call([CC, "-o", "test_cpp"] + flags + ["inheritance.cpp"])
    results = list(map(int, subprocess.check_output(["taskset", "-c", f"{core}", "./test_cpp"]).decode().split("\n")[:-1]))
    return sorted(results)[len(results) // 2]
    
def benchmark(CC, flags, core, call, max_amount):
    results = []
    for i in range(max_amount):
        write_template(i+1, call)
        results.append(run_benchmark(CC, flags, core))
    return results

def run_suit(suites):
    with open("results_cpp.py", "w") as f:
        for name, suite in suites.items():
            result = benchmark(*suite)
            f.write(f"results__{name} = {result}\n")
            
def plot():
    import results_cpp
    import matplotlib.pyplot as plt
    for name in results_cpp.__dict__:
        if "results__" in name:
            res = eval(f"results_cpp.{name}")
            plt.plot(range(1, len(res) + 1), res, label = name[9:])
    plt.legend()
    plt.savefig("virtual_functions.pdf")

if __name__ == "__main__":
    if RUN_TESTS:
        run_suit(SUITE)
    if PLOT_RESULTS:
        plot()
