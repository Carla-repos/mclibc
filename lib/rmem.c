/* rmem.c -- Raw allocator: one mapping per block, refs in a global
   bookkeeping region.

   Author:  contato.lucasdwbfff@gmail.com

   This file is part of mclibc.

   mclibc is a MINIMAL package of functions that ships bundled with
   Carla, the programming language.  Everything in mclibc is written to
   be tiny, dependency free (no libc, no malloc) and easy to audit.

   The code conventions are the ones of `nashj.c'.  ONE of them is
   bent on purpose here: rule 6 says "no global state", but this lib
   exists to have a global bookkeeping.  The whole global state is the
   single `static' variable `book' below; nothing else is global.

   DESIGN OVERVIEW

   * `bookkeeping_set (N)' maps one region of N bytes.  It is carved
     into refs, one after the other, from the bottom up.
   * `alloc (SIZE)' maps SIZE bytes (the kernel rounds it up to whole
     pages, and gives the pages zero-filled), takes the next free slot
     of the bookkeeping, and fills it with the address and the size.
   * `dump (REF)' unmaps the block and marks the ref as freed.

   The bookkeeping has a FIXED size and its slots are never reused, so
   it holds at most N / sizeof(ref) allocations over its whole life.
   When it is full, `alloc' fails.  */

#include "rmem.h"
#include <stddef.h>

#define LIB rmem
#include "errors.h"

/* ---------------------------------------------------------------------
   Platform detection.  See `nashj.c'.  */

#if defined(_WIN32)
#define RAWMEM_OS_WINDOWS 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#define RAWMEM_OS_LINUX 1
#include "syscall.h"
#else
#error "raw: os not supported"
#endif

/* =====================================================================
   LINUX BACKEND

   Raw system calls, no libc.  Values from the Linux kernel ABI.  */

#if defined(RAWMEM_OS_LINUX)

#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

#if defined(__x86_64__)

/* Perform a raw system call with up to six arguments.  The kernel
   returns -errno (-4095..-1) on failure and touches no `errno'.  */
static inline long
mangled(mclibcsys6) (
    long number,
    long a1, long a2, long a3,
    long a4, long a5, long a6
) {
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    long result;
    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(a1), "S"(a2), "d"(a3),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return result;
}

/* System call 9, `mmap': anonymous, zero-filled, read/write.  */
static inline long
mangled(sys_mmap) (size_t length)
{   return mangled(mclibcsys6)(
        9, 0, (long)length,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    );
}

/* System call 11, `munmap'.  */
static inline long
mangled(sys_munmap) (mangled(addr) ptr, size_t length)
{   return mangled(mclibcsys6)(11, (long)ptr, (long)length, 0, 0, 0, 0);
}

#elif defined(__i386__)

/* Perform a raw system call with up to three arguments.  EBX is saved
   by hand because it is the GOT pointer in PIC code.  */
static inline long
mangled(mclibcsys3) (long number, long a1, long a2, long a3)
{   long result;
    __asm__ volatile (
        "pushl %%ebx\n\t"
        "movl  %2, %%ebx\n\t"
        "int   $0x80\n\t"
        "popl  %%ebx"
        : "=a"(result)
        : "0"(number), "ri"(a1), "c"(a2), "d"(a3)
        : "memory", "cc"
    );
    return result;
}

/* System call 90, `old_mmap': one pointer to a block of six args.  */
static inline long
mangled(sys_mmap) (size_t length)
{   struct {
        unsigned long addr, len, prot, flags, fd, offset;
    } args = {
        0, (unsigned long)length,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        (unsigned long)-1, 0
    };
    return mangled(mclibcsys3)(90, (long)&args, 0, 0);
}

/* System call 91, `munmap'.  */
static inline long
mangled(sys_munmap) (mangled(addr) ptr, size_t length)
{   return mangled(mclibcsys3)(91, (long)ptr, (long)length, 0);
}

#else
#error "raw: Arch Linux not supported"
#endif

/* Map LENGTH bytes of fresh memory.  Returns the address, or NULL on
   failure.  Errors are the top 4095 values of the address space.  */
static inline mangled(addr)
mangled(mmap) (size_t length)
{   long r = mangled(sys_mmap)(length);
    if( ((unsigned long)r) > (unsigned long)-4096L ) return NULL;
    return (mangled(addr)) r;
}

/* Unmap LENGTH bytes at PTR.  Returns 0 on success.  */
static inline int
mangled(munmap) (mangled(addr) ptr, size_t length)
{   return (int) mangled(sys_munmap)(ptr, length);
}

/* =====================================================================
   WINDOWS BACKEND  */

#elif defined(RAWMEM_OS_WINDOWS)

/* Map LENGTH bytes of fresh, zero-filled memory.  Returns the address,
   or NULL on failure.  */
static inline mangled(addr)
mangled(mmap) (size_t length)
{   return VirtualAlloc(NULL, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

/* Release the region that starts at PTR.  LENGTH is unused: with
   MEM_RELEASE, Windows frees the whole region that was reserved.  */
static inline int
mangled(munmap) (mangled(addr) ptr, size_t length)
{   (void)length;
    return VirtualFree(ptr, 0, MEM_RELEASE) ? 0 : -1;
}

#endif

/* =====================================================================
   THE GLOBAL BOOKKEEPING

   It is the ONLY global state of the lib.  A zero-filled one (the
   state before `bookkeeping_set') has no memory, so every `alloc'
   fails.  */

typedef struct mangled(book) {
    mangled(byte) *mem;         /* base of the bookkeeping, or NULL.     */
    size_t capacity;            /* size of the bookkeeping, in bytes.    */
    size_t used;                /* bytes already handed out as refs.     */
} mangled(book);

static mangled(book) mangled(global);

/* =====================================================================
   PUBLIC API  */

/* Map the global bookkeeping with BYTES bytes.  It holds at most
   BYTES / sizeof(ref) allocations, because refs are never recycled.

   Returns 0 on success.  Returns -1, after emitting a runtime error,
   if BYTES cannot hold even one ref, if the bookkeeping was already
   set, or if the system refuses the memory.  Nothing is changed on
   failure.  */
inline int
mangled(bookkeeping_set) (size_t bytes)
{   mangled(book) *b = &mangled(global);
    if( b->mem ) {
        runtime_error(LIB)("The bookkeeping was already set.");
        return -1;
    }
    if( bytes < sizeof(mangled(ref)) ) {
        runtime_error(LIB)("The bookkeeping is too small to hold a single ref.");
        return -1;
    }
    b->mem = (mangled(byte)*) mangled(mmap) (bytes);
    if( !b->mem ) {
        runtime_error(LIB)("Could not obtain memory for the bookkeeping.");
        return -1;
    }
    b->capacity = bytes;
    b->used = 0;
    return 0;
}

/* Allocate SIZE bytes, all of them set to zero, as a mapping of their
   own, and return the ref that describes it.  The plain address is in
   `ref->ptr'.

   Returns NULL, after emitting a runtime error, if SIZE is zero or
   absurdly large, if the bookkeeping was never set or is full, or if
   the system refuses the memory.  Nothing is modified on failure: the
   bookkeeping is checked BEFORE the block is mapped, so a failure
   never leaves a mapping behind.  */
inline mangled(ref)*
mangled(alloc) (size_t size)
{   mangled(book) *b = &mangled(global);
    if( size == 0 ) {
        runtime_error(LIB)("Can't allocate zero bytes.");
        return NULL;
    }
    if( size > ((size_t)-1 >> 1) ) {
        runtime_error(LIB)("The requested size is too large.");
        return NULL;
    }
    if( !b->mem ) {
        runtime_error(LIB)("The bookkeeping was not set. Call bookkeeping_set first.");
        return NULL;
    }
    if( b->capacity - b->used < sizeof(mangled(ref)) ) {
        runtime_error(LIB)("The bookkeeping is full: no room left for another ref.");
        return NULL;
    }

    mangled(addr) p = mangled(mmap) (size);
    if( !p ) {
        runtime_error(LIB)("Could not obtain memory from the operating system.");
        return NULL;
    }

    mangled(ref) *h = (mangled(ref)*) (b->mem + b->used);
    b->used += sizeof(mangled(ref));
    h->freed = 0;
    h->ptr = p;
    h->size = size;
    return h;
}

/* Free the allocation described by the handle H and mark H as freed.

   A NULL handle is ignored, like `free (NULL)'.  A handle that is
   already freed, or that does not point to a slot of the bookkeeping,
   emits a runtime error and changes nothing.  If the system refuses to
   unmap the block, a runtime error is emitted, but H is marked as
   freed anyway, so that it is never used again.  */
inline void
mangled(dump) (mangled(ref) *h)
{   mangled(book) *b = &mangled(global);
    if( !h ) return;

    /* H must be exactly one of the slots already handed out.  */
    mangled(byte) *p = (mangled(byte)*) h;
    if( !b->mem || p < b->mem || p >= b->mem + b->used
        || (size_t)(p - b->mem) % sizeof(mangled(ref)) != 0 ) {
        runtime_error(LIB)("This handle does not belong to the bookkeeping.");
        return;
    }
    if( h->freed ) {
        runtime_error(LIB)("Address already freed. You can't free it twice.");
        return;
    }

    h->freed = 1;
    if( mangled(munmap)(h->ptr, h->size) )
        runtime_error(LIB)("Could not return the memory to the operating system.");
}

/* Return the plain address stored in the handle H.

   Returns NULL, after emitting a runtime error, if H is NULL or if the
   allocation has already been freed, so a stale handle can never be
   used by mistake.  */
inline mangled(addr)
mangled(makeraw) (mangled(ref) *h)
{   if( !h ) {
        runtime_error(LIB)("Null handle. It has no address.");
        return NULL;
    }
    if( h->freed ) {
        runtime_error(LIB)("Address already freed. You can't get its raw address.");
        return NULL;
    }
    return h->ptr;
}

#undef mangled
