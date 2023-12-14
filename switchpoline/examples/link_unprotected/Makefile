FLAGS=-flto --target=aarch64-linux-musl

SYSROOTS="../../sysroots"

CC_PROTECTED="$(SYSROOTS)/aarch64-linux-musl/bin/my-clang"
CC_REFERENCE="$(SYSROOTS)/aarch64-linux-musl-ref/bin/my-clang"

all:
	$(CC_REFERENCE) $(FLAGS) -shared -o unprotected.so unprotected.c
	TG_ENFORCE_JIT=1  $(CC_PROTECTED) $(FLAGS) unprotected.so -o protected protected.c

clean:
	rm -f unprotected.so
	rm -f protected
