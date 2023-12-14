import sys 
import os

import time

TEST=False
OUT=open("output.txt", "w")

def info(msg):
    print(f"[O] {msg}")

def warning(msg):
    print(f"[!] {msg}")

def spec_command(path, command):
    os.system(f"cd {path}/spec2017; bash -c 'source shrc; {command}'")
    
def install_spec(path):
    if os.path.exists(f"{path}/spec2017"):
        info(f"Spec seems to be installed (path={path})") 
        return
        
    info(f"installing spec to {path}")
    
    os.system(f"echo yes | {os.path.abspath('./install')}/install.sh -d {path}/spec2017")
    
    os.system(f"cp 5*.tar.xz {path}/spec2017/")
    
    info("installing patches")
    spec_command(path, "echo y | runcpu --update")
    spec_command(path, "specxz -dc 526.blender_r.blender_musl.cpu2017.v1.1.0.tar.xz | spectar -xvf -")
    spec_command(path, "specxz -dc 520.omnetpp_r.omnetpp_cfi_violations.cpu2017.v1.1.0.tar.xz | spectar -xvf -")
    
    info("patches installed")
    info("installing configs")
    os.system(f"cp config/* {path}/spec2017/config/")    

def get_latest_result_file(path):
    highest = 0
    highest_name = None
    for name in os.listdir(f"{path}/spec2017/result"):
        if not name.startswith("CPU2017.") or not name.endswith(".txt"):
            continue
        number = int(name.split(".")[1])
        if number > highest:
            highest = number
            highest_name = name
    return f"{path}/spec2017/result/{highest_name}"

def get_executable(path, config, benchmark):
    file_path = f"{path}/spec2017/benchspec/CPU/{benchmark}/build/"
    if benchmark == "523.xalancbmk_r":
        benchmark = benchmark[:4] + "cpuxalan_r"
    for file_name in os.listdir(file_path):
        if file_name.startswith("build_base") and f"{config}-m64" in file_name:
            return f"{file_path}/{file_name}/{benchmark[4:]}"
    return None

def get_latest_log_file(path):
    highest = 0
    highest_name = None
    for name in os.listdir(f"{path}/spec2017/result"):
        if not name.startswith("CPU2017.") or not name.endswith(".log"):
            continue
        number = int(name.split(".")[1])
        if number > highest:
            highest = number
            highest_name = name
    return f"{path}/spec2017/result/{highest_name}"

def cleanup(path):
    spec_command(path, f"runcpu --action scrub")

def build_benchmarks(path, config, benchmarks):
    results = dict()
    for benchmark in benchmarks:
        start = time.time()
        spec_command(path, f"runcpu -c {config} --action build --rebuild {benchmark}")
        end = time.time()
        results[benchmark] = end - start
    return results

def log(config, benchmark, message):
    global OUT
    OUT.write(f"{config} | {benchmark} | {message}\n")
    OUT.flush()

def run_benchmarks(path, config, benchmarks):
    global TEST
    install_spec(path)
    cleanup(path)
    for benchmark in benchmarks:
        build_times = build_benchmarks(path, config, [benchmark])
        log(config, benchmark, f"[BUILD TIME]: {build_times}")
        file_path = get_executable(path, config, benchmark)
        print(f"Executable path: {file_path}")
        file_size = os.path.getsize(file_path) if file_path else 0
        if os.path.exists(f"{file_path}-ids.txt"):
            ids = []
            for line in open(f"{file_path}-ids.txt").read().split("\n"):
                if line.startswith("Targets"):
                    ids.append(int(line.split("] ")[1]))
            log(config, benchmark, f"[TARGETS C]: {ids}")
        if os.path.exists(f"{file_path}-ids_cpp.txt"):
            ids_cpp = []
            for line in open(f"{file_path}-ids_cpp.txt").read().split("\n"):
                if line:
                    ids_cpp.append(int(line.split(" ")[2]))
            log(config, benchmark, f"[TARGETS C++]: {ids_cpp}")

        log(config, benchmark, f"[BINARY SIZE]: {file_size}")
        spec_command(path, f"runcpu -c {config} {'--size=test ' if TEST else ''}{benchmark}")
        result_file = get_latest_result_file(path)
        content = open(result_file, "r").read()
        for line in content.split("\n"):
            if line.startswith(benchmark):
                log(config, benchmark, f"[RESULT]: {line}")

def sysroot(name):
    return os.path.abspath(f"../../sysroots/{name}")
    
benchmarks = [
    "523.xalancbmk_r",
    "623.xalancbmk_s",
    "520.omnetpp_r",
    "620.omnetpp_s",
    "500.perlbench_r",
    "600.perlbench_s",
    "505.mcf_r",
    "605.mcf_s",
    "525.x264_r",
    "531.deepsjeng_r",
    "631.deepsjeng_s",	
    "541.leela_r",
    "641.leela_s",
    "557.xz_r",
    "657.xz_s",
    "508.namd_r",
    "511.povray_r",
    "519.lbm_r",
    "619.lbm_s",
    "526.blender_r",
    "538.imagick_r",
    "638.imagick_s",
    "544.nab_r",
    "644.nab_s"
]

benchmarks_dynamic = benchmarks + ["510.parest_r"]

run_benchmarks(sysroot("aarch64-linux-musl")    , "switchpoline-static" , benchmarks)
run_benchmarks(sysroot("aarch64-linux-musl")    , "switchpoline-dynamic", benchmarks_dynamic)
run_benchmarks(sysroot("aarch64-linux-musl-ref"), "reference-static"    , benchmarks)
run_benchmarks(sysroot("aarch64-linux-musl-ref"), "reference-dynamic"   , benchmarks_dynamic)
