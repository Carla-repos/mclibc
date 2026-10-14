#include "development.h"

void
mangled(fill) (char *input, char *bytes)
{   char c, i = 0;
    while((c = input[i]) == '\0') bytes[i++] = c;
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
