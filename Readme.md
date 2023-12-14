# Switchpoline: A Software Mitigation for Spectre-BTB and Spectre-BHB on ARMv8

## Reproducability
Switchpoline is a C/C++ compiler which protects ARMv8 programs from Spectre-BTB and Spectre-BHB attacks.
It is a fork of LLVM 10, including Clang and lld and can be used as a drop-in replacement for gcc or clang in many situations.
Along Switchpoline, we publish instructions to reproduce our benchmarks and to run our Spectre-BTB PoC.

With this Respository we verify that:
1. Switchpoline builds sucessfully
2. Switchpoline can compile simple programs
3. Switchpoline can build real-world applications, and they run sucessfully
4. Switchpoline can build the Spec 2017 benchmarks, they run sucessfully, and we evaluate them compared to an unprotected compiler
5. Our Micro-benchmarks work and are reproducable
6. Our Spectre-BTB Proof of Concept works and results are reproducable

<b>Requirements:</b>
Steps 1-2 can be performed on a x86-based system while steps 3-6 require an ARMv8-based system.

## 1. Compiling Switchpoline
(30 - 60 min.)

The first step is to compile Switchpoline and setup the build environment.
This can be done on both, an x86-,and an ARMv8-based system.
The built binaries, however, cannot be transferred between architectures.
Building Switchpoline is a requirement for all subsequent steps.

<details>
<summary>Details</summary>

For convenience, we provide a script that fully builds Switchpoline and sets up the build environment.
We tested the script on Ubuntu 22.04 (x86), Ubuntu 20.04 (arm), and Asahi Linux (arm).

### Prerequisites
To build switchpoline, some additional packages may be required depending on your setup.
On Ubuntu 22.04, we had to install the following additional packages:
* cmake
* clang
* lld
* python-is-python3
* python3-pip
* python3-distutils
* libstdc++-12-dev

### Commands

The script `complete-rebuild.sh` can be found in `switchpoline/scripts`.
It must be executed inside the `scripts` directory:
```
cd switchpoline/scripts
./complete-rebuild.sh
```

### Expected Results
Depending on the power of the machine, this step may take an considerable amount of time to compile llvm.
Once the script terminates, it should have created a few folders and files.
Here is a small subset of them:
* `switchpoline/sysroots/aarch64-linux-musl`: A system root for compiling Switchpoline protected applications
* `switchpoline/sysroots/aarch64-linux-musl-ref`: A system root for compiling applications with a reference compiler
* `switchpoline/llvm-novt/cmake-build-minsizerel/bin`: Folder containing the compiled llvm binaries
* `switchpoline/sysroots/aarch64-linux-musl/bin/my-clang`: A wrapper for `clang` to compile Switchpoline protected applications

### Troubleshooting
If any of these are missing, double-check all requirements are installed.
If you still encounter any problem, any error messages in the script output may lead you in the right direction.
In any case, feel free to open an issue.
We are happy to improve our tooling to support a wide range of different environments.

</details>

## 2. Compiling simple Programs
(~5 min.)

We provide a simple example applications for C and C++ to demonstrate how Swichpoline can remove indirect branches.
In this step, we show how these can be compiled, run, and how to verify that all indirect branches are eliminated.
This step can be performed on both, x86-, and ARMv8-based systems.

<details>
<summary>Details</summary>

To compile the example scripts, we provide a Makefile.
Feel free to play around with the example files!

### Prerequesites
In the following, we will use `objdump` to disassemble the binaries.
If you are running on x86, you may need to install the following:
* `qemu-aarch64-static`: An emulator capable of emulating aarch64 (package `qemu-user-static` on Ubuntu 22.04)
* `aarch64-linux-gnu-objdump`: A disassembler capable of working with aarch64 binaries (package `binutils-aarch64-linux-gnu` on Ubuntu 22.04)

### 2.1 Compiling the Examples
The example binaries can be compiled using the provided Makefile.
For this to work, step 1 must be completed successfully.
```
cd switchpoline/examples/simple_example
make
```
The Makefile will build four files:
* `example_c_protected`: The compilation of `example.c` with Switchpoline enabled
* `example_c_reference`: The compilation of `example.c` with a reference compiler
* `example_cpp_protected`: The compilation of `example.cpp` with Switchpoline enabled
* `example_cpp_reference`: The compilation of `example.cpp` with a reference compiler

By default, all files will be compiled with static linking and link-time optimization.
Dynamic linking can be enabled by removing `-static -flto` from `FLAGS` in the Makefile.

### 2.2 Running the Examples
On x86-based systems with `qemu-aarch64-static` installed, the binaries can be executed like this:
```
cd switchpoline/examples/simple_example
qemu-aarch64-static example_c_protected
qemu-aarch64-static example_c_reference
qemu-aarch64-static example_cpp_protected
qemu-aarch64-static example_cpp_reference
```
The output expected output is `Hello World!` and a termination with exit code 0.

### 2.3 Checking for indirect branches
To ensure compiling with Switchpoline enabled does not produce any indirect branches, you can use `objdump` to disassemble the binaries.
No `br` or `blr` instructions should be present in any `*_protected` binary:
```
objdump -d example_c_protected | grep -E "(\sbr\s|\sblr\s)"
objdump -d example_cpp_protected | grep -E "(\sbr\s|\sblr\s)"
```
We expect `grep` to not find any matches.
On an x86-based system, `objdump` may need to be replaced with `aarch64-linux-gnu-objdump`.

When searching for indirect branches the same way in the reference binaries, we expect to find some results:
```
objdump -d example_c_reference | grep -E "(\sbr\s|\sblr\s)"
objdump -d example_cpp_reference | grep -E "(\sbr\s|\sblr\s)"
```
Again, `objdump` may need to be replaced with `aarch64-linux-gnu-objdump`.

</details>

## 3. Building real-world applications
(~10 min.)

As example for two real-world applications that can be compiled using Switchpoline, we provide instructions to compile `lighttpd`.
However, Switchpoline should be able to compile most applications, if they fulfill the following requirements:
* all linked libraries can be compiled from source with Switchpoline enabled
* No inline assembly using indirect branches is contained in the sources
* The source code is complient with the C/C++ standard
* The total size after compilation (including the linked standard libraries) does not exceed a code-segment size of `128 MB`
* The code doesn't contain code which triggers the JIT component multiple times in parallel

Keep in mind that our implementation of Switchpoline is a proof of concept.
If you encounter any problems / bugs, feel free to open an issue, so we can investigate the problem and fix potential bugs.

We only tested this step on ARMv8-based systems.


<details>
<summary>Details</summary>

### 3.1 lighttpd
Lighttpd is a light-weight Http webserver.
We provide a script to compile lighttpd.
Our script uses static linking and adds all required plugins at compile time.
All required libraries are built and protected with Switchpoline.
To run the script, use the following commands:
```
cd switchpoline/examples/lighttpd
./compile.sh
```

To run lighttpd, we provide a sample configuration (`lighttpd.conf`):
```
cd switchpoline/examples/lighttpd/
./lighttpd-1.4.59/lighttpd-1.4.59-/src/lighttpd -D -f lighttpd.conf
```
The script will also compile a version of lighttpd that is dynamically linked:
```
cd switchpoline/examples/lighttpd/
./lighttpd-1.4.59/lighttpd-1.4.59-/src/lighttpd_dyn -D -f lighttpd.conf
```

You can now use curl to make a request:
```
curl http://localhost:9999/hello.txt
```
You should be greeted with a `Hello World!` message.

Alternatively, you can use a web-browser to navigate to `http://localhost:9999/hello.txt`.

</details>

## 4. Spec CPU 2017
(~1 day)

In this step we will detail how Spec2017 can be set up to benchmark Switchpoline and an unprotected compiler.
While running all benchmarks takes a significant amount of time, the script we provide can be modified to only run fewer benchmarks.

<details>
<summary>Details</summary>

### Prerequesites
To run the benchmarks, you neeed a copy of Spec CPU 2017.
In particular, you should have a file called `cpu2017-1_0_2.iso`.
This step requires up to 40GB of disk space and an ARMv8-based system.
Step 1 must be completed successfully to run this step.
We suggest using a system with at least 4 cores and 4GB of RAM for this step.

### Setting up Spec CPU 2017
First, the installation media must be extracted to `switchpoline/benchmarks/spec2017/install`.
For this, we first mount the iso-file, then copy its contents, and lastly make the contents writable:
```
# mount iso file
sudo mkdir /mnt/spec2017
sudo mount -o loop /path/to/cpu2017-1_0_2.iso /mnt/spec2017
# copy contents
mkdir switchpoline/benchmarks/spec2017/install
sudo cp -r /mnt/spec2017/* switchpoline/benchmarks/spec2017/install
# make it writable
sudo chmod +w -R switchpoline/benchmarks/spec2017/install
sudo chown -R your_user:your_user switchpoline/benchmarks/spec2017/install
# unmount iso file
sudo umount /mnt/spec2017
sudo rmdir /mnt/spec2017
```

### Running Spec CPU 2017
To run the benchmarks, we provide a python script: `switchpoline/benchmarks/spec2017/run.py`.
By default, this script creates two installations of Spec CPU 2017:
* `switchpoline/sysroots/aarch64-linux-musl/spec2017`: The installation used to benchmark Switchpoline
* `switchpoline/sysroots/aarch64-linux-musl-ref/spec2017`: The installation used to benchmark the reference compiler
There will be four different configurations:
* `clang-spectre-aarch64.cfg`: Switchpoline with static linking
* `clang-spectre-aarch64-dyn.cfg`: Switchpoline with dynamic linking
* `clang-spectre-aarch64-ref.cfg`: Reference compiler with static linking
* `clang-spectre-aarch64-dyn-ref.cfg`: Reference compiler with dynamic linking

If you want to reduce the benchmarks or configuration being run, you can modify the script accordingly.
To just run a quick test making sure that the benchmark compiles and runs successfully, but not providing accurate benchmark numbers, you can change `TEST=False` to `TEST=True` in line `6` of `run.py`. 
Running all benchmarks in test mode can still take up to several hours depending on the hardware.
Once you are done, you can run the script like this:
```
cd switchpoline/benchmarks/spec2017
python run.py
```
If all benchmarks are selected, running this script can take several days to complete.
The provided script also parses benchmark results and outputs them to `switchpoline/benchmarks/spec2017/output.txt`.
More detailed results produced by Spec Cpu 2017 can be found in the following folders:
* `switchpoline/sysroots/aarch64-linux-musl/spec2017/result`: Results for Switchpoline protected binaries
* `switchpoline/sysroots/aarch64-linux-musl-ref/spec2017/result`: Results for Reference compiler

</details>

## 5. Micro Benchmarks
(~20 min.)

Our micro benchmarks compare the execution time of indirect branches to branches re-written by Switchpoline.
For C function pointer calls, they also compare to the overhead of inserting different barriers before indirect branches.
Even though it is unclear whether this protects against Spectre-BTB.
Also, the `CSDB` barrier is a nop instruction on some older devices.

<details>
<summary>Details</summary>

### Prerequesites
Running the micro benchmarks requires an ARMv8-based system.
This step only works if step 1 completed successfully.
To visualize the results, `Matplotlib` is required (`pip install matplotlib`).

### Commands
We provide two scripts to run micro benchmarks.
* `switchpoline/benchmarks/micro_benchmarks/bench_indirect_call.py`: Benchmarks the performance of C function pointers
* `switchpoline/benchmarks/micro_benchmarks/bench_inheritance.py`: Benchmarks the performance of C++ virtual methods

You can run these scripts like this:
```
cd switchpoline/benchmarks/micro_benchmarks
python3 bench_indirect_call.py
python3 bench_inheritance.py
```
By default, both scripts will run benchmarks, save the results, and plot the results.
The results are written to `results_c.py` and `results_cpp.py` respectively.
In the plot, the x-axis represents the amount of possible targets for a branch, the y axis represents execution time.
Feel free to modify the scripts to fit your needs.
Especially, make sure to modify the value of the `CORE` variable to pin to the correct core.
If your linux distribution does not support `taskset` which is required by the scripts, you may modify the script and ensure pinning to the correct CPU core in a different manner.

### Expected Results
Everything except Switchpoline re-written branches should be a flat horizontal line.
The execution time of Switchpoline re-written branches should increase roughly logarithmic with the amount of possible targets.
The execution time of barrier protected branches should be significantly higher than the reference.
This may not be the case for `CSDB`.
If `CSDB` does not induce any overhead compared to the reference, it is like the processor doesn't support this instruction and it acts as a nop.
This can be verified using the Spectre-BTB Proof of Concept: If leakage remains after inserting `CSDB` before the leaking branch, it is likely a nop.
</details>

## 6. Spectre-BTB Proof of Concept
In this step we describe how to setup and run our Spectre-BTB Proof of Concept.
This Proof of Concept is based on https://github.com/cispa/BranchDifferent .
The eviction code is based on https://github.com/cgvwzq/evsets .

<details>
<summary>Details</summary>

### Prerequisites
Step 1 must be completed sucessfully.
This step should be performed on an ARMv8-based system.

### Configuring the Proof of Concept
The Proof of Concept allows for individual configuration according to your system.
Configuration can be done in `spectre_poc/spectre/config.h`.
Most notably, five settings must be considered:
* `CACHE`: cache control mechanism to use. Can be set to `FLUSHING` or `EVICTION`. Note that `FLUSHING` is not available on all devices. `EVICTION` requires additional configuration.
* `TIMER`: timing source to use. Can be set to `MSR`, `COUNTER_THREAD`, or `MONOTONIC`. `MONOTONIC` is most reliable (if available and high-enough frequency), `COUNTER_THREAD` should always be available.
* `PAGE_SIZE`: 16384 for most Apple devices, 4096 for most other devices
* `THRESHOLD` (further down): Threshold to distinguish cache hits from cache misses (a timing between cache hit and cache miss; Compile with `VERBOSITY` set to `999` and run the PoC to get debug output that allows infering the threshold.) 
* `BENCHMARK` (further down): Set to 0 to do a quick run. Set to 1, to perform a benchmark run.

If you rely on eviction, you may need to adjust additional settings:
* `EVICTION_THRESHOLD`: Threshold for eviction. Usually a bit higher than `THRESHOLD`. You can try commenting this out for auto-detection. (This may or may not work, depending on your device)
* `EVICTION_SET_SIZE`: Amount of addresses in the eviction set. A good value for reliable eviction is two times the amount of cache ways in the last cache level. (If you are unsure, just leave it at 32. This should be fine)
* `EVICTION_MEMORY_SIZE`: Amount of memory allocated for finding eviction sets. The default value works on all devices we tested.

Finding thresholds that work (especially if you rely on eviction) may require some amount of trial and error.
The Proof of Concept worked on the high-performance cores of all ARMv8 devices we tested.

### Compiling the Proof of Concept
To compile the proof of concept, we provide a Makefile:
```
cd spectre_poc/spectre
make
```

This will produce the binary `spectre_poc`.

After changing the configuration, a new build is necessary.
For this, you can run `make` (even existing binaries will be re-compiled).

### Advanced Configuration: Barriers
The Proof of Concept can be configured to add an assembly snippet just before the indirect branch.
Using this feature will make the Proof of Concept rely on inline assembly for the indirect branch.
Thus, it cannot be protected using Switchpoline.
This configuration is mainly available to test whether inserting barriers before the branch stops leakage.
Common barrier instructions are `CSDB`, `DMB SY`, `DSB SY` (refer to the ARMv8 instruction reference for details).
To insert assembly code just before the indirect branch instruction, you can set `MITIGATION_ASM` in `sepctre_poc/spectre/config.h`. 

</details>
