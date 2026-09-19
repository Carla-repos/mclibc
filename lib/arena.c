/* arena.c -- Arena allocator with a free list and no per-block headers.

   Author:  contato.lucasdwbfff@gmail.com

   This file is part of mclibc.

   mclibc is a MINIMAL package of functions that ships bundled with
   Carla, the programming language.  Everything in mclibc is written to
   be tiny, dependency free (no libc, no malloc) and easy to audit.

   ---------------------------------------------------------------------
   CODE CONVENTIONS (READ THIS BEFORE WRITING ANY "stdlibcore" LIB)
   ---------------------------------------------------------------------

   The code in this file is the REFERENCE EXAMPLE for every "lib" that
   belongs to Carla's "stdlibcore".  Its style is intentionally
   uniform, so that all libs look and behave the same way.  The rules
   that can be observed here are:

   1. Namespacing.  Every public or internal identifier is spelled
      through the `mangled (id)' macro, defined in the lib's own header
      (here, `arena.h').  For this lib, `mangled (alloc)' expands to
      `arena_alloc'.  Changing the prefix means changing ONE macro.

   2. Public API through an X-macro.  The header lists every public
      function once, in a `FUNCTIONS' table, and expands it to emit the
      `extern inline' declarations.  This file provides the matching
      `inline' definitions.

   3. Function layout.  The return type goes on its own line, the
      function name starts at column 0 (so `grep -n "^name"' finds the
      definition), and the body's opening brace is followed by three
      spaces and the first statement:

          inline void
          mangled (name) (arguments)
          {   first_statement;
              ...
          }

   4. Internal helpers are `static inline'.  Only the functions listed
      in the header's `FUNCTIONS' table are visible to the outside.

   5. Platform code is isolated in a single block at the top of the
      file, behind `#if' guards, and exports the same tiny interface on
      every platform.  The rest of the file never mentions an OS.

   6. No hidden allocations, no global state.  Every function works only
      on the state that the caller hands to it.

   7. Comments follow the GNU style: full sentences, two spaces after a
      period, and the closing marker of a multi-line comment on the
      last line of text.

   ---------------------------------------------------------------------
   DESIGN OVERVIEW
   ---------------------------------------------------------------------

   The arena is ONE contiguous region obtained from the operating
   system.  Inside that region, the memory is split in two areas that
   grow towards each other:

       low addresses                                    high addresses
       +--------------------+-----------------+----------------------+
       | user data  ->      |   (free room)   |      <-  node table  |
       +--------------------+-----------------+----------------------+
       ^ mem                ^ mem+offset        ^ mem+limit-meta       ^ mem+limit

   * The data area grows upwards, from `mem'.  `offset' is its top.
   * The node table grows downwards, from the end of the region.
     `meta' is its size in bytes.

   Every allocation is described by a NODE, which stores an address and
   a size ("9 until 103", for instance).  Nodes live inside the arena
   itself, so no external allocator is needed.  Nodes are kept in three
   singly linked lists:

   * `used_list'  - live allocations.  Freeing looks up the pointer
                    here; the address found IS the identity, and the
                    size comes from the same node.  This is why no
                    header is stored before the user's data.
   * `free_list'  - ranges that were freed, sorted by address, and
                    merged with their neighbours whenever they touch.
   * `spare'      - dead nodes, kept for reuse so that the node table
                    is bounded by the PEAK number of simultaneous
                    allocations rather than by the total history.

   Freed ranges have no "chunk" concept: a range is just an address
   and a length, and may be split or merged freely.  */

#include "arena.h"
#include <stddef.h>

/* ---------------------------------------------------------------------
   Platform detection.

   Exactly one of ARENA_OS_WINDOWS or ARENA_OS_LINUX is defined here.
   `_WIN32' is set by every Windows compiler (MSVC, MinGW, clang-cl) for
   both 32 and 64 bit targets, unlike `WIN32', which usually comes from
   the project files.  Any other system is rejected at compile time
   instead of failing mysteriously at run time.  */

#if defined(_WIN32)
#define ARENA_OS_WINDOWS 1
/* Trim <windows.h> down to the parts that we really need.  */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#define ARENA_OS_LINUX 1
#include "syscall.h"
#else
#error "arena: os not supported"
#endif

/* =====================================================================
   LINUX BACKEND

   We talk to the kernel directly, bypassing libc entirely.  Each
   supported architecture provides two raw wrappers, `sys_mmap' and
   `sys_munmap', and the common code further below turns them into the
   portable `mmap' and `munmap' used by the rest of the file.  */

#if defined(ARENA_OS_LINUX)

/* Values from the Linux kernel ABI (<sys/mman.h>).  They are repeated
   here so that this file does not depend on any libc header.

   PROT_READ / PROT_WRITE : the pages may be read and written.
   MAP_PRIVATE            : changes are not shared with other processes.
   MAP_ANONYMOUS          : the pages are not backed by a file; the
                            kernel hands them over zero-filled.  */
#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

/* ---------------------------------------------------------------------
   x86_64: the `syscall' instruction.

   Calling convention of the Linux x86_64 kernel:

     rax = system call number         (also the return value)
     rdi, rsi, rdx, r10, r8, r9 = arguments 1 to 6

   Note that the 4th argument goes in r10, NOT in rcx, because the
   `syscall' instruction itself clobbers rcx and r11.  */

#if defined(__x86_64__)

/* Perform a raw system call with up to six arguments.

   NUMBER is the system call number; A1..A6 are its arguments.  The
   return value is whatever the kernel left in rax.  On failure, the
   kernel returns a small negative number (-errno) in the range
   -1..-4095; it does NOT touch any `errno' variable, since there is
   no libc here.  */
static inline long
mangled (mclibcsys6) (
    long number,
    long a1, long a2, long a3,
    long a4, long a5, long a6
) {
    /* Arguments 4 to 6 have no single-letter GCC constraint, so they
       are pinned to their registers with explicit register variables.  */
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    long result;
    __asm__ volatile (
        "syscall"
        : "=a"(result)          /* output: rax                          */
        : "a"(number),          /* input:  rax = system call number     */
          "D"(a1),              /*         rdi = argument 1             */
          "S"(a2),              /*         rsi = argument 2             */
          "d"(a3),              /*         rdx = argument 3             */
          "r"(r10),             /*         r10 = argument 4             */
          "r"(r8),              /*         r8  = argument 5             */
          "r"(r9)               /*         r9  = argument 6             */
        : "rcx", "r11", "memory"    /* clobbered by the instruction     */
    );
    return result;
}

/* Ask the kernel for LENGTH bytes of anonymous, zero-filled,
   read/write memory (system call 9, `mmap').  The kernel chooses the
   address.  Returns the address, or -errno on failure.  */
static inline long
mangled(sys_mmap) (size_t length)
{   return mangled(mclibcsys6)(
        9, 0, (long)length,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0                   /* no file descriptor, no file offset   */
    );
}

/* Give the LENGTH bytes starting at PTR back to the kernel
   (system call 11, `munmap').  Returns 0 on success, -errno on
   failure.  */
static inline long
mangled(sys_munmap) (mangled(addr) ptr, size_t length)
{   return mangled(mclibcsys6)(11, (long)ptr, (long)length, 0, 0, 0, 0);
}

/* ---------------------------------------------------------------------
   x86 (32 bit): the `int $0x80' software interrupt.

   Calling convention of the Linux i386 kernel:

     eax = system call number         (also the return value)
     ebx, ecx, edx, esi, edi, ebp = arguments 1 to 6

   We only need three registers, which keeps the asm simple.  */

#elif defined(__i386__)

/* Perform a raw system call with up to three arguments.

   In position independent code (-fPIC) ebx is reserved as the GOT
   pointer, so GCC refuses the "b" constraint.  To stay portable, ebx
   is saved with `pushl', loaded by hand, and restored with `popl'
   around the interrupt.  */
static inline long
mangled (mclibcsys3) (long number, long a1, long a2, long a3)
{   long result;
    __asm__ volatile (
        "pushl %%ebx\n\t"       /* save the PIC register                */
        "movl  %2, %%ebx\n\t"   /* ebx = argument 1                     */
        "int   $0x80\n\t"       /* enter the kernel                     */
        "popl  %%ebx"           /* restore the PIC register             */
        : "=a"(result)
        : "0"(number),          /* eax = system call number (same reg   */
                                /* as the output)                       */
          "ri"(a1),             /* argument 1: any register or an imm.  */
          "c"(a2),              /* ecx = argument 2                     */
          "d"(a3)               /* edx = argument 3                     */
        : "memory", "cc"
    );
    return result;
}

/* Ask the kernel for LENGTH bytes of anonymous, zero-filled,
   read/write memory.

   We use system call 90 (`old_mmap') on purpose: it takes ONE pointer
   to a block of six arguments, so it needs a single register.  The
   newer `mmap2' (call 192) needs six registers, including ebp, which
   collides with frame pointers.  */
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

/* Give the LENGTH bytes starting at PTR back to the kernel
   (system call 91, `munmap').  */
static inline long
mangled(sys_munmap) (mangled(addr) ptr, size_t length)
{   return mangled(mclibcsys3)(91, (long)ptr, (long)length, 0);
}

#else
#error "arena: Arch Linux not supported"
#endif

/* ---------------------------------------------------------------------
   Portable wrappers shared by every Linux architecture.  */

/* Map LENGTH bytes of fresh memory.  PTR is a placement hint that is
   currently ignored (the kernel always picks the address).

   Returns the address, or NULL on failure.  The kernel reports errors
   as values in the range -4095..-1, which, read as an unsigned number,
   is the highest 4095 values of the address space; no valid mapping
   can live there.  */
static inline mangled(addr)
mangled(mmap) (mangled(addr) ptr, size_t length)
{   (void)ptr;
    long r = mangled(sys_mmap)(length);
    if( ((unsigned long)r) > (unsigned long)-4096L ) return NULL;
    return (mangled(addr)) r;
}

/* Unmap LENGTH bytes at PTR.  Returns 0 on success, non-zero on
   failure.  */
static inline int
mangled(munmap) (mangled(addr) ptr, size_t length)
{   return (int) mangled(sys_munmap)(ptr, length);
}

/* =====================================================================
   WINDOWS BACKEND

   Windows has no raw system call ABI that is stable across versions,
   so the documented Win32 virtual memory API is used instead.  */

#elif defined(ARENA_OS_WINDOWS)

/* Map LENGTH bytes of fresh memory.  MEM_RESERVE | MEM_COMMIT gets the
   address range and the backing storage in a single call, and the
   pages come zero-filled, just like an anonymous mmap on Linux.

   Returns the address, or NULL on failure.  */
static inline mangled(addr)
mangled(mmap) (mangled(addr) ptr, size_t length)
{   return VirtualAlloc(ptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

/* Release the region that starts at PTR.  Returns 0 on success and -1
   on failure, mirroring the Linux convention.

   LENGTH is unused: with MEM_RELEASE, Windows requires the size to be
   zero and always frees the whole region that was reserved.  */
static inline int
mangled(munmap) (mangled(addr) ptr, size_t length)
{   (void)length;   /* MEM_RELEASE exige tamanho 0 */
    return VirtualFree(ptr, 0, MEM_RELEASE) ? 0 : -1;
}

#endif

/* =====================================================================
   INTERNAL HELPERS

   From this point on, the code is completely platform independent.  */

/* Return the usable size of the arena, rounded DOWN to a multiple of
   ARENA_ALIGN.  The node table is anchored at this boundary, which
   keeps every node naturally aligned.  */
static inline size_t
mangled(limit) (mangled(t) *a)
{   return a->capacity & ~(size_t)(ARENA_ALIGN - 1);
}

/* Return how many bytes are still untouched between the top of the
   data area and the bottom of the node table.  This is the room
   available for BOTH new data and new nodes.  */
static inline size_t
mangled(room) (mangled(t) *a)
{   return mangled(limit)(a) - a->offset - a->meta;
}

/* Obtain a node to describe an allocation or a free range.

   A recycled node from the `spare' list is preferred.  Only when none
   is available does the node table grow downwards by one node.

   Returns NULL if there is not enough room left for another node.  */
static inline mangled(node)*
mangled(node_get) (mangled(t) *a)
{   mangled(node) *n = a->spare;
    if( n ) { a->spare = n->next; return n; }
    if( mangled(room)(a) < sizeof(mangled(node)) ) return NULL;
    a->meta += sizeof(mangled(node));
    return (mangled(node)*)(a->mem + mangled(limit)(a) - a->meta);
}

/* Give a node that is no longer needed back to the `spare' list, so
   that `node_get' can reuse it.  The node table itself never shrinks
   (except through `reset'), which is why recycling matters.  */
static inline void
mangled(node_put) (mangled(t) *a, mangled(node) *n)
{   n->next = a->spare;
    a->spare = n;
}

/* =====================================================================
   PUBLIC API

   Every function below is declared in `arena.h' through the
   `FUNCTIONS' X-macro.  */

/* Create an arena backed by BYTES bytes obtained from the operating
   system.  The arena is returned BY VALUE; the caller owns it and must
   eventually pass it to `destroy'.

   If the system refuses the request, the returned arena has
   `capacity' == 0 and every later `alloc' on it returns NULL, so the
   failure needs no special handling by the caller.  */
inline mangled(t)
mangled(start) (size_t bytes)
{   mangled(t) arena = {0};     /* all lists start empty (NULL)         */
    arena.mem = mangled(mmap) (NULL, bytes);
    arena.capacity = arena.mem ? bytes : 0;
    return arena;
}

/* Allocate SIZE bytes from the arena A.

   The size is rounded up to a multiple of ARENA_ALIGN, so every
   returned pointer is ARENA_ALIGN-byte aligned.  The strategy is:

     1. First fit over the free list.  A range that is a perfect fit is
        promoted to a live allocation as is.  A larger range is split:
        the front becomes the allocation and the free range shrinks.
     2. Otherwise, the top of the data area is advanced.

   Returns NULL if SIZE is zero, if it is absurdly large, or if the
   arena has no room left for the data plus its bookkeeping node.  */
inline mangled(addr)
mangled(alloc) (mangled(t) *a, size_t size)
{   /* Reject zero, and sizes that would overflow when rounded up.  */
    if( size == 0 || size > (size_t)-1 - ARENA_ALIGN ) return NULL;
    size_t total = ARENA_NORM(size);

    /* Step 1: look for a suitable free range (first fit).  `link'
       points at the pointer that references the current node, which
       lets us unlink a node without keeping a `prev' pointer.  */
    for( mangled(node) **link = &a->free_list; *link; link = &(*link)->next ) {
        mangled(node) *n = *link;
        if( n->size < total ) continue;     /* too small, keep looking  */

        /* Perfect fit: the free node itself becomes the allocation
           node, so no new node is needed.  */
        if( n->size == total ) {
            *link = n->next;                /* unlink from free list    */
            n->next = a->used_list;         /* push on used list        */
            a->used_list = n;
            return n->addr;
        }

        /* The range is larger than needed: carve the allocation out
           of its front.  A fresh node describes the allocation...  */
        mangled(node) *r = mangled(node_get)(a);
        if( !r ) return NULL;
        r->addr = n->addr;
        r->size = total;
        r->next = a->used_list;
        a->used_list = r;

        /* ...and the free range just loses the bytes we took.  */
        n->addr += total;
        n->size -= total;
        return r->addr;
    }

    /* Step 2: no free range fits, so grow the data area.  We need a
       node first; it is returned to the spare list if the data then
       turns out not to fit.  */
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

/* Free the allocation that starts at P.

   The allocation is looked up in the used list by ADDRESS: if a node
   has the same address as P, the size stored in that node is the size
   of the allocation.  A pointer that is not found (a bogus pointer or
   a double free) is silently ignored, leaving the arena untouched.

   The freed range is then inserted in the free list, keeping it sorted
   by address, and merged with the previous and next ranges whenever
   they are adjacent in memory.  Finally, if the resulting range touches
   the top of the data area, it is handed back to the untouched region
   by lowering `offset'.  */
inline void
mangled(dump) (mangled(t) *a, mangled(addr) p)
{   if( !p ) return;

    /* Find the node whose address equals P and detach it from the used
       list.  R stays NULL if there is no such node.  */
    mangled(node) *r = NULL;
    for( mangled(node) **l = &a->used_list; *l; l = &(*l)->next )
    /* -> */ if( (*l)->addr == (mangled(byte)*)p ) {
        r = *l;
        *l = r->next;
        break;
    }

    if( !r ) return;    /* not ours, or already freed: nothing to do    */

    /* Find the insertion point in the address-sorted free list.  When
       the loop ends, PREV is the last range before R (or NULL) and NEXT
       is the first range after it (or NULL).  */
    mangled(node) *prev = NULL, *next = a->free_list;
    while( next && next->addr < r->addr ) { prev = next; next = next->next; }
    r->next = next;

    /* Merge with the range that follows, if R ends exactly where it
       starts.  The absorbed node is recycled.  */
    if( next && r->addr + r->size == next->addr ) {
        r->size += next->size;
        r->next  = next->next;
        mangled(node_put)(a, next);
    }

    /* Merge with the range that precedes, if it ends exactly where R
       starts.  In that case PREV swallows R, and R is recycled.
       Otherwise, R is linked in as a range of its own.  */
    if( prev && prev->addr + prev->size == r->addr ) {
        prev->size += r->size;
        prev->next  = r->next;
        mangled(node_put)(a, r);
        r = prev;                       /* R is now the merged range    */
    } else if( prev ) prev->next = r;
    else a->free_list = r;              /* R is the new list head       */

    /* If the range reaches the top of the data area, there is nothing
       above it that is still in use, so give it back to the untouched
       region by lowering the top.  Being the highest range, it is
       necessarily the LAST node of the free list, so it is unlinked
       from there and recycled.  */
    if( r->addr + r->size == a->mem + a->offset ) {
        a->offset = r->addr - a->mem;
        mangled(node) **l = &a->free_list;
        while(*l != r) l = &(*l)->next;
        *l = NULL;
        mangled(node_put)(a, r);
    }
}

/* Forget every allocation at once, keeping the underlying memory.

   The arena goes back to its just-started state: the data area and the
   node table are empty, and all three lists are dropped.  Every pointer
   previously returned by `alloc' becomes invalid.  This is the classic
   arena "free everything" operation, and it runs in constant time.  */
inline void
mangled(reset) (mangled(t) *a)
{   a->offset = 0;
    a->meta = 0;
    a->free_list = a->used_list = a->spare = NULL;
}

/* Return the whole arena to the operating system.

   After this call, A is left in a safe, empty state (`capacity' is
   zero and `mem' is NULL), so a repeated `destroy' or a later `alloc'
   is harmless, and any pointer obtained from the arena is invalid.  */
inline void
mangled(destroy) (mangled(t) *a)
{   if( a->mem ) mangled(munmap)(a->mem, a->capacity);
    a->mem = NULL;
    a->capacity = a->offset = a->meta = 0;
    a->free_list = a->used_list = a->spare = NULL;
}
