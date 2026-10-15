#include "development.h"
#include <stdint.h>

void
mangled(fill) (char *bytes, int32_t start, int32_t size)
{   char *input = "Hello, world!\nLorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n";
    int32_t i = 0;
    while( i < size && input[i] != '\0' ) bytes[i] = input[i + start], i++;
}

void
mangled(exit)(int code)
{
    asm volatile(
        "syscall"
        :
        : "a"(60L), "D"((long) code)
        : "rcx", "r11", "memory"
    );
    __builtin_unreachable();
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
