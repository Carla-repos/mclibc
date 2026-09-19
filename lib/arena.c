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

inline mangled(t)
mangled(start) (size_t bytes)
{   mangled(t) arena = {0};
    arena.mem = mangled(mmap) (NULL, bytes);
    arena.capacity = arena.mem ? bytes : 0;
    return arena;
}

inline mangled(addr)
mangled(alloc) (mangled(t) *arena, size_t size)
{   size = ARENA_NORM(size);
    mangled(node) **link = &arena->free_list;
    while(*link) {
        mangled(node) *n = *link;
        if( n->size >= size ) {
            size_t rest = n->size - size;
            if( rest <= 0 ) *link = n->next;
            else {
                mangled(node) *r = (mangled(node)*)((mangled(byte)*)n + size);
                r->size = rest;
                r->next = n->next;
                *link = r;
            }

            return (mangled(addr))n;
        }
        link = &n->next;
    }

    if( arena->offset + size > arena->capacity ) return NULL;
    mangled(byte) *ptr = arena->mem + arena->offset;
    arena->offset += size;
    return ptr;
}

inline void
mangled(dump) (mangled(t) *arena, mangled(addr) p, size_t size)
{   if( !p ) return;
    mangled(byte) *ptr = p;
    size = ARENA_NORM(size);

    mangled(node) *prev = NULL, *next = arena->free_list;
    while(next && (mangled(byte)*)next < ptr) {
        prev = next;
        next = next->next;
    }

    mangled(node) *n = (mangled(node)*)ptr;
    n->size = size;
    n->next = next;

    if( next && ptr + size == (mangled(byte)*)next ) {
        n->size += next->size;
        n->next  = next->next;
    }

    if( prev && ((mangled(byte)*)prev) + prev->size == ptr ) {
        prev->size += n->size;
        prev->next  = n->next;
        n = prev;
    } else if( prev ) prev->next = n;
    else arena->free_list = n;

    if( ((mangled(byte)*)n) + n->size == arena->mem + arena->offset ) {
        arena->offset = (mangled(byte)*)n - arena->mem;
        mangled(node) **l = &arena->free_list;
        while( *l != n ) l = &(*l)->next;
        *l = NULL;
    }
}

inline void
mangled(reset) (mangled(t) *arena)
{   arena->offset = 0;
    arena->free_list = NULL;
}


inline void
mangled(destroy) (mangled(t) *arena)
{   if( arena->mem ) mangled(munmap)(arena->mem, arena->capacity);
    arena->mem = NULL;
    arena->free_list = NULL;
    arena->capacity = 0;
    arena->offset = 0;
}
