#!/bin/sh

set -eu

cc="/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin/clang"
basedir="`cd "$(dirname "$0")/.."; pwd`"
case "$0" in
    *++) is_cxx=1 ;;
    *)   is_cxx= ;;
esac

arch=x86_64-linux-gnu

export TG_ENABLED=0
export NOVT_ENABLED=0


# prevent clang from running the linker (and erroring) on no input.
is_linking=1
cleared=
for x ; do
    test "$cleared" || set -- ; cleared=1
    case "$x" in
        -l*)
            input=1
            set -- "$@" "$x"
            ;;
        -c)
            is_linking=
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
    #additional_includes="-isystem $basedir/include/c++/v1"
    additional_includes="-isystem $basedir/usr/include/c++/10 -isystem $basedir/usr/include/$arch/c++/10"
fi

# Linking params
if test "$is_linking"; then
    linker_flags1="-fuse-ld=lld"
    linker_flags2="-L$basedir/lib/$arch -L$basedir/lib -L$basedir/usr/lib -B$basedir/lib -B$basedir/usr/lib"
    clear_include=
else
    linker_flags1=
    linker_flags2=
fi

# Linking C++ params
#if test "$is_cxx"; then
#    if test "$is_linking"; then
#        linker_flags1="$linker_flags1 -stdlib=libc++"
#    fi
#fi


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
    --sysroot "$basedir" \
    $additional_includes \
    -isystem "$basedir/usr/include/$arch" \
    -isystem "$basedir/include" \
    -isystem "$basedir/usr/include" \
    "$@" \
    $linker_flags2

