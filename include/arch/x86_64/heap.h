#ifndef SPLINTOS_ARCH_X86_64_HEAP_H
#define SPLINTOS_ARCH_X86_64_HEAP_H

#include <stddef.h>

int x86_64_heap_init(void);
void *x86_64_heap_alloc(size_t size);
void x86_64_heap_free(void *pointer);

#endif
