#include "arena.h"
#include "development.h"

#define mangled_a(id) arena_##id
#define mangled_d(id) development_##id

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
    mangled_a(t) arena = mangled_a(start)(1024);
    const char msg[] = "I love", second[] = "Samar";

    mangled_a(ref) *mem = mangled_a(alloc)(&arena, 32);
    sprintf(mem->ptr, "%s %s\n", msg, second);
    mangled_d(debug)(mem);

    return 0;
}
