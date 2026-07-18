#ifndef SPLINTOS_ARCH_X86_64_PHYSICAL_H
#define SPLINTOS_ARCH_X86_64_PHYSICAL_H

#include <stdint.h>

int x86_64_physical_init(uint32_t multiboot_info);
uint64_t x86_64_physical_alloc(void);
uint64_t x86_64_physical_alloc_below(uint64_t maximum_address);
int x86_64_physical_free(uint64_t address);
uint64_t x86_64_physical_free_pages(void);

#endif
