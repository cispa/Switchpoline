#include <stdlib.h>
#include <stdio.h>

typedef void (*runnable_t)(void);

/* defined in unprotected library */
void run(runnable_t what);
runnable_t blubb(void);
int foobar(int a, int b);

void runme(void){
    puts("I was run");
}

int main(int argc, char** argv){
    // this is ok, and hello is callable (unprotected --function-pointer--> protected)
    runnable_t hello = blubb();
    
    // this is also ok. (protected ---call--> unprotected)
    printf("foobar(1, 2) = %d\n", foobar(1, 2)); 
    
    // this also works since hello came from unprotected code and remains a function pointer
    run(hello);
    
    // this will crash (protected --function-pointer--> protected) since we pass a function ID that will be treated like a function pointer
    run(runme);
}
