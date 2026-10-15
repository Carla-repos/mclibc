#ifndef DEVELOPMENT_H
#define DEVELOPMENT_H

#include <stdint.h>
#include <arena_ref.h>

#define mangled(id) development_##id

void mangled(fill) (ref *bytes, int32_t start, int32_t size);
void mangled(debug) (ref *str);
void mangled(exit) (int code);


#endif
