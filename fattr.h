#ifndef FATTR_H
#define FATTR_H

#include <stdint.h>

typedef struct {
    uint32_t hash;
    const char * name;
    size_t size;
    const uint8_t * content;
} file_attr_t;

#endif
