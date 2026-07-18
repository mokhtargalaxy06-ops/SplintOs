#include "memory.h"

#include "devices.h"
#include "interrupts.h"
#include "arch/x86/cpu.h"

#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

enum {
    PAGE_SIZE = 4096,
    PAGE_COUNT = 1024 * 1024,
    BITMAP_SIZE = PAGE_COUNT / 8,
    HEAP_SIZE = 1024 * 1024,
};

struct PACKED multiboot_memory_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint8_t syms[16];
    uint32_t mmap_length, mmap_addr;
};

struct PACKED memory_map_entry {
    uint32_t size;
    uint64_t address;
    uint64_t length;
    uint32_t type;
};

struct heap_block {
    size_t size;
    bool free;
    struct heap_block *next;
    struct heap_block *previous;
};

extern uint8_t kernel_end;

static uint8_t page_bitmap[BITMAP_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint8_t page_allocated[BITMAP_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint32_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static uint8_t kernel_heap[HEAP_SIZE] __attribute__((aligned(16)));
static struct heap_block *heap_head;
static uint32_t total_pages;
static uint32_t free_pages;

enum {
    PAGE_PRESENT = 1U,
    PAGE_WRITABLE = 2U,
    PAGE_USER = 4U,
    PAGE_LARGE = 0x80U,
};

static void bytes_set(void *destination, uint8_t value, size_t length)
{
    uint8_t *bytes = destination;
    while (length-- != 0) *bytes++ = value;
}

static void page_mark_used(uint32_t page)
{
    if (page >= PAGE_COUNT) return;
    uint8_t mask = (uint8_t)(1U << (page & 7U));
    if ((page_bitmap[page >> 3] & mask) == 0) {
        page_bitmap[page >> 3] |= mask;
        if (free_pages != 0) --free_pages;
    }
}

static bool page_mark_free(uint32_t page)
{
    if (page >= PAGE_COUNT) return false;
    uint8_t mask = (uint8_t)(1U << (page & 7U));
    if ((page_bitmap[page >> 3] & mask) != 0) {
        page_bitmap[page >> 3] &= (uint8_t)~mask;
        ++free_pages;
        return true;
    }
    return false;
}

static bool page_is_allocated(uint32_t page)
{
    return page < PAGE_COUNT &&
           (page_allocated[page >> 3] & (1U << (page & 7U))) != 0;
}

static void page_set_allocated(uint32_t page, bool allocated)
{
    uint8_t mask = (uint8_t)(1U << (page & 7U));
    if (allocated) page_allocated[page >> 3] |= mask;
    else page_allocated[page >> 3] &= (uint8_t)~mask;
}

static void reserve_range(uintptr_t start, uintptr_t end)
{
    if (end <= start) return;
    uint32_t first = (uint32_t)(start / PAGE_SIZE);
    uint64_t rounded_end = (uint64_t)end + PAGE_SIZE - 1;
    uint32_t last = rounded_end / PAGE_SIZE > PAGE_COUNT
        ? PAGE_COUNT : (uint32_t)(rounded_end / PAGE_SIZE);
    for (uint32_t page = first; page < last; ++page) page_mark_used(page);
}

static void paging_enable(void)
{
    /* Kernel and device identity mappings are supervisor-only. */
    for (uint32_t i = 0; i < 1024; ++i)
        page_directory[i] = (i << 22) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE;
    arch_enable_paging((uint32_t)(uintptr_t)page_directory);
}

static void heap_init(void)
{
    heap_head = (struct heap_block *)kernel_heap;
    heap_head->size = HEAP_SIZE - sizeof(*heap_head);
    heap_head->free = true;
    heap_head->next = NULL;
    heap_head->previous = NULL;
}

static bool heap_failure_self_test(void)
{
    void *blocks[256];
    size_t count = 0;
    while (count < sizeof(blocks) / sizeof(blocks[0])) {
        blocks[count] = kmalloc(PAGE_SIZE);
        if (blocks[count] == NULL) break;
        ++count;
    }
    bool exhausted = count != 0 && count < sizeof(blocks) / sizeof(blocks[0]) &&
                     kmalloc(PAGE_SIZE) == NULL && kmalloc(SIZE_MAX) == NULL;
    while (count != 0) kfree(blocks[--count]);
    void *coalesced = kmalloc(HEAP_SIZE / 2);
    bool reusable = coalesced != NULL;
    if (coalesced != NULL) kfree((uint8_t *)coalesced + 8);
    void *overlap = kmalloc(HEAP_SIZE / 2);
    bool interior_rejected = overlap == NULL;
    kfree(overlap);
    kfree(coalesced);
    return exhausted && reusable && interior_rejected;
}

static bool physical_allocator_self_test(void)
{
    uint32_t before = free_pages;
    uint32_t unowned = PAGE_COUNT;
    for (uint32_t page = 0; page < PAGE_COUNT; ++page) {
        uint8_t mask = (uint8_t)(1U << (page & 7U));
        if ((page_bitmap[page >> 3] & mask) == 0) {
            unowned = page;
            break;
        }
    }
    if (unowned == PAGE_COUNT) return false;
    physical_page_free((void *)(uintptr_t)(unowned * PAGE_SIZE));
    if (free_pages != before) return false;
    void *page = physical_page_alloc();
    if (page == NULL || free_pages + 1 != before) return false;
    physical_page_free(page);
    if (free_pages != before) return false;
    physical_page_free(page);
    return free_pages == before;
}

static bool memory_map_valid(uint32_t address, uint32_t length)
{
    if (address == 0 || length < sizeof(struct memory_map_entry) ||
        length > UINT32_MAX - address) return false;
    uintptr_t cursor = address;
    uintptr_t end = cursor + length;
    while (cursor < end) {
        size_t remaining = end - cursor;
        if (remaining < sizeof(uint32_t)) return false;
        const struct memory_map_entry *entry =
            (const struct memory_map_entry *)cursor;
        if (entry->size < sizeof(*entry) - sizeof(entry->size)) return false;
        uint64_t step = (uint64_t)entry->size + sizeof(entry->size);
        if (step > remaining) return false;
        cursor += (uintptr_t)step;
    }
    return cursor == end;
}

static bool memory_map_validation_self_test(void)
{
    struct memory_map_entry entry = {
        sizeof(entry) - sizeof(entry.size), 0x100000, 0x200000, 1
    };
    uint32_t address = (uint32_t)(uintptr_t)&entry;
    if (!memory_map_valid(address, sizeof(entry))) return false;
    entry.size = sizeof(entry) - sizeof(entry.size) - 1;
    if (memory_map_valid(address, sizeof(entry))) return false;
    entry.size = sizeof(entry) - sizeof(entry.size);
    if (memory_map_valid(address, sizeof(entry) - 1)) return false;
    if (memory_map_valid(UINT32_MAX - 3U, 8)) return false;
    return true;
}

bool memory_init(uint32_t address)
{
    if (address == 0 || address > UINT32_MAX - sizeof(struct multiboot_memory_info)) {
        serial_write("SplintOS: rejected invalid boot information address\r\n");
        return false;
    }
    const struct multiboot_memory_info *info =
        (const struct multiboot_memory_info *)(uintptr_t)address;
    if (!memory_map_validation_self_test()) {
        serial_write("SplintOS: boot memory-map validation self-test failed\r\n");
        return false;
    }
    serial_write("SplintOS: checked boot memory-map fixtures online\r\n");
    if ((info->flags & (1U << 6)) == 0) {
        serial_write("SplintOS: bootloader supplied no memory map\r\n");
        return false;
    }
    if (!memory_map_valid(info->mmap_addr, info->mmap_length)) {
        serial_write("SplintOS: rejected malformed boot memory map\r\n");
        return false;
    }

    bytes_set(page_bitmap, 0xFF, sizeof(page_bitmap));
    bytes_set(page_allocated, 0, sizeof(page_allocated));
    total_pages = 0;
    free_pages = 0;
    uintptr_t cursor = info->mmap_addr;
    uintptr_t map_end = cursor + info->mmap_length;
    while (cursor < map_end) {
        const struct memory_map_entry *entry = (const struct memory_map_entry *)cursor;
        if (entry->type == 1 && entry->address < 0x100000000ULL) {
            uint64_t maximum = 0x100000000ULL - entry->address;
            uint64_t end = entry->length > maximum
                ? 0x100000000ULL : entry->address + entry->length;
            uint64_t start = (entry->address + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
            for (uint64_t physical = start; physical + PAGE_SIZE <= end; physical += PAGE_SIZE) {
                if (page_mark_free((uint32_t)(physical / PAGE_SIZE)))
                    ++total_pages;
            }
        }
        cursor += entry->size + sizeof(entry->size);
    }

    reserve_range(0, (uintptr_t)&kernel_end);
    reserve_range(address, address + sizeof(*info));
    reserve_range(info->mmap_addr, info->mmap_addr + info->mmap_length);
    if (!physical_allocator_self_test()) {
        serial_write("SplintOS: physical allocator ownership self-test failed\r\n");
        return false;
    }
    serial_write("SplintOS: physical allocator ownership online\r\n");
    paging_enable();
    heap_init();
    if (!heap_failure_self_test()) {
        serial_write("SplintOS: kernel heap failure self-test failed\r\n");
        return false;
    }
    serial_write("SplintOS: kernel heap exhaustion and reuse online\r\n");
    serial_write("SplintOS: physical allocator, paging and heap online\r\n");
    return true;
}

void *physical_page_alloc(void)
{
    KERNEL_ASSERT(free_pages <= total_pages, "physical page accounting");
    for (uint32_t byte = 0; byte < BITMAP_SIZE; ++byte) {
        if (page_bitmap[byte] == 0xFF) continue;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            uint32_t page = byte * 8 + bit;
            if ((page_bitmap[byte] & (1U << bit)) == 0) {
                page_mark_used(page);
                page_set_allocated(page, true);
                return (void *)(uintptr_t)(page * PAGE_SIZE);
            }
        }
    }
    return NULL;
}

void physical_page_free(void *pointer)
{
    uintptr_t address = (uintptr_t)pointer;
    if ((address & (PAGE_SIZE - 1)) != 0 || address < (uintptr_t)&kernel_end) return;
    uint32_t page = (uint32_t)(address / PAGE_SIZE);
    if (!page_is_allocated(page)) return;
    page_set_allocated(page, false);
    (void)page_mark_free(page);
    KERNEL_ASSERT(free_pages <= total_pages, "physical page accounting");
}

void *kmalloc(size_t size)
{
    if (size == 0 || size > SIZE_MAX - 7U) return NULL;
    size = (size + 7U) & ~7U;
    for (struct heap_block *block = heap_head; block != NULL; block = block->next) {
        if (!block->free || block->size < size) continue;
        if (block->size >= size + sizeof(*block) + 16) {
            struct heap_block *split = (struct heap_block *)((uint8_t *)(block + 1) + size);
            split->size = block->size - size - sizeof(*split);
            split->free = true;
            split->next = block->next;
            split->previous = block;
            if (split->next != NULL) split->next->previous = split;
            block->next = split;
            block->size = size;
        }
        block->free = false;
        return block + 1;
    }
    return NULL;
}

void kfree(void *pointer)
{
    if (pointer == NULL || (uint8_t *)pointer < kernel_heap + sizeof(struct heap_block) ||
        (uint8_t *)pointer >= kernel_heap + HEAP_SIZE) return;
    struct heap_block *block = heap_head;
    while (block != NULL && block + 1 != pointer) block = block->next;
    if (block == NULL || block->free) return;
    block->free = true;
    if (block->next != NULL && block->next->free) {
        block->size += sizeof(*block) + block->next->size;
        block->next = block->next->next;
        if (block->next != NULL) block->next->previous = block;
    }
    if (block->previous != NULL && block->previous->free) {
        block = block->previous;
        block->size += sizeof(*block) + block->next->size;
        block->next = block->next->next;
        if (block->next != NULL) block->next->previous = block;
    }
}

uint32_t memory_total_kib(void) { return total_pages * 4; }
uint32_t memory_free_kib(void) { return free_pages * 4; }

uint32_t *address_space_kernel(void) { return page_directory; }

void address_space_activate(uint32_t *directory)
{
    if (directory == NULL) directory = page_directory;
    arch_load_page_directory((uint32_t)(uintptr_t)directory);
}

uint32_t *address_space_create(void)
{
    uint32_t *directory = physical_page_alloc();
    if (directory == NULL) return NULL;
    for (uint32_t i = 0; i < 1024; ++i) directory[i] = page_directory[i];
    return directory;
}

bool address_space_map_user(uint32_t *directory, uintptr_t virtual_address,
                            void *physical_page, bool writable)
{
    if (directory == NULL || physical_page == NULL ||
        (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        virtual_address < 0x40000000U || virtual_address >= 0xC0000000U)
        return false;
    uint32_t directory_index = (uint32_t)(virtual_address >> 22);
    uint32_t table_index = (uint32_t)((virtual_address >> 12) & 0x3FFU);
    uint32_t *table;
    if ((directory[directory_index] & PAGE_LARGE) != 0) {
        table = physical_page_alloc();
        if (table == NULL) return false;
        bytes_set(table, 0, PAGE_SIZE);
        directory[directory_index] = (uint32_t)(uintptr_t)table |
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    } else {
        table = (uint32_t *)(uintptr_t)(directory[directory_index] & ~0xFFFU);
        if (table == NULL) return false;
    }
    if ((table[table_index] & PAGE_PRESENT) != 0) return false;
    table[table_index] = (uint32_t)(uintptr_t)physical_page | PAGE_PRESENT |
        PAGE_USER | (writable ? PAGE_WRITABLE : 0U);
    return true;
}

bool address_space_unmap_user(uint32_t *directory, uintptr_t virtual_address)
{
    if (directory == NULL || (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        virtual_address < 0x40000000U || virtual_address >= 0xC0000000U)
        return false;
    uint32_t directory_index = (uint32_t)(virtual_address >> 22);
    uint32_t pde = directory[directory_index];
    if ((pde & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER) ||
        (pde & PAGE_LARGE) != 0) return false;
    uint32_t *table = (uint32_t *)(uintptr_t)(pde & ~0xFFFU);
    uint32_t table_index = (uint32_t)((virtual_address >> 12) & 0x3FFU);
    uint32_t pte = table[table_index];
    if ((pte & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER)) return false;
    table[table_index] = 0;
    arch_invalidate_page((const void *)virtual_address);
    physical_page_free((void *)(uintptr_t)(pte & ~0xFFFU));
    bool empty = true;
    for (size_t i = 0; i < 1024; ++i)
        if ((table[i] & PAGE_PRESENT) != 0) { empty = false; break; }
    if (empty) {
        directory[directory_index] = 0;
        physical_page_free(table);
    }
    return true;
}

bool user_range_valid(uint32_t *directory, uintptr_t address, size_t length,
                      bool writable)
{
    if (directory == NULL || length == 0 || address < 0x40000000U ||
        address >= 0xC0000000U || length > 0xC0000000U - address)
        return false;
    uintptr_t end = address + length - 1;
    uintptr_t last_page = end & ~(uintptr_t)(PAGE_SIZE - 1);
    for (uintptr_t page = address & ~(uintptr_t)(PAGE_SIZE - 1);;) {
        uint32_t pde = directory[page >> 22];
        if ((pde & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER) ||
            (pde & PAGE_LARGE) != 0) return false;
        uint32_t *table = (uint32_t *)(uintptr_t)(pde & ~0xFFFU);
        uint32_t pte = table[(page >> 12) & 0x3FFU];
        if ((pte & (PAGE_PRESENT | PAGE_USER)) != (PAGE_PRESENT | PAGE_USER) ||
            (writable && (pte & PAGE_WRITABLE) == 0)) return false;
        if (page == last_page) break;
        page += PAGE_SIZE;
    }
    return true;
}

void address_space_destroy(uint32_t *directory)
{
    if (directory == NULL || directory == page_directory) return;
    for (uint32_t i = 256; i < 768; ++i) {
        uint32_t pde = directory[i];
        if ((pde & PAGE_PRESENT) == 0 || (pde & PAGE_LARGE) != 0 ||
            (pde & PAGE_USER) == 0) continue;
        uint32_t *table = (uint32_t *)(uintptr_t)(pde & ~0xFFFU);
        for (uint32_t j = 0; j < 1024; ++j)
            if ((table[j] & (PAGE_PRESENT | PAGE_USER)) == (PAGE_PRESENT | PAGE_USER))
                physical_page_free((void *)(uintptr_t)(table[j] & ~0xFFFU));
        physical_page_free(table);
    }
    physical_page_free(directory);
}
