#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>

class Base {
    public:
        virtual void func() {asm volatile("nop");}
};

typedef Base* func_t;

func_t targets[256];

// all configuration will be in there
#include "indirect_call.hpp"

static void shuffle(func_t targets[], int size){
    srand(time(0));
    for (int i = 0; i < size; i++) {
        int j = rand() % size;
        func_t t = targets[i];
        targets[i] = targets[j];
        targets[j] = t;
    }
}

static inline uint64_t timestamp(){
  uint64_t value;
  __asm__ volatile(
          "MRS %[target], CNTVCT_EL0" /* read CNTVCT_EL0 model specific register */
          : [target] "=r" (value) /* write result to value */
  : /* no register read */
  : /* no additional register changed */
  );
  return value;
}

void do_bench(){
    for(int i = 0; i < 1000; i++){
        for(int j = 0; j < 256; j++){
            CALL(targets[j]);
        }
    }
}

int main(int argc, char** argv){
    assign_targets();
    
    shuffle(targets, 256);
    
    uint64_t results[200];
    
    for(int i = 0; i < 200; i++){   
        uint64_t start = timestamp();
        do_bench();
        uint64_t end = timestamp();
        results[i] = end - start;
    }
    
    for(int i = 0; i < 200; i++){
        printf("%zu\n", results[i]);
    }
}
