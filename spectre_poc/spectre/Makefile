# Select compiler. If Switchpoline is build, use clang from its reference (as this one is known to produce working binaries)
ifeq ($(wildcard ../../switchpoline/sysroots/aarch64-linux-musl-ref/bin/my-clang),)
    C_COMP=$(CC)
else 
    C_COMP=../../switchpoline/sysroots/aarch64-linux-musl-ref/bin/my-clang
endif 

all:
	$(C_COMP) ${FLAGS} --target=aarch64-linux-musl -I. -I../common -lpthread -pthread -lm ../common/*.c *.c -static -o spectre_poc

clean:
	rm -f spectre_poc

