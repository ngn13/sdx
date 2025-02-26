#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

void *heap_alloc(uint64_t size);              // allocate an arbitrary sized buffer
void *heap_realloc(void *mem, uint64_t size); // resize an arbitrary sized allocated buffer
void  heap_free(void *mem);                   // free an arbtirary sized buffer

#endif
