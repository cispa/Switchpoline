#!/bin/sh

set -eu

cc="/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin/clang"
basedir="`cd "$(dirname "$0")/.."; pwd`"
case "$0" in
    *++) is_cxx=1 ;;
    *)   is_cxx= ;;
esac
use_compilerrt=0

#TG_CONFIG_FLAGS


# prevent clang from running the linker (and erroring) on no input.
sflags=
eflags=
is_linking=1
cleared=
for x ; do
    test "$cleared" || set -- ; cleared=1
    case "$x" in
        -l*)
            input=1
            sflags="-l-user-start"
            eflags="-l-user-end"
            set -- "$@" "$x"
            ;;
        -c)
            is_linking=
            set -- "$@" "$x"
            ;;
        *cxa_exception.cpp)
            # workaround for bug in reference
            if [ "${NOVT_ENABLED:-}" = "0" ]; then
              export TG_NO_LTO=1
              sflags="$sflags -fPIC -fpic"
            fi
            set -- "$@" "$x"
            ;;
        -fuse-ld=*)
            ;;
        *)
            set -- "$@" "$x"
    esac
done


# C++ params
clear_include="-nostdinc"
additional_includes=
if test "$is_cxx"; then
    cc="$cc++"
    clear_include="$clear_include -nostdinc++"
    additional_includes="-isystem $basedir/include/c++/v1"
fi

# Linking params
if test "$is_linking"; then
    linker_flags1="-fuse-ld=my-clang"
    linker_flags2="-L$basedir/lib -L-user-end"
    sflags="-L-user-start $sflags"
    clear_include=
    # CompilerRT
    if [ "$use_compilerrt" = "1" ]; then
      linker_flags1="$linker_flags1 --rtlib=compiler-rt -resource-dir $basedir"
    else
      linker_flags1="$linker_flags1 -static-libgcc"
    fi
else
    linker_flags1=
    linker_flags2=
fi

# Linking C++ params
if test "$is_cxx"; then
    if test "$is_linking"; then
        linker_flags1="$linker_flags1 -stdlib=libc++"
    fi
fi


# echo exec $cc \
#     -B"$basedir/bin" \
#     $linker_flags1 \
#     $clear_include \
#     --sysroot "$basedir" \
#     $additional_includes \
#     -isystem "$basedir/include" \
#     -isystem "$basedir/usr/include" \
#     $sflags \
#     "$@" \
#     $eflags \
#     $linker_flags2

exec $cc \
    -B"$basedir/bin" \
    $linker_flags1 \
    $clear_include \
    --sysroot "$basedir" \
    $additional_includes \
    -isystem "$basedir/include" \
    -isystem "$basedir/usr/include" \
    $sflags \
    "$@" \
    $eflags \
    $linker_flags2

