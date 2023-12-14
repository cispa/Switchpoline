#!/bin/sh
cc="/home/markus/Projekte/arm-spectre-llvm/llvm-novt/cmake-build-minsizerel/bin/clang"
basedir="`cd "$(dirname "$0")/.."; pwd`"
#ldso="/home/markus/Projekte/arm-spectre-llvm/sysroots/x86_64-linux-musl/lib/ld-musl-x86_64.so.1"
ldso="$basedir"/lib/ld-musl-*.so.1
cleared=
shared=
userlinkdir=
userlink=
target=

#TG_CONFIG_FLAGS

for x ; do
    test "$cleared" || set -- ; cleared=1

    case "$x" in
        -L-user-start)
            userlinkdir=1
            ;;
        -L-user-end)
            userlinkdir=
            ;;
        -L*)
            test "$userlinkdir" && set -- "$@" "$x"
            ;;
        -l-user-start)
            userlink=1
            ;;
        -l-user-end)
            userlink=
            ;;
        crtbegin*.o|crtend*.o)
            set -- "$@" $($cc --sysroot $basedir $target -print-file-name=$x)
            ;;
        -lgcc|-lgcc_eh)
            file=lib${x#-l}.a
            set -- "$@" $($cc --sysroot $basedir $target -print-file-name=$file)
            ;;
        -lc++)    set -- "$@" "$x" ;;
        -lc++abi) set -- "$@" "$x" ;;
        -lunwind) set -- "$@" "$x" ;;
        -l*)
            test "$userlink" && set -- "$@" "$x"
            ;;
        -shared)
            shared=1
            set -- "$@" -shared
            ;;
        -sysroot=*|--sysroot=*)
            ;;
        aarch64linux)
            target="--target=aarch64-linux-musl"
            set -- "$@" "$x"
            ;;
        /*libclang_rt.builtins-*.a)
            set -- "$@" "$basedir/lib/linux/$(basename $x)"
            ;;
        *)
            set -- "$@" "$x"
            ;;
    esac
done

echo EXEC $($cc -print-prog-name=lld) "-rpath" "$basedir/lib" "-nostdlib" "$@" -lc -dynamic-linker $ldso
exec $($cc -print-prog-name=lld) -rpath "$basedir/lib" -nostdlib "$@" -lc -dynamic-linker $ldso
