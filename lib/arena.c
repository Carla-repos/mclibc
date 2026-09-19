#include "arena.h"
#include "syscall.h"
#include <stddef.h>

#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

static inline long
mangled (mclibcsys6) (
    long number,
    long a1,
    long a2,
    long a3,
    long a4,
    long a5,
    long a6
) {
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    long result;
    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "a"(number),
          "D"(a1),
          "S"(a2),
          "d"(a3),
          "r"(r10),
          "r"(r8),
          "r"(r9)
        : "rcx", "r11", "memory"
    );
    return result;
}

static inline mangled(addr)
mangled(mmap) (mangled(addr) ptr, size_t length)
{   long r = mangled(mclibcsys6)(
        9, (long)ptr, (long)length,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    );
    if( ((unsigned long)r) > (unsigned long)-4096L ) return NULL;
    return (mangled(addr)) r;
}

static inline int
mangled(munmap) (mangled(addr) ptr, size_t length)
{   return (int) mangled(mclibcsys6)(
        11,
        (long)ptr,
        (long)length,
        0, 0, 0, 0
    );
}

static inline size_t
mangled(limit) (mangled(t) *a)
{   return a->capacity & ~(size_t)(ARENA_ALIGN - 1);
}

static inline size_t
mangled(room) (mangled(t) *a)
{   return mangled(limit)(a) - a->offset - a->meta;
}

static inline mangled(node)*
mangled(node_get) (mangled(t) *a)
{   mangled(node) *n = a->spare;
    if( n ) { a->spare = n->next; return n; }
    if( mangled(room)(a) < sizeof(mangled(node)) ) return NULL;
    a->meta += sizeof(mangled(node));
    return (mangled(node)*)(a->mem + mangled(limit)(a) - a->meta);
}

static inline void
mangled(node_put) (mangled(t) *a, mangled(node) *n)
{   n->next = a->spare;
    a->spare = n;
}

inline mangled(t)
mangled(start) (size_t bytes)
{   mangled(t) arena = {0};
    arena.mem = mangled(mmap) (NULL, bytes);
    arena.capacity = arena.mem ? bytes : 0;
    return arena;
}

inline mangled(addr)
mangled(alloc) (mangled(t) *a, size_t size)
{   if( size == 0 || size > (size_t)-1 - ARENA_ALIGN ) return NULL;
    size_t total = ARENA_NORM(size);

    for( mangled(node) **link = &a->free_list; *link; link = &(*link)->next ) {
        mangled(node) *n = *link;
        if( n->size < total ) continue;

        if( n->size == total ) {
            *link = n->next;
            n->next = a->used_list;
            a->used_list = n;
            return n->addr;
        }

        mangled(node) *r = mangled(node_get)(a);
        if( !r ) return NULL;
        r->addr = n->addr;
        r->size = total;
        r->next = a->used_list;
        a->used_list = r;

        n->addr += total;
        n->size -= total;
        return r->addr;
    }

    mangled(node) *r = mangled(node_get)(a);
    if( !r ) return NULL;
    if( total > mangled(room)(a) ) { mangled(node_put)(a, r); return NULL; }

    r->addr = a->mem + a->offset;
    r->size = total;
    r->next = a->used_list;
    a->used_list = r;
    a->offset += total;
    return r->addr;
}

inline void
mangled(dump) (mangled(t) *a, mangled(addr) p)
{   if( !p ) return;

    mangled(node) *r = NULL;
    for( mangled(node) **l = &a->used_list; *l; l = &(*l)->next )
    /* -> */ if( (*l)->addr == (mangled(byte)*)p ) {
        r = *l;
        *l = r->next;
        break;
    }

    if( !r ) return;

    mangled(node) *prev = NULL, *next = a->free_list;
    while( next && next->addr < r->addr ) { prev = next; next = next->next; }
    r->next = next;

    if( next && r->addr + r->size == next->addr ) {
        r->size += next->size;
        r->next  = next->next;
        mangled(node_put)(a, next);
    }

    if( prev && prev->addr + prev->size == r->addr ) {
        prev->size += r->size;
        prev->next  = r->next;
        mangled(node_put)(a, r);
        r = prev;
    } else if( prev ) prev->next = r;
    else a->free_list = r;

    if( r->addr + r->size == a->mem + a->offset ) {
        a->offset = r->addr - a->mem;
        mangled(node) **l = &a->free_list;
        while(*l != r) l = &(*l)->next;
        *l = NULL;
        mangled(node_put)(a, r);
    }
}

inline void
mangled(reset) (mangled(t) *a)
{   a->offset = 0;
    a->meta = 0;
    a->free_list = a->used_list = a->spare = NULL;
}

inline void
mangled(destroy) (mangled(t) *a)
{   if( a->mem ) mangled(munmap)(a->mem, a->capacity);
    a->mem = NULL;
    a->capacity = a->offset = a->meta = 0;
    a->free_list = a->used_list = a->spare = NULL;
}
