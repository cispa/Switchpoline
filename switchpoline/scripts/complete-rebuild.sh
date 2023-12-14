ROOT=$(readlink -f "$(dirname $0)/..")

export CC=clang
export CXX=clang++

bash "$ROOT/scripts/complete-cleanup.sh"

echo "compiling LLVM"
cd "$ROOT/llvm-novt"
mkdir "cmake-build-minsizerel"
cd "cmake-build-minsizerel"
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVM_TARGETS_TO_BUILD="X86;AArch64;ARM" -DLLVM_INCLUDE_EXAMPLES=Off -DLLVM_INCLUDE_TESTS=Off -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_ENABLE_IDE=On -DBUILD_SHARED_LIBS=On -DLLVM_OPTIMIZED_TABLEGEN=On -DLLVM_BUILD_TESTS=OFF -DLLVM_USE_LINKER=lld
make -j6 clang lld llvm-config llvm-ar llvm-ranlib llvm-dis opt llc
echo "OK"

echo "building sysroots"
cd "$ROOT/scripts"
bash build-libraries.sh
echo "OK"
