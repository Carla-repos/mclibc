#include <stddef.h>

#define mangled(id) arena_##id

typedef unsigned char mangled(byte);
typedef void* mangled(addr);

typedef struct mangled(node) {
    size_t size;
    struct mangled(node) *next;
} mangled(node);

typedef struct mangled(t) {
    mangled(byte) *mem;
    size_t capacity;
    size_t offset;
    mangled(node) *free_list;
} mangled(t);

typedef struct mangled(f) {
    mangled(byte) *addr;
    size_t length;
} mangled(f);

#define ARENA_ALIGN 16
#define ARENA_NORM(n)                                                          \
    ( ( ((n) < sizeof(mangled(node)) ? sizeof(mangled(node)) : (n))            \
        + ARENA_ALIGN - 1 ) & ~(size_t)(ARENA_ALIGN - 1) )


#define FUNCTIONS                                                              \
X(start,   mangled(t),    (size_t bytes))                                      \
X(alloc,   mangled(addr), (mangled(t) *arena, size_t size))                    \
X(dump,    void,          (mangled(t) *arena, mangled(addr) ptr, size_t size)) \
X(reset,   void,          (mangled(t) *arena))                                 \
X(destroy, void,          (mangled(t) *arena))

#define X(id, ret, args)                                                       \
extern inline ret mangled(id) args;
FUNCTIONS
#undef X
