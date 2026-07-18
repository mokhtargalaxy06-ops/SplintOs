#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/layout.h"
#include "splint/abi64.h"

_Static_assert(sizeof(void *) == 8, "x86_64 pointers must be 64-bit");
_Static_assert(sizeof(uintptr_t) == 8, "x86_64 uintptr_t must be 64-bit");
_Static_assert(sizeof(size_t) == 8, "x86_64 size_t must be 64-bit");

/* This object is never linked into the 32-bit kernel. */
void splintos_x86_64_toolchain_probe(void) {}
