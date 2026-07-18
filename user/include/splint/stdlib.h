#ifndef SPLINT_USER_STDLIB_H
#define SPLINT_USER_STDLIB_H

#include <stddef.h>

void *splint_malloc(size_t size);
void splint_free(void *pointer);
void *splint_calloc(size_t count, size_t size);
void *splint_realloc(void *pointer, size_t size);

#endif
