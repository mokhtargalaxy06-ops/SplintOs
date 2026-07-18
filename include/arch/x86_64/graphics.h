#ifndef SPLINTOS_ARCH_X86_64_GRAPHICS_H
#define SPLINTOS_ARCH_X86_64_GRAPHICS_H
#include <stdint.h>
int x86_64_graphics_init(uint32_t multiboot_info);
int x86_64_graphics_conformance_test(void);
#endif
