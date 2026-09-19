#include "arena.h"
#include <string.h>
#include <stdio.h>

static void arena_debug(mangled(t) *a) {
    printf("using: %zu / %zu\n", a->offset, a->capacity);
    for (mangled(node) *n = a->free_list; n; n = n->next) {
        size_t ini = (mangled(byte)*)n - a->mem;
        printf("  free: %zu until %zu\n", ini, ini + n->size);
    }
}

int main(void) {
    mangled(t) arena = mangled(start)(1024);
    const char msg[] = "I love", second[] = "Samar \n";

    mangled(byte) *ptr  = mangled(alloc)(&arena, 27);
    mangled(byte) *what = mangled(alloc)(&arena, 32);

    memcpy(ptr, msg, sizeof msg);
    memcpy(what, second, sizeof second);

    printf("base    = %p\n", (void*)arena.mem);
    printf("ptr     = %p (offset %td)\n", (void*)ptr,  ptr  - arena.mem);
    printf("what    = %p (offset %td)\n", (void*)what, what - arena.mem);
    printf("content = %s %s", ptr, what);
    printf("offset of arena = %zu\n", arena.offset);

    arena_dump(&arena, ptr, 16);
    arena_debug(&arena);

    return 0;
}
