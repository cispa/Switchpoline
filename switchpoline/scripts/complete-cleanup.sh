ROOT=$(readlink -f "$(dirname $0)/..")

echo "cleaning up"
# sysroots
rm -rf "$ROOT/sysroots/*"
# llvm build
rm -rf "$ROOT/llvm-novt/cmake-build-minsizerel"
# lighttpd build
rm -rf "$ROOT/benchmark/scripts/lighttpd/pcre-*"
rm -rf "$ROOT/benchmark/scripts/lighttpd/zlib-*"
rm -rf "$ROOT/benchmark/scripts/lighttpd/lighttpd-*"
