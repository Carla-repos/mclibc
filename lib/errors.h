#ifndef LIB
#error "LIB define not found"
#endif

#define MAKE_ERROR_PRINTER_BY_LIB_X86_64(library)                        \
inline static void                                                       \
lib_##library##_error_print(char *str)                                   \
{                                                                        \
    unsigned long len = 0;                                               \
    while(str[len] != '\0') len++;                                       \
    asm volatile(                                                        \
        "mov $1, %%rax\n"                                                \
        "mov $1, %%rdi\n"                                                \
        "mov %0, %%rsi\n"                                                \
        "mov %1, %%rdx\n"                                                \
        "syscall\n"                                                      \
        :                                                                \
        : "r"(str), "r"(len)                                             \
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"             \
    );                                                                   \
};

#define MAKE_ERROR_BY_LIB(library)                                       \
inline static void                                                       \
lib_##library##_e_error(char *msg)                                       \
{   lib_##library##_error_print("[");                                    \
    lib_##library##_error_print(#library);                               \
    lib_##library##_error_print("]: An runtime error was happened\n~ "); \
    lib_##library##_error_print(msg);                                    \
    lib_##library##_error_print("\n");                                   \
}

MAKE_ERROR_PRINTER_BY_LIB_X86_64(LIB)
MAKE_ERROR_BY_LIB(LIB)

#define runtime_error(library) lib_##library##_e_error
