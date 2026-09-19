#include "arena.h"
#include <string.h>
#include <stdio.h>

static void arena_debug(arena_t *a) {
    printf("dados ate %zu | nodes: %zu bytes no fim | capacidade %zu\n",
           a->offset, a->meta, a->capacity);
    for (arena_node *n = a->used_list; n; n = n->next)
        printf("  usado: %td until %td\n", n->addr - a->mem, n->addr - a->mem + (ptrdiff_t)n->size);
    for (arena_node *n = a->free_list; n; n = n->next)
        printf("  livre: %td until %td\n", n->addr - a->mem, n->addr - a->mem + (ptrdiff_t)n->size);
}

int main(void) {
    mangled(t) arena = mangled(start)(1024);
    const char msg[] = "I love", second[] = "Samar";

    mangled(byte) *ptr  = mangled(alloc)(&arena, 32);
    mangled(byte) *what = mangled(alloc)(&arena, 16);

    memcpy(ptr, msg, sizeof msg);
    memcpy(what, second, sizeof second);

    printf("base    = %p\n", (void*)arena.mem);
    printf("ptr     = %p (offset %td)\n", (void*)ptr,  ptr  - arena.mem);
    printf("what    = %p (offset %td)\n", (void*)what, what - arena.mem);
    printf("content = %s %s\n", ptr, what);
    printf("offset of arena = %zu\n", arena.offset);

    arena_dump(&arena, ptr);
    mangled(byte) *check = mangled(alloc)(&arena, 12);
    memcpy(check, second, sizeof second);
    printf("\nbase    = %p\n", (void*)arena.mem);
    printf("ptr     = %p (offset %td)\n", (void*)ptr,  ptr  - arena.mem);
    printf("check   = %p (offset %td)\n", (void*)check,  check  - arena.mem);
    printf("what    = %p (offset %td)\n", (void*)what, what - arena.mem);
    printf("content = %s %s\n", check, what);

    arena_dump(&arena, check);
    arena_dump(&arena, what);
    arena_destroy(&arena);

    return 0;
}
