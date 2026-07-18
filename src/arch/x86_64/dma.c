#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/dma.h"
#include "arch/x86_64/physical.h"

enum { PAGE_SIZE = 4096, DMA_SLOTS = 8 };
static uint8_t slots[DMA_SLOTS];

static void copy(void *destination, const void *source, size_t size)
{
    uint8_t *to = destination; const uint8_t *from = source;
    for (size_t i = 0; i < size; ++i) to[i] = from[i];
}

int x86_64_dma_address_valid(uint64_t address, size_t size, uint64_t mask)
{
    return size != 0 && address <= mask && size - 1U <= mask - address;
}

static int map_internal(void *buffer, size_t size, uint64_t mask,
                        enum x86_64_dma_direction direction,
                        struct x86_64_dma_mapping *mapping, int force_bounce)
{
    if (buffer == NULL || mapping == NULL || size == 0 || size > PAGE_SIZE ||
        direction > X86_64_DMA_BIDIRECTIONAL) return 0;
    *mapping = (struct x86_64_dma_mapping){0};
    uint64_t address = (uintptr_t)buffer;
    if (!force_bounce && x86_64_dma_address_valid(address, size, mask)) {
        *mapping = (struct x86_64_dma_mapping){buffer, buffer, address, size,
            direction, 0, 1, UINT8_MAX};
        return 1;
    }
    uint8_t slot;
    for (slot = 0; slot < DMA_SLOTS && slots[slot]; ++slot) {}
    if (slot == DMA_SLOTS) return 0;
    uint64_t page = x86_64_physical_alloc_below(mask);
    if (page == 0 || !x86_64_dma_address_valid(page, size, mask)) {
        if (page != 0) (void)x86_64_physical_free(page);
        return 0;
    }
    slots[slot] = 1;
    *mapping = (struct x86_64_dma_mapping){buffer, (void *)(uintptr_t)page,
        page, size, direction, 1, 1, slot};
    if (direction != X86_64_DMA_FROM_DEVICE) copy(mapping->cpu_address, buffer, size);
    return 1;
}

int x86_64_dma_map(void *buffer, size_t size, uint64_t mask,
                   enum x86_64_dma_direction direction,
                   struct x86_64_dma_mapping *mapping)
{ return map_internal(buffer, size, mask, direction, mapping, 0); }
int x86_64_dma_map_bounce(void *buffer, size_t size, uint64_t mask,
                          enum x86_64_dma_direction direction,
                          struct x86_64_dma_mapping *mapping)
{ return map_internal(buffer, size, mask, direction, mapping, 1); }

int x86_64_dma_unmap(struct x86_64_dma_mapping *mapping)
{
    if (mapping == NULL || !mapping->active) return 0;
    if (mapping->bounced) {
        if (mapping->direction != X86_64_DMA_TO_DEVICE)
            copy(mapping->original, mapping->cpu_address, mapping->size);
        if (mapping->slot >= DMA_SLOTS || !slots[mapping->slot] ||
            !x86_64_physical_free(mapping->device_address)) return 0;
        slots[mapping->slot] = 0;
    }
    mapping->active = 0;
    return 1;
}

int x86_64_dma_conformance_test(void)
{
    uint8_t source[128], result[128];
    for (size_t i = 0; i < sizeof(source); ++i) source[i] = (uint8_t)(i ^ 0x9bU);
    struct x86_64_dma_mapping direct;
    if (!x86_64_dma_map(source, sizeof(source), UINT64_MAX,
                        X86_64_DMA_TO_DEVICE, &direct) || direct.bounced ||
        direct.device_address != (uintptr_t)source || !x86_64_dma_unmap(&direct)) return 0;
    struct x86_64_dma_mapping mappings[DMA_SLOTS];
    for (size_t slot = 0; slot < DMA_SLOTS; ++slot) {
        if (!x86_64_dma_map_bounce(source, sizeof(source), UINT32_MAX,
                                   X86_64_DMA_BIDIRECTIONAL, &mappings[slot]) ||
            !mappings[slot].bounced || mappings[slot].device_address > UINT32_MAX)
            return 0;
    }
    struct x86_64_dma_mapping exhausted;
    if (x86_64_dma_map_bounce(source, sizeof(source), UINT32_MAX,
                              X86_64_DMA_TO_DEVICE, &exhausted)) return 0;
    for (size_t i = 0; i < sizeof(result); ++i)
        ((uint8_t *)mappings[0].cpu_address)[i] = (uint8_t)(i ^ 0x42U);
    mappings[0].original = result;
    if (!x86_64_dma_unmap(&mappings[0])) return 0;
    for (size_t i = 0; i < sizeof(result); ++i)
        if (result[i] != (uint8_t)(i ^ 0x42U)) return 0;
    for (size_t slot = 1; slot < DMA_SLOTS; ++slot)
        if (!x86_64_dma_unmap(&mappings[slot])) return 0;
    return !x86_64_dma_address_valid(UINT32_MAX - 3U, 8, UINT32_MAX) &&
           !x86_64_dma_map_bounce(source, PAGE_SIZE + 1U, UINT32_MAX,
                                  X86_64_DMA_TO_DEVICE, &exhausted);
}
