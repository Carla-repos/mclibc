#ifndef ARENA_USE_IN_MCLIBC_H
#define ARENA_USE_IN_MCLIBC_H
/* ---------------------------------------------------------------------
   Ref: the handle returned by `alloc'.

   It holds the address of the allocation and one byte that tells
   whether that address has already been freed.  The handle is stored
   inside the arena and, unlike a node, it is NEVER recycled while the
   arena lives (only `reset' and `destroy' invalidate it).  This is
   what lets `freed' keep answering correctly after `dump'.

   Use `makeraw' to obtain the plain address from a handle.  */

typedef struct { char freed; void* addr; } ref;
#endif
