#!/usr/bin/env bash

set -eu

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
ROOT=$(dirname "$SCRIPTPATH")
CLANG_BIN="$ROOT/build/bin"
if [ "$ROOT/llvm-novt/cmake-build-minsizerel/bin/clang" -nt "$CLANG_BIN/clang" ]; then
  CLANG_BIN=$ROOT/llvm-novt/cmake-build-minsizerel/bin
fi


FLAGS="-fPIC"
SYSROOT="$ROOT/sysroots/x86_64-linux-musl"
$SYSROOT/bin/my-clang -c $ROOT/noic-obj/patchpoline_jit.c -o $SYSROOT/lib/patchpoline_jit.o $FLAGS -fno-stack-protector -fomit-frame-pointer -O3
$SYSROOT/bin/my-clang -c $ROOT/noic-obj/patchpoline_plt.c -o $SYSROOT/lib/patchpoline_plt.o $FLAGS -fno-stack-protector -fomit-frame-pointer -O3

FLAGS="--target=aarch64-linux-musl -fPIC"
SYSROOT="$ROOT/sysroots/aarch64-linux-musl"
$SYSROOT/bin/my-clang -c $ROOT/noic-obj/patchpoline_jit.c -o $SYSROOT/lib/patchpoline_jit.o $FLAGS -fno-stack-protector -fomit-frame-pointer -O3
$SYSROOT/bin/my-clang -c $ROOT/noic-obj/patchpoline_plt.c -o $SYSROOT/lib/patchpoline_plt.o $FLAGS -fno-stack-protector -fomit-frame-pointer -O3

echo OK

#$CLANG_BIN/clang -c $SCRIPTPATH/patchpoline_jit.c -o $SCRIPTPATH/patchpoline_jit.o $FLAGS -fno-stack-protector -O3
#$CLANG_BIN/llvm-ar -crs $SYSROOT/lib/libpatchpoline_jit.a $SCRIPTPATH/patchpoline_jit.o
