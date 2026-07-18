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
uint32_t *address_space_create(void);
void address_space_destroy(uint32_t *directory);
bool address_space_map_user(uint32_t *directory, uintptr_t virtual_address,
                            void *physical_page, bool writable);
bool address_space_unmap_user(uint32_t *directory, uintptr_t virtual_address);
void address_space_activate(uint32_t *directory);
uint32_t *address_space_kernel(void);
bool user_range_valid(uint32_t *directory, uintptr_t address, size_t length,
                      bool writable);

#endif
