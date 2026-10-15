#ifndef DEVELOPMENT_H
#define DEVELOPMENT_H

#include <stdint.h>

#define mangled(id) development_##id

void mangled(fill) (char *bytes, int32_t start, int32_t size);
void mangled(debug) (char *str);
void mangled(exit) (int code);


#endif
