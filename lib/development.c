#include "development.h"
#include <stdint.h>

void
mangled(fill) (char *bytes, int32_t size)
{   char *input = "This text isn't in the mclibc test file (development.c), so I need to type a bunch of planned things - like \"I love Samar\" - so we can test the byte allocation.\n\n\n";
    int32_t i = 0;
    while( i < size && input[i] != '\0' )
        bytes[i] = input[i], i++;
}

void
mangled(debug) (char *str)
{   unsigned long len = 0;
    while(str[len] != '\0') len++;

    asm volatile(
        "mov $1, %%rax\n"
        "mov $1, %%rdi\n"
        "mov %0, %%rsi\n"
        "mov %1, %%rdx\n"
        "syscall\n"
        :
        : "r"(str), "r"(len)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
}
