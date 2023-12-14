#include <stdio.h>

typedef void (*runnable_t)(void);

static void hello(){
    puts("Hello World");
}


void run(runnable_t what){
    what();
}

runnable_t blubb(){
    return hello;
}

int foobar(int a, int b){
    return a + b;
}
