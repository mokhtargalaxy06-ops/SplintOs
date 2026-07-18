#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/physical.h"

#define PACKED __attribute__((packed))

enum {
    PAGE_SHIFT = 12,
    PAGE_SIZE = 1U << PAGE_SHIFT,
    MAX_PHYSICAL_GIB = 16,
    MAX_PAGES = MAX_PHYSICAL_GIB * 1024U * 1024U / 4U,
    BITMAP_BYTES = MAX_PAGES / 8U,
};

struct PACKED multiboot_info_prefix {
    uint32_t flags, mem_lower, mem_upper, boot_device, command_line;
    uint32_t modules_count, modules_address;
    uint8_t symbols[16];
    uint32_t memory_map_length, memory_map_address;
};

struct PACKED memory_map_entry {
    uint32_t size;
    uint64_t address, length;
    uint32_t type;
};

static uint8_t used[BITMAP_BYTES];
static uint8_t owned[BITMAP_BYTES];
static uint64_t free_pages;

static int bit_get(const uint8_t *map, uint32_t page)
{ return (map[page / 8] & (uint8_t)(1U << (page % 8))) != 0; }

static void bit_set(uint8_t *map, uint32_t page, int value)
{
    uint8_t mask = (uint8_t)(1U << (page % 8));
    if (value) map[page / 8] |= mask;
    else map[page / 8] &= (uint8_t)~mask;
}

static void mark_available(uint64_t start, uint64_t end)
{
    uint64_t first = (start + PAGE_SIZE - 1U) >> PAGE_SHIFT;
    uint64_t last = end >> PAGE_SHIFT;
    if (first > MAX_PAGES) first = MAX_PAGES;
    if (last > MAX_PAGES) last = MAX_PAGES;
    for (uint32_t page = (uint32_t)first; page < (uint32_t)last; ++page) {
        if (!bit_get(used, page)) continue;
        bit_set(used, page, 0);
        ++free_pages;
    }
}

static void reserve(uint64_t start, uint64_t end)
{
    uint64_t first = start >> PAGE_SHIFT;
    uint64_t last = (end + PAGE_SIZE - 1U) >> PAGE_SHIFT;
    if (last > MAX_PAGES) last = MAX_PAGES;
    for (uint32_t page = (uint32_t)first; page < (uint32_t)last; ++page) {
        if (bit_get(used, page)) continue;
        bit_set(used, page, 1);
        --free_pages;
    }
}

int x86_64_physical_init(uint32_t multiboot_address)
{
    extern uint8_t x86_64_kernel_start[], x86_64_kernel_end[];
    if (multiboot_address == 0 ||
        multiboot_address > UINT32_MAX - sizeof(struct multiboot_info_prefix)) return 0;
    const struct multiboot_info_prefix *info =
        (const struct multiboot_info_prefix *)(uintptr_t)multiboot_address;
    if ((info->flags & (1U << 6)) == 0 || info->memory_map_address == 0 ||
        info->memory_map_length < sizeof(struct memory_map_entry) ||
        info->memory_map_address > UINT32_MAX - info->memory_map_length) return 0;
    for (size_t i = 0; i < BITMAP_BYTES; ++i) { used[i] = 0xff; owned[i] = 0; }
    free_pages = 0;
    uint32_t cursor = info->memory_map_address;
    uint32_t end = cursor + info->memory_map_length;
    while (cursor < end) {
        if (end - cursor < sizeof(uint32_t)) return 0;
        const struct memory_map_entry *entry =
            (const struct memory_map_entry *)(uintptr_t)cursor;
        uint64_t step64 = (uint64_t)entry->size + sizeof(entry->size);
        if (entry->size < sizeof(*entry) - sizeof(entry->size) ||
            step64 > end - cursor) return 0;
        if (entry->type == 1 && entry->length != 0 &&
            entry->address <= UINT64_MAX - entry->length)
            mark_available(entry->address, entry->address + entry->length);
        cursor += (uint32_t)step64;
    }
    reserve(0, UINT64_C(0x100000));
    reserve((uintptr_t)x86_64_kernel_start, (uintptr_t)x86_64_kernel_end);
    uint64_t first = x86_64_physical_alloc();
    if (first == 0 || !x86_64_physical_free(first)) return 0;
    uint64_t second = x86_64_physical_alloc();
    if (second != first || !x86_64_physical_free(second)) return 0;
    return free_pages != 0;
}

uint64_t x86_64_physical_alloc(void)
{
    for (uint32_t page = 1; page < MAX_PAGES; ++page) {
        if (bit_get(used, page)) continue;
        bit_set(used, page, 1);
        bit_set(owned, page, 1);
        --free_pages;
        return (uint64_t)page << PAGE_SHIFT;
    }
    return 0;
}

uint64_t x86_64_physical_alloc_below(uint64_t maximum_address)
{
    if (maximum_address < PAGE_SIZE - 1U) return 0;
    uint64_t page_limit = (maximum_address - (PAGE_SIZE - 1U)) >> PAGE_SHIFT;
    if (page_limit >= MAX_PAGES) page_limit = MAX_PAGES - 1U;
    for (uint32_t page = 0x100000U >> PAGE_SHIFT;
         page <= (uint32_t)page_limit; ++page) {
        if (bit_get(used, page)) continue;
        bit_set(used, page, 1); bit_set(owned, page, 1); --free_pages;
        return (uint64_t)page << PAGE_SHIFT;
    }
    return 0;
}

int x86_64_physical_free(uint64_t address)
{
    if ((address & (PAGE_SIZE - 1U)) != 0 || address == 0 ||
        (address >> PAGE_SHIFT) >= MAX_PAGES) return 0;
    uint32_t page = (uint32_t)(address >> PAGE_SHIFT);
    if (!bit_get(used, page) || !bit_get(owned, page)) return 0;
    bit_set(owned, page, 0);
    bit_set(used, page, 0);
    ++free_pages;
    return 1;
}

uint64_t x86_64_physical_free_pages(void) { return free_pages; }
