#ifndef SPLINTOS_ARCH_X86_64_DMA_H
#define SPLINTOS_ARCH_X86_64_DMA_H
#include <stddef.h>
#include <stdint.h>
enum x86_64_dma_direction { X86_64_DMA_TO_DEVICE, X86_64_DMA_FROM_DEVICE,
                            X86_64_DMA_BIDIRECTIONAL };
struct x86_64_dma_mapping {
    void *original, *cpu_address;
    uint64_t device_address;
    size_t size;
    uint8_t direction, bounced, active, slot;
};
int x86_64_dma_address_valid(uint64_t address, size_t size, uint64_t mask);
int x86_64_dma_map(void *buffer, size_t size, uint64_t mask,
                   enum x86_64_dma_direction direction,
                   struct x86_64_dma_mapping *mapping);
int x86_64_dma_map_bounce(void *buffer, size_t size, uint64_t mask,
                          enum x86_64_dma_direction direction,
                          struct x86_64_dma_mapping *mapping);
int x86_64_dma_unmap(struct x86_64_dma_mapping *mapping);
int x86_64_dma_conformance_test(void);
#endif
