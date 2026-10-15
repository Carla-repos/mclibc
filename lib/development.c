#include "development.h"
#include <stdint.h>

void
mangled(fill) (char *bytes, int32_t size)
{   char *input = "Hello, world!\nLorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since 1966, when designers at Letraset and James Mosley, the librarian at St Bride Printing Library in London, took a 1914 Cicero translation and scrambled it to make dummy text for Letraset's Body Type sheets. It has survived not only many decades, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised thanks to these sheets and more recently with desktop publishing software like Aldus PageMaker and Microsoft Word including versions of Lorem Ipsum.\n";
    int32_t i = 0;
    while( i < size && input[i] != '\0' )
        bytes[i] = input[i], i++;
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
