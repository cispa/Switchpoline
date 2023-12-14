#include <stdio.h>
#include <stdlib.h>

void do_print(){
    puts("Hello World!");
}

void do_exit(){
    exit(0);
}

typedef void (*func_t)();

func_t get_func(func_t last_func){
    if(last_func == &do_print){
        return &do_exit;
    }
    return &do_print;
}

int main(int argc, char** argv){
    func_t func = get_func(NULL);
    func();
    func = get_func(func);
    func();
    exit(1337);
}
