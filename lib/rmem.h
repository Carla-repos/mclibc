/* rmem.h -- Public interface of the raw allocator (one mapping per block).

   Author:  contato.lucasdwbfff@gmail.com

   This file is part of mclibc.

   mclibc is a MINIMAL package of functions that ships bundled with
   Carla, the programming language.  Everything in mclibc is written to
   be tiny, dependency free (no libc, no malloc) and easy to audit.

   `raw' is the simplest allocator of the package: it behaves like a
   `malloc' / `free' pair, but every allocation is its OWN mapping,
   obtained straight from the operating system with `mmap' (or
   `VirtualAlloc') and given back with `munmap' (or `VirtualFree').

   The handles (refs) that `alloc' returns do not live in the mapped
   blocks.  They live in ONE global bookkeeping region, whose size is
   chosen by the program through `bookkeeping_set'.

   The implementation lives in `raw.c'.  This header only declares
   the types and the public functions.  For the code conventions, see
   `nashj.h' and `nashj.c', which are the reference example.  */

#include <stddef.h>

/* ---------------------------------------------------------------------
   Namespacing.  See `nashj.h'.  */

#define mangled(id) rmem_##id

/* ---------------------------------------------------------------------
   Basic types.  */

/* A single byte of memory.  */
typedef unsigned char mangled(byte);

/* An opaque address handed to (or received from) the user.  */
typedef void* mangled(addr);

/* ---------------------------------------------------------------------
   Ref: the handle returned by `alloc'.

   Same idea as the ref of `nashj': the address of the allocation and
   a byte that tells whether it has already been freed.  There is one
   difference.  The nashj finds the size of an allocation through its
   node lists, but `raw' has no lists, and `munmap' needs the length.
   So the ref also stores the size.

   A ref is NEVER recycled, so `freed' keeps answering correctly after
   `dump'.  Each `alloc' consumes one ref from the global bookkeeping,
   and the bookkeeping has a fixed size (see `bookkeeping_set').  */

typedef struct mangled(ref) {
    mangled(byte) freed;            /* 0 while live, 1 once freed.       */
    mangled(addr) ptr;              /* first byte of the allocation.     */
    size_t size;                    /* length that was mapped, in bytes. */
} mangled(ref);

/* ---------------------------------------------------------------------
   Public functions, listed as an X-macro.  See `nashj.h'.

     bookkeeping_set  Map the global bookkeeping region with BYTES
                      bytes.  Must be called once, before `alloc'.
                      Returns 0, or -1 on failure.
     alloc            Map SIZE zero-filled bytes and return a ref, or
                      NULL on failure.
     dump             Unmap the allocation of a ref and mark the ref
                      as freed.  */

#define FUNCTIONS                                                           \
X(bookkeeping_set, int,           (size_t bytes))                           \
X(alloc,           mangled(ref)*, (size_t size))                            \
X(dump,            void,          (mangled(ref) *handle))

#define X(id, ret, args)                                            \
extern inline ret mangled(id) args;
FUNCTIONS
#undef X
