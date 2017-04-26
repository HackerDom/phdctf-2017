#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <csetjmp>

jmp_buf ebuf;

void SignalHandler(int signal) {
    if (signal == SIGSEGV) {
        printf("Oops! SIGSEGV\n");
        longjmp(ebuf, 1);
    }
}

int main() {
    signal(SIGSEGV , SignalHandler);
    if (setjmp(ebuf) == 0) {
        printf("Going to die ... \n");
        *(int *) 0 = 0;
    }
    printf("But please kids, DONT TRY THIS AT HOME ;)\n");
    return 0;
}

