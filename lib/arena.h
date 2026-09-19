#include <stddef.h>

#define mangled(id) arena_##id

typedef unsigned char mangled(byte);
typedef void* mangled(addr);

typedef struct mangled(t) {
    mangled(byte) *mem;
    size_t capacity;
    size_t offset;
} mangled(t);

#define FUNCTIONS \
X(dump, void, (mangled(t) *arena)) \
X(start, mangled(t), (size_t bytes)) \
X(destroy, void, (mangled(t) *arena)) \
X(alloc, mangled(addr), (mangled(t) *arena, size_t size)) \

#define X(id, ret, args) extern inline ret mangled(id) args;
FUNCTIONS
#undef X
