#ifndef SPLINTOS_MEMORY_H
#define SPLINTOS_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool memory_init(uint32_t multiboot_info_address);
void *physical_page_alloc(void);
void physical_page_free(void *page);
void *kmalloc(size_t size);
void kfree(void *pointer);
uint32_t memory_total_kib(void);
uint32_t memory_free_kib(void);

#endif
