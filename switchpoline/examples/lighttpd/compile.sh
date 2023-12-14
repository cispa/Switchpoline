#!/bin/bash

# make sure we start in the correct directory
MY_PATH=$(dirname $(readlink -f "$0"))
cd "$MY_PATH"

# make sure errors in one command lead to termination
set -e
set -o pipefail

# version of lighttpd
lighttpd_v=1.4.59

# version of pcre
pcre_v=8.45

# version of zlib
zlib_v=1.2.11

# path to sysroot
SYSROOT=$(realpath "../../sysroots/aarch64-linux-musl")


# C compiler to use
CC="$SYSROOT/bin/my-clang"

# compiler flags to use
CFLAGS="-O3 -w -fsigned-char --target=aarch64-linux-musl -mcpu=apple-m1 -mtune=apple-m1 --sysroot=$SYSROOT"

# set up environment
export TG_ENABLED=1
export NOIC_ENABLED=1
export NOVT_ENABLED=1
export TG_ENFORCE_JIT=1
export TG_INDIRECT_CALLS_ERROR=1

# make sure we have the queue.h header
if [[ ! -f "$SYSROOT/include/sys/queue.h" ]]
then
  cp "$MY_PATH/queue.h" "$SYSROOT/include/sys/"
  cp "$MY_PATH/_null.h" "$SYSROOT/include/sys/"
fi

function download_and_unpack {
    NAME=$1
    URI=$2
    
    mkdir -p $NAME
    cd $NAME
    if [ ! -d $NAME-$MODE ]; then
        if [ ! -f $NAME.tar.gz ]; then
            wget -O $NAME.tar.gz $URI
        fi
        tar -xf $NAME.tar.gz
        mv $NAME $NAME-$MODE
    fi
    
    cd $NAME-$MODE
}


export TG_NOIC_JIT_ALL=1
export TG_NOVT_JIT_ALL=1

# ======   PCRE   ======

# download and unpack lib-pcre
download_and_unpack pcre-${pcre_v} https://sourceforge.net/projects/pcre/files/pcre/${pcre_v}/pcre-${pcre_v}.tar.bz2

# source files to compile
SOURCES="pcre_byte_order.c pcre_chartables.c pcre_compile.c pcre_config.c pcre_dfa_exec.c pcre_exec.c pcre_fullinfo.c \
pcre_get.c pcre_globals.c pcre_jit_compile.c pcre_maketables.c pcre_newline.c pcre_ord2utf8.c pcre_refcount.c \
pcre_string_utils.c pcre_study.c pcre_tables.c pcre_ucd.c pcre_valid_utf8.c pcre_version.c pcre_xclass.c"

# use default config.h
mv config.h.generic config.h
mv pcre.h.generic pcre.h
mv pcre_chartables.c.dist pcre_chartables.c

# compilation like this makes it easier to tweak compile flags, etc.
$CC $CFLAGS -static -fPIC -c -DHAVE_CONFIG_H -DSUPPORT_PCRE8 -I. $SOURCES

# package everything into a static library
ar cr libpcre.a *.o

# package everything into a dynamic library
$CC $CFLAGS -shared -fPIC -o libpcre.so *.o

# path to pcre
PCRE_PATH=$(pwd)

# go back to initial path
cd $MY_PATH


# ======   ZLIB   ======

# source files to compile
SOURCES="adler32.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c trees.c zutil.c compress.c uncompr.c gzclose.c gzlib.c gzread.c gzwrite.c"

# download and unpack zlib
download_and_unpack zlib-${zlib_v} https://zlib.net/fossils/zlib-${zlib_v}.tar.gz

# fix: include unistd.h
echo "#include <unistd.h>">>zconf.h

# compilation like this makes it easier to tweak compile flags, etc.
$CC $CFLAGS -c -static -fPIC -I. $SOURCES

# package everything into a static library
ar cr libz.a *.o

# package everything into a dynamic library
$CC $CFLAGS -shared -fPIC -o libz.so *.o

# path to zlib
ZLIB_PATH=$(pwd)

# go back to initial path
cd $MY_PATH


# ====== LIGHTTPD ======

# download and unpack lighttpd source
download_and_unpack lighttpd-${lighttpd_v} https://download.lighttpd.net/lighttpd/releases-1.4.x/lighttpd-${lighttpd_v}.tar.gz

# source files to compile
SOURCES="base64.c buffer.c burl.c log.c http_header.c http_kv.c keyvalue.c chunk.c http_chunk.c \
stream.c fdevent.c gw_backend.c stat_cache.c plugin.c http_etag.c array.c data_string.c data_array.c \
data_integer.c algo_md5.c algo_sha1.c algo_splaytree.c fdevent_select.c fdevent_libev.c fdevent_poll.c \
fdevent_linux_sysepoll.c fdevent_solaris_devpoll.c fdevent_solaris_port.c fdevent_freebsd_kqueue.c \
connections-glue.c configfile-glue.c http-header-glue.c http_auth.c http_date.c http_vhostdb.c \
request.c sock_addr.c rand.c safe_memclear.c \
server.c response.c connections.c h2.c reqpool.c sock_addr_cache.c ls-hpack/lshpack.c algo_xxhash.c \
network.c network_write.c data_config.c vector.c configfile.c configparser.c \
mod_rewrite.c mod_redirect.c mod_alias.c mod_extforward.c mod_access.c mod_auth.c mod_authn_file.c \
mod_setenv.c mod_flv_streaming.c mod_indexfile.c mod_userdir.c mod_dirlisting.c mod_status.c \
mod_simple_vhost.c mod_secdownload.c mod_cgi.c mod_fastcgi.c mod_scgi.c mod_ssi.c mod_ssi_expr.c mod_ssi_exprparser.c \
mod_deflate.c mod_proxy.c mod_staticfile.c mod_evasive.c mod_usertrack.c mod_expire.c mod_accesslog.c"

# libraries to link
LIBS="$ZLIB_PATH/libz.a $PCRE_PATH/libpcre.a"

# go into source folder
cd src

# copy header for modules
cp "$MY_PATH/plugin-static.h" .

echo "#include <aio.h>" >>first.h
echo "#include <stdint.h>" >>first.h
echo "#include <sys/time.h>" >>first.h
echo "#include <syslog.h>" >>first.h
echo "#include <sys/wait.h>" >>first.h
echo "#include <sys/types.h>" >>first.h
echo "#include <sys/un.h>" >>first.h
echo "#define LIGHTTPD_VERSION_ID 0x1043b">>first.h

# static
$CC -o lighttpd $CFLAGS -static -flto -DFDEVENT_USE_LINUX_EPOLL -DLIGHTTPD_STATIC -DPACKAGE_NAME=\"lighttpd\" -DLIBRARY_DIR=\".\" -DPACKAGE_VERSION=\"$lighttpd_v\" -DREPO_VERSION=\"\" -DHAVE_SOCKLEN_T -DHAVE_SYS_MMAN_H -I. -I$ZLIB_PATH -I$PCRE_PATH -Icompat -Ils-hpack $SOURCES $LIBS

# dynamic
$CC -o lighttpd_dyn $CFLAGS -DFDEVENT_USE_LINUX_EPOLL -DLIGHTTPD_STATIC -DPACKAGE_NAME=\"lighttpd\" -DLIBRARY_DIR=\".\" -DPACKAGE_VERSION=\"$lighttpd_v\" -DREPO_VERSION=\"\" -DHAVE_SOCKLEN_T -DHAVE_SYS_MMAN_H -I. -I$ZLIB_PATH -I$PCRE_PATH -Icompat -Ils-hpack $SOURCES $ZLIB_PATH/libz.so $PCRE_PATH/libpcre.so

echo "Compilation done"
