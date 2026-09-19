/* arena.h -- Public interface of the arena allocator (no per-block headers).

   Author:  contato.lucasdwbfff@gmail.com

   This file is part of mclibc.

   mclibc is a MINIMAL package of functions that ships bundled with
   Carla, the programming language.  Everything in mclibc is written to
   be tiny, dependency free (no libc, no malloc) and easy to audit.

   The implementation lives in `arena.c'.  This header only declares
   the types, the constants and the public functions.

   ---------------------------------------------------------------------
   CODE CONVENTIONS (READ THIS BEFORE WRITING ANY "stdlibcore" LIB)
   ---------------------------------------------------------------------

   The code in this file and in `arena.c' is the REFERENCE EXAMPLE for
   every "lib" that belongs to Carla's "stdlibcore".  Its style is
   intentionally uniform, so that all libs look and behave the same
   way.  The rules that this header demonstrates are:

   1. Namespacing.  Every identifier that the lib defines is spelled
      through the `mangled (id)' macro.  For this lib,
      `mangled (alloc)' expands to `arena_alloc'.  Changing the prefix
      means changing ONE line.  Only the lib's own constants and
      macros (such as ARENA_ALIGN) carry the prefix in plain text, in
      upper case.

   2. One header per lib, one source file per lib.  The header holds
      types, constants and the public function table; the source file
      holds every definition.

   3. Public API through an X-macro.  The functions are listed ONCE, in
      a `FUNCTIONS' table, and that single list is expanded to emit
      the declarations.  Adding a public function means adding one line
      to the table, so the declarations can never drift apart.

   4. Data types are plain structs, passed to functions by pointer,
      with no hidden global state.  The caller owns all the memory.

   5. Comments follow the GNU style: full sentences, two spaces after a
      period, and the closing marker of a multi-line comment on the
      last line of text.

   For the function layout and the platform rules, see `arena.c'.  */

#include <stddef.h>

/* ---------------------------------------------------------------------
   Namespacing.

   `mangled (id)' pastes the prefix `arena_' in front of ID, so that
   the lib's names cannot clash with the ones of any other lib or of
   the user's program.  It is used for types and for functions alike.  */

#define mangled(id) arena_##id

/* ---------------------------------------------------------------------
   Basic types.  */

/* A single byte of memory.  Unsigned, so that pointer arithmetic on
   `byte *' moves one byte at a time and never sign-extends.  */
typedef unsigned char mangled(byte);

/* An opaque address handed to (or received from) the user.  It is a
   `void *' so that it converts to any pointer type without a cast.  */
typedef void* mangled(addr);

/* ---------------------------------------------------------------------
   Node: describes a range of memory as "ADDR until ADDR + SIZE".

   There is no concept of chunk or block with a header.  A node is just
   an address and a length, and the same structure describes both live
   allocations and freed ranges.  Nodes are stored INSIDE the arena, at
   its end, so no external allocator is needed to manage them.

   The same node lives in exactly one of three lists (see `t' below).  */

typedef struct mangled(node) {
    mangled(byte) *addr;            /* first byte of the range.          */
    size_t size;                    /* length of the range, in bytes.    */
    struct mangled(node) *next;     /* next node in the same list, or
                                       NULL at the end of the list.      */
} mangled(node);

/* ---------------------------------------------------------------------
   Arena: the state of one allocator.

   The arena is ONE contiguous region obtained from the operating
   system.  Inside it, two areas grow towards each other:

       low addresses                                    high addresses
       +--------------------+-----------------+----------------------+
       | user data  ->      |   (free room)   |      <-  node table  |
       +--------------------+-----------------+----------------------+
       ^ mem                ^ mem+offset        ^ mem+limit-meta       ^ mem+limit

   The struct is small and is returned BY VALUE from `start'; every
   other function receives a pointer to it.  A zero-filled struct is a
   valid, empty arena that has no memory (every `alloc' on it fails).  */

typedef struct mangled(t) {
    mangled(byte) *mem;         /* base of the region given by the OS.   */
    size_t capacity;            /* size of that region, in bytes.        */

    size_t offset;              /* top of the data area.  It grows
                                   upwards; everything below it has been
                                   handed out at least once.             */
    size_t meta;                /* bytes used by the node table at the
                                   end of the region.  It grows
                                   downwards.                            */

    mangled(node) *free_list;   /* ranges that were freed, sorted by
                                   address and merged with their
                                   neighbours when they touch.           */
    mangled(node) *used_list;   /* live allocations.  Looked up by
                                   address when the user frees one.      */
    mangled(node) *spare;       /* dead nodes kept for reuse, so that the
                                   node table is bounded by the PEAK
                                   number of simultaneous allocations.   */
} mangled(t);

/* ---------------------------------------------------------------------
   Alignment.

   ARENA_ALIGN is the alignment, in bytes, of every pointer returned by
   `alloc'.  It MUST be a power of two: the rounding below relies on it.
   16 bytes satisfies the strictest scalar type on x86_64 (and SSE
   vectors), and the base of the region is page aligned by the OS.  */

#define ARENA_ALIGN 16

/* Round the request N up to the size that is really reserved.

   Two things happen, in this order:

     1. N is raised to at least sizeof (node).  This is a minimum
        block size that fits a node.

     2. The result is rounded UP to the next multiple of ARENA_ALIGN,
        with the usual trick `(x + A - 1) & ~(A - 1)', valid because A
        is a power of two.

   The macro evaluates N more than once, so N must not have side
   effects.  */

#define ARENA_NORM(n)                                               \
    ( ( ((n) < sizeof(mangled(node)) ? sizeof(mangled(node)) : (n)) \
        + ARENA_ALIGN - 1 ) & ~(size_t)(ARENA_ALIGN - 1) )

/* ---------------------------------------------------------------------
   Public functions, listed as an X-macro.

   Each line is `X (name, return type, (parameters))'.  Expanding
   `FUNCTIONS' with different definitions of `X' yields different
   artifacts from the same single list.  Here it emits the declarations;
   the contract of each function is documented in `arena.c', next to its
   definition.

     start    Create an arena backed by BYTES bytes from the OS.
     alloc    Allocate SIZE bytes; NULL if there is no room.
     dump     Free the allocation that starts at PTR (the size is
              found by the arena itself, from the address).
     reset    Forget every allocation at once, keeping the memory.
     destroy  Return the whole arena to the OS.  */

#define FUNCTIONS                                                   \
X(start,   mangled(t),    (size_t bytes))                           \
X(alloc,   mangled(addr), (mangled(t) *arena, size_t size))         \
X(dump,    void,          (mangled(t) *arena, mangled(addr) ptr))   \
X(reset,   void,          (mangled(t) *arena))                      \
X(destroy, void,          (mangled(t) *arena))

/* Emit one `extern inline' declaration per entry of the table.

   In C99 and later, an `inline' definition in `arena.c' together with
   an `extern inline' declaration visible in that same file makes the
   compiler emit ONE external definition of each function.  Other
   translation units can inline the calls or link against that
   definition, with no duplicated symbols.  */

#define X(id, ret, args)                                            \
extern inline ret mangled(id) args;
FUNCTIONS
#undef X
