FLAGS=-static -flto -target aarch64-linux-musl

SYSROOTS="../../sysroots"

CC_PROTECTED="$(SYSROOTS)/aarch64-linux-musl/bin/my-clang"
CXX_PROTECTED="$(CC_PROTECTED)++"

CC_REFERENCE="$(SYSROOTS)/aarch64-linux-musl-ref/bin/my-clang"
CXX_REFERENCE="$(CC_REFERENCE)++"

all:
	$(CC_PROTECTED) $(FLAGS) example.c -o example_c_protected
	$(CXX_PROTECTED) $(FLAGS) example.cpp -o example_cpp_protected
	$(CC_REFERENCE) $(FLAGS) example.c -o example_c_reference
	$(CXX_REFERENCE) $(FLAGS) example.cpp -o example_cpp_reference

clean:
	rm -f example_c_protected
	rm -f example_cpp_protected
	rm -f example_c_reference
	rm -f example_cpp_reference
