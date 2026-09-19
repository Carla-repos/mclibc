#include <stddef.h>

#define mangled(id) arena_##id

typedef unsigned char mangled(byte);
typedef void* mangled(addr);

typedef struct mangled(node) {
    mangled(byte) *addr;
    size_t size;
    struct mangled(node) *next;
} mangled(node);

typedef struct mangled(t) {
    mangled(byte) *mem;
    size_t capacity;
    size_t offset;                // topo dos dados (cresce pra cima)
    size_t meta;                  // bytes de nodes no fim (cresce pra baixo)
    mangled(node) *free_list;     // faixas livres, ordenadas por endereço
    mangled(node) *used_list;     // alocações ativas
    mangled(node) *spare;         // nodes reciclados
} mangled(t);

#define ARENA_ALIGN 16
#define ARENA_NORM(n)                                               \
    ( ( ((n) < sizeof(mangled(node)) ? sizeof(mangled(node)) : (n)) \
        + ARENA_ALIGN - 1 ) & ~(size_t)(ARENA_ALIGN - 1) )


#define FUNCTIONS                                                   \
X(start,   mangled(t),    (size_t bytes))                           \
X(alloc,   mangled(addr), (mangled(t) *arena, size_t size))         \
X(dump,    void,          (mangled(t) *arena, mangled(addr) ptr))   \
X(reset,   void,          (mangled(t) *arena))                      \
X(destroy, void,          (mangled(t) *arena))

#define X(id, ret, args)                                            \
extern inline ret mangled(id) args;
FUNCTIONS
#undef X
