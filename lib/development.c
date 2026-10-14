#include "development.h"
#include <stdint.h>

void
mangled(fill) (char *bytes, int32_t size)
{   char *input = "These strings are designed to provide meaningful, readable data for testing memory allocation and byte-level operations, rather than arbitrary sequences like \"kaskdopaskdp\". Even a phrase like \"I love Samar\" can serve as useful test data, as long as the buffer handles every byte correctly.\n\n\n";
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
