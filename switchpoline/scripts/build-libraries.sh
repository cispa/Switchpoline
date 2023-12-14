#!/usr/bin/env bash

set -eu

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
ROOT=$(dirname "$SCRIPTPATH")
CLANG_BIN="$ROOT/build/bin"
if [ "$ROOT/llvm-novt/cmake-build-minsizerel/bin/clang" -nt "$CLANG_BIN/clang" ]; then
  CLANG_BIN=$ROOT/llvm-novt/cmake-build-minsizerel/bin
fi
echo "Clang directory: $CLANG_BIN"
[ -f $CLANG_BIN/llvm-ar ] || (echo "Please build targets: llvm-ar llvm-ranlib llvm-config" ; exit 1)
[ -f $CLANG_BIN/llvm-ranlib ] || (echo "Please build targets: llvm-ar llvm-ranlib llvm-config" ; exit 1)
[ -f $CLANG_BIN/llvm-config ] || (echo "Please build targets: llvm-ar llvm-ranlib llvm-config" ; exit 1)
# we also need llc, but that can come from the system
LLC_BINARY=llc-10
if ! $LLC_BINARY --version > /dev/null; then
  LLC_BINARY="$CLANG_BIN/llc"
  if ! $LLC_BINARY --version > /dev/null; then
    echo "Please build target: llc"
    exit 1
  fi
fi


# check for some dependencies
if python --version 2>&1 | grep -q 'Python 2.'; then
  echo "Your 'python' is python2, which is problematic for LLVM."
  echo "On ubuntu, please install \"python-is-python3\""
  exit 1
fi


# download source bundles
cd $ROOT/sysroots
if [ ! -f 'musl-1.2.3.tar.gz' ]; then
  wget 'https://musl.libc.org/releases/musl-1.2.3.tar.gz'
fi
if [ ! -f 'libcxxabi-10.0.1.src.tar.xz' ]; then
  wget 'https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/libcxxabi-10.0.1.src.tar.xz'
fi
if [ ! -f 'libunwind-10.0.1.src.tar.xz' ]; then
  wget 'https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/libunwind-10.0.1.src.tar.xz'
fi
if [ ! -f 'libcxx-10.0.1.src.tar.xz' ]; then
  wget 'https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/libcxx-10.0.1.src.tar.xz'
fi
if [ ! -f 'compiler-rt-10.0.1.src.tar.xz' ]; then
  wget 'https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/compiler-rt-10.0.1.src.tar.xz'
fi


# === Build instruction ===

install_deb() {  # <url> <filename>
  mkdir -p "$SYSROOT"
  WORKDIR="$SYSROOT-work"
  mkdir -p "$WORKDIR"
  cd "$WORKDIR"
  if [ ! -f "$2" ]; then
    wget "$1"
  fi
  mkdir -p tmp
  cd tmp
  ar x "../$2"
  tar -xf data.tar.xz -C "$SYSROOT"
  cd ..
  rm -rf tmp
}

download_and_install_deb() {
  package_name_for_regex="${1/+/\\+}"
  # download package index
  if [ ! -f /tmp/Packages-Sysroots-$DEB_ARCH ]; then
    wget "http://ftp.de.debian.org/debian/dists/bullseye/main/binary-$DEB_ARCH/Packages.gz" -O/tmp/Packages-Sysroots-$DEB_ARCH.gz
    gzip -d /tmp/Packages-Sysroots-$DEB_ARCH.gz
  fi
  # parse the package index
  url=$(grep -Pzo '(?s)Package: '"$package_name_for_regex"'\n.*?\n\n' /tmp/Packages-Sysroots-$DEB_ARCH | grep -Poa '^Filename: \K.*$' || true)
  if [ -z "$url" ]; then
    echo "Package not found: $1 ($DEB_ARCH)"
    return 1
  fi
  # download and install the package
  url="http://ftp.de.debian.org/debian/$url"
  echo "[+] Installing $(basename $url) from '$url' ..."
  install_deb "$url" "$(basename $url)"
}

make_musl_libc() {
  mkdir -p "$SYSROOT"
  WORKDIR="$SYSROOT-work"
  mkdir -p "$WORKDIR"
  cd "$WORKDIR"
  tar -xf $ROOT/sysroots/musl-1.2.3.tar.gz
  cd musl-1.2.3

  # configure compiler if necessary
  if [ ! -z "$TG_FLAGS" ]; then
    echo '#!/bin/sh' > $SYSROOT/compiler-config.sh
    for flag in $TG_FLAGS; do
      echo "export $flag" >> $SYSROOT/compiler-config.sh
    done
    echo 'exec "$@"' >> $SYSROOT/compiler-config.sh
    chmod +x $SYSROOT/compiler-config.sh
  fi
  if [ "$1" = "compilerrt" ] && [ "$NOIC_ACTIVE" = "1" ] && [ "$TARGET" = "aarch64-linux-musl" ]; then
    echo '#!/bin/sh' > $SYSROOT/compiler-config-stdlib.sh
    if [ ! -z "$TG_FLAGS" ]; then
      for flag in $TG_FLAGS; do
        echo "export $flag" >> $SYSROOT/compiler-config-stdlib.sh
      done
    fi
    echo "export TG_DYNAMIC_LINKING=1" >> $SYSROOT/compiler-config-stdlib.sh
    echo "export TG_ENFORCE_JIT=1" >> $SYSROOT/compiler-config-stdlib.sh
    echo 'exec "$@"' >> $SYSROOT/compiler-config-stdlib.sh
    chmod +x $SYSROOT/compiler-config-stdlib.sh
  fi

  # musl patches
  sed -i 's|#define VDSO_|// #define VDSO_|' arch/x86_64/syscall_arch.h
  sed -i 's|#define VDSO_|// #define VDSO_|' arch/aarch64/syscall_arch.h

  # patches for NOIC
  if [ "$NOIC_ACTIVE" = "1" ]; then
    sed -i 's|blr x1|bl __noic_handler_clone|' src/thread/aarch64/clone.s
    sed -i 's|call \*%r9|mov %r9, %rsi ; call __noic_handler_clone|' src/thread/x86_64/clone.s
    sed -i 's|br x30|ret x30|' src/setjmp/aarch64/longjmp.s
    sed -i 's|br %0|ret %0|' arch/aarch64/reloc.h
    sed -i 's|ksa.handler = sa->sa_handler;|ksa.handler = (sa->sa_flags \& SA_SIGINFO) ? __noic_resolve_sig2(sa->sa_sigaction) : __noic_resolve_sig1(sa->sa_handler);|' src/signal/sigaction.c
    sed -i 's|static int unmask_done;|void *__noic_resolve_sig1(void (*x)(int));\nvoid *__noic_resolve_sig2(void (*x)(int, siginfo_t *, void *));\nstatic int unmask_done;|' src/signal/sigaction.c

    # install full-file patches
    cp -rp "$ROOT"/musl-patches/* ./
  fi

  optional=
  if test "$TARGET"; then
    optional="--target $TARGET"
  fi
  if [ "$1" = "compilerrt" ] && [ "$NOIC_ACTIVE" = "1" ] && [ "$TARGET" = "aarch64-linux-musl" ]; then
    export CC="$SYSROOT/compiler-config-stdlib.sh $CLANG_BIN/clang"
  elif [ -z "$TG_FLAGS" ]; then
    export CC=$CLANG_BIN/clang
  else
    export CC="$SYSROOT/compiler-config.sh $CLANG_BIN/clang"
  fi
  export AR=$CLANG_BIN/llvm-ar
  export RANLIB=$CLANG_BIN/llvm-ranlib
  export CFLAGS="$FLAGS"
  export LDFLAGS="--sysroot $SYSROOT $LINK_FLAGS"
  if [ "$1" = "compilerrt" ]; then
    export LIBCC="$SYSROOT/lib/linux/libclang_rt.builtins-$ARCH.a"
    export CFLAGS="$FLAGS --rtlib=compiler-rt"
    export LDFLAGS="$LDFLAGS $WORKDIR/compilerrt-build/lib/builtins/CMakeFiles/clang_rt.builtins-$ARCH.dir/mul*.c.o"
  else
    unset LIBCC
  fi
  ./configure --prefix="$SYSROOT" --syslibdir="$SYSROOT/lib" $optional
  make -j$(nproc)
  make install

  sed -i 's|print-prog-name=ld|print-prog-name=lld|' $SYSROOT/bin/ld.musl-clang
  cp $SYSROOT/bin/musl-clang $SYSROOT/bin/musl-clang++
  cp $SYSROOT/bin/ld.musl-clang $SYSROOT/bin/ld.musl-clang++
  sed -i 's|clang|clang++|' $SYSROOT/bin/musl-clang++
  sed -i 's|clang|clang++|' $SYSROOT/bin/ld.musl-clang++
  sed -i 's|-nostdinc |-nostdinc -nostdinc++ |' $SYSROOT/bin/musl-clang++

  echo "[OK] Build musl libc in $SYSROOT ($1)"
  echo -e "\n\n"
}

make_clang_scripts() {
  # these scripts replace clang/lld, they modify some parameters before invoking the actual compiler

  cp "$ROOT/scripts/clang-wrapper.sh" "$SYSROOT/bin/my-clang"
  cp "$ROOT/scripts/clang-wrapper.sh" "$SYSROOT/bin/my-clang++"
  cp "$ROOT/scripts/ld-wrapper.sh" "$SYSROOT/bin/ld.my-clang"

  if [ -z "$TG_FLAGS" ]; then
    sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$CLANG_BIN|" "$SYSROOT/bin/my-clang"
    sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$CLANG_BIN|" "$SYSROOT/bin/my-clang++"
    sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$CLANG_BIN|" "$SYSROOT/bin/ld.my-clang"
  else
    sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$SYSROOT/compiler-config.sh $CLANG_BIN|" "$SYSROOT/bin/my-clang"
    sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$SYSROOT/compiler-config.sh $CLANG_BIN|" "$SYSROOT/bin/my-clang++"
    sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$SYSROOT/compiler-config.sh $CLANG_BIN|" "$SYSROOT/bin/ld.my-clang"
    for flag in $TG_FLAGS; do
      sed -i "s|#TG_CONFIG_FLAGS|export $flag\n#TG_CONFIG_FLAGS|" "$SYSROOT/bin/my-clang"
      sed -i "s|#TG_CONFIG_FLAGS|export $flag\n#TG_CONFIG_FLAGS|" "$SYSROOT/bin/my-clang++"
      sed -i "s|#TG_CONFIG_FLAGS|export $flag\n#TG_CONFIG_FLAGS|" "$SYSROOT/bin/ld.my-clang"
    done
  fi

  if [ "$1" == "compilerrt" ]; then
    sed -i "s|use_compilerrt=0|use_compilerrt=1|" "$SYSROOT/bin/my-clang"
    sed -i "s|use_compilerrt=0|use_compilerrt=1|" "$SYSROOT/bin/my-clang++"
  fi

  # for compilling the stdlib we need a better configuration script:
  cp $SYSROOT/bin/my-clang $SYSROOT/bin/my-clang-stdlib
  sed -i "s|#TG_CONFIG_FLAGS|export TG_DYNAMIC_LINKING=1\nexport TG_ENFORCE_JIT=1\n#TG_CONFIG_FLAGS|" "$SYSROOT/bin/my-clang-stdlib"
}

make_clang_ref_scripts() {
  mkdir -p "$SYSROOT/bin"

  cp "$ROOT/scripts/clang-wrapper-ref.sh" "$SYSROOT/bin/my-clang"
  cp "$ROOT/scripts/clang-wrapper-ref.sh" "$SYSROOT/bin/my-clang++"

  sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$CLANG_BIN|" "$SYSROOT/bin/my-clang"
  sed -i "s|/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin|$CLANG_BIN|" "$SYSROOT/bin/my-clang++"
  sed -i "s|x86_64-linux-gnu|$DEB_TARGET|" "$SYSROOT/bin/my-clang"
  sed -i "s|x86_64-linux-gnu|$DEB_TARGET|" "$SYSROOT/bin/my-clang++"
}

download_dependencies() {
  download_and_install_deb libgcc-10-dev
  download_and_install_deb linux-libc-dev
}

make_libcxx() {
  mkdir -p "$SYSROOT"
  WORKDIR="$SYSROOT-work"
  mkdir -p "$WORKDIR"
  cd "$WORKDIR"
  tar -xf $ROOT/sysroots/libcxxabi-10.0.1.src.tar.xz
  tar -xf $ROOT/sysroots/libcxx-10.0.1.src.tar.xz
  tar -xf $ROOT/sysroots/libunwind-10.0.1.src.tar.xz

  export CC=$SYSROOT/bin/my-clang
  export CXX=$SYSROOT/bin/my-clang
  export AR=$CLANG_BIN/llvm-ar
  export RANLIB=$CLANG_BIN/llvm-ranlib
  export CFLAGS="$FLAGS"
  export CXXFLAGS="$FLAGS"
  export LDFLAGS="--sysroot $SYSROOT $LINK_FLAGS"

  if [ "$1" == "compilerrt" ]; then
    COMPILER_RT_ENABLED=On
  else
    COMPILER_RT_ENABLED=Off
  fi

  if [ "$1" = "compilerrt" ] && [ "$NOIC_ACTIVE" = "1" ] && [ "$TARGET" = "aarch64-linux-musl" ]; then
    export CC="$SYSROOT/bin/my-clang-stdlib"
    export CXX="$SYSROOT/bin/my-clang-stdlib"
  fi

  if test "$TARGET"; then
    target_triple="$TARGET"
  else
    target_triple="x86_64-linux-musl"
  fi

  # libunwind
  rm -rf libunwind-build
  mkdir -p libunwind-build
  cd libunwind-build


  cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=$SYSROOT -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_CXX_COMPILER_WORKS=1 -DLIBUNWIND_USE_COMPILER_RT=$COMPILER_RT_ENABLED \
    -DLLVM_VERSION_SUFFIX="libcxx" -DLLVM_ENABLE_LIBCXX=ON\
    ../libunwind-10.0.1.src
  
  make -j$(nproc)
  make install
  cd ..
  echo "[OK] libunwind"

  # libcxxabi
  rm -rf libcxxabi-build
  mkdir -p libcxxabi-build
  cd libcxxabi-build

  sed -i 's|$<TARGET_LINKER_FILE:unwind_static>|'$SYSROOT/lib/libunwind.a'|' ../libcxxabi-10.0.1.src/src/CMakeLists.txt

  cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=$SYSROOT -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR \
    -DLIBCXXABI_LIBCXX_PATH=$WORKDIR/libcxx-10.0.1.src/ -DLLVM_PATH=$ROOT/llvm-novt \
    -DLIBCXXABI_LIBUNWIND_PATH=$WORKDIR/libunwind-10.0.1.src/ -DLIBCXXABI_LIBUNWIND_INCLUDES=$WORKDIR/libunwind-10.0.1.src/includes \
    -DLIBCXXABI_USE_LLVM_UNWINDER=On -DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
    -DLIBCXX_USE_COMPILER_RT=$COMPILER_RT_ENABLED -DLIBCXXABI_USE_COMPILER_RT=$COMPILER_RT_ENABLED -DLIBUNWIND_USE_COMPILER_RT=$COMPILER_RT_ENABLED \
    -DLIBCXXABI_INCLUDE_TESTS=Off \
    -DCMAKE_CXX_COMPILER_WORKS=1 \
    ../libcxxabi-10.0.1.src
  
  make -j$(nproc)
  make install
  cd ..
  echo "[OK] libcxxabi"

  # libcxx
  rm -rf libcxx-build
  mkdir -p libcxx-build
  cd libcxx-build

  export LDFLAGS="$LINK_FLAGS -static-libgcc -lgcc"

  cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=$SYSROOT -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$AR \
    -DLIBCXX_CXX_ABI=libcxxabi -DLIBCXX_CXX_ABI_INCLUDE_PATHS=$WORKDIR/libcxxabi-10.0.1.src/include \
    -DLIBCXX_CXX_ABI_LIBRARY_PATH=$SYSROOT/lib -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=On \
    -DLIBCXXABI_USE_LLVM_UNWINDER=On -DLIBCXXABI_ENABLE_STATIC_UNWINDER=On \
    -DLIBCXX_USE_COMPILER_RT=$COMPILER_RT_ENABLED -DLIBCXXABI_USE_COMPILER_RT=$COMPILER_RT_ENABLED -DLIBUNWIND_USE_COMPILER_RT=$COMPILER_RT_ENABLED \
    -DLIBCXX_HAS_MUSL_LIBC=On \
    -DLIBCXX_INCLUDE_TESTS=Off -DLIBCXX_INCLUDE_BENCHMARKS=Off \
    -DCMAKE_CXX_COMPILER_WORKS=1 \
    ../libcxx-10.0.1.src
  make -j$(nproc)
  make install
  echo "[OK] libcxx"
}


make_compilerrt() {
  mkdir -p "$SYSROOT"
  WORKDIR="$SYSROOT-work"
  mkdir -p "$WORKDIR"
  cd "$WORKDIR"
  tar -xf $ROOT/sysroots/compiler-rt-10.0.1.src.tar.xz

  export CC=$SYSROOT/bin/my-clang
  export CXX=$SYSROOT/bin/my-clang++
  export AR=$CLANG_BIN/llvm-ar
  export RANLIB=$CLANG_BIN/llvm-ranlib
  export CFLAGS="$FLAGS"
  export CXXFLAGS="$FLAGS"
  export LDFLAGS="--sysroot $SYSROOT $LINK_FLAGS"

  if [ -z "$TARGET" ]; then
    target=x86_64-linux-musl
  else
    target="$TARGET"
  fi

  rm -rf compilerrt-build
  mkdir -p compilerrt-build
  cd compilerrt-build

  cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=$SYSROOT -DCMAKE_CXX_COMPILER=$CXX \
      -DLLVM_CONFIG_PATH=$CLANG_BIN/llvm-config \
      -DCOMPILER_RT_BUILD_SANITIZERS=Off -DCOMPILER_RT_BUILD_XRAY=Off -DCOMPILER_RT_BUILD_LIBFUZZER=Off -DCOMPILER_RT_BUILD_PROFILE=Off \
      -DCOMPILER_RT_DEFAULT_TARGET_TRIPLE=$target \
      ../compiler-rt-10.0.1.src
  make -j$(nproc)
  make install
  cd ..

  mv $SYSROOT/lib/linux/clang_rt.crtend-$ARCH.o $SYSROOT/lib/linux/clang_rt.crtend-$ARCH.o.bc
  $LLC_BINARY $SYSROOT/lib/linux/clang_rt.crtend-$ARCH.o.bc --filetype=obj -o $SYSROOT/lib/linux/clang_rt.crtend-$ARCH.o

  echo "[OK] compilerrt"
}

build_obj_file() { # <name>
  $SYSROOT/bin/my-clang -I $ROOT/noic-obj -c $ROOT/noic-obj/$1.c -o $SYSROOT/lib/$1.o --target=$DEB_TARGET -fPIC -fno-stack-protector -fomit-frame-pointer -O3
}

build_obj_files() {
    build_obj_file "patchpoline_jit"
    build_obj_file "patchpoline_plt"
    echo "[OK] noic object files"
}

download_ref_debs() {
  download_and_install_deb "libgcc-10-dev"
  download_and_install_deb "libc6-dev"
  download_and_install_deb "libc-dev-bin"
  download_and_install_deb "linux-libc-dev"
  download_and_install_deb "libc6"
  download_and_install_deb "libc-bin"
  download_and_install_deb "libcrypt1"
  download_and_install_deb "libgcc-s1"
  download_and_install_deb "gcc-10-base"

  download_and_install_deb "libatomic1"
  download_and_install_deb "libc++abi1-11"
  download_and_install_deb "libc++1-11"
  download_and_install_deb "libc++abi-11-dev"
  download_and_install_deb "libc++-11-dev"
  download_and_install_deb "libc++abi-11-dev"

  download_and_install_deb "libstdc++6"
  download_and_install_deb "libstdc++-10-dev"

  pushd .
  cd "$SYSROOT"
  find . -type l -lname '/*' -exec sh -c 'file="$0"; dir=$(dirname "$file"); target=$(readlink "$0"); prefix=$(dirname "$dir" | sed 's@[^/]*@\.\.@g'); newtarget="$prefix$target"; ln -snf $newtarget $file' {} \;
  popd

  echo "[OK] Build reference sysroot (from debian packages)"
}


# === Architecture-specific instructions ===

#### x86_64 / musl / protected
#FLAGS="-fPIC"
#LINK_FLAGS=""
#TG_FLAGS=""
#TARGET=""
#SYSROOT="$ROOT/sysroots/x86_64-linux-musl"
#ARCH=x86_64
#DEB_ARCH=amd64
#DEB_TARGET=x86_64-linux-gnu
#NOIC_ACTIVE=1
#download_dependencies
# build musl + libc++ to bootstrap compilerrt
#make_musl_libc libgcc
#make_clang_scripts libgcc
#make_libcxx libgcc
#make_compilerrt
#build_obj_files
#make_musl_libc compilerrt
#make_libcxx compilerrt
#make_clang_scripts compilerrt


### x86_64 / musl / ref
#FLAGS="-fPIC"
#SYSROOT="$ROOT/sysroots/x86_64-linux-musl-ref"
#TG_FLAGS="TG_ENABLED=0 NOVT_ENABLED=0"
#NOIC_ACTIVE=0
#download_dependencies
#make_musl_libc libgcc
#make_clang_scripts libgcc
#make_libcxx libgcc
#make_compilerrt
#make_musl_libc compilerrt
#make_libcxx compilerrt
#make_clang_scripts compilerrt


#SYSROOT="$ROOT/sysroots/x86_64-linux-gnu-ref"
#download_ref_debs
#make_clang_ref_scripts


### aarch64 / musl / protected
FLAGS="--target=aarch64-linux-musl -fPIC"
LINK_FLAGS="-Wl,-z,notext"
TG_FLAGS=""
TARGET="aarch64-linux-musl"
SYSROOT="$ROOT/sysroots/aarch64-linux-musl"
ARCH=aarch64
DEB_ARCH=arm64
DEB_TARGET=aarch64-linux-gnu
NOIC_ACTIVE=1
download_dependencies
make_musl_libc libgcc
make_clang_scripts libgcc
make_libcxx libgcc
make_compilerrt
build_obj_files
make_musl_libc compilerrt
make_clang_scripts compilerrt
make_libcxx compilerrt


### aarch64 / musl / ref
SYSROOT="$ROOT/sysroots/aarch64-linux-musl-ref"
TG_FLAGS="TG_ENABLED=0 NOVT_ENABLED=0"
NOIC_ACTIVE=0
download_dependencies
make_musl_libc libgcc
make_clang_scripts libgcc
make_libcxx libgcc
make_compilerrt
make_musl_libc compilerrt
make_libcxx compilerrt
make_clang_scripts compilerrt


SYSROOT="$ROOT/sysroots/aarch64-linux-gnu-ref"
download_ref_debs
make_clang_ref_scripts
