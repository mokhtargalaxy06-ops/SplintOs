#ifndef SPLINTOS_KERNEL_H
#define SPLINTOS_KERNEL_H

#include <stdint.h>

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info);
void terminal_write(const char *text);

#endif
