#include "arena.h"
#include <string.h>
#include <stdio.h>

int main() {
    mangled(t) arena = mangled(start)(1024);
    const char msg[] = "Hello, world\n";
    mangled(addr) ptr = mangled(alloc)(&arena, 30);
    memcpy(ptr, msg, sizeof(msg));
    printf("%s", ptr);
    return 0;
}
