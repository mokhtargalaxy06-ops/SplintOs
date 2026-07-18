#ifndef SPLINTOS_ARCH_X86_64_BLOCK_H
#define SPLINTOS_ARCH_X86_64_BLOCK_H

#include <stddef.h>
#include <stdint.h>

struct x86_64_block_device;
typedef int (*x86_64_block_transfer_fn)(struct x86_64_block_device *device,
                                        uint64_t first_block, size_t block_count,
                                        void *buffer);

struct x86_64_block_device {
    uint64_t block_count;
    uint32_t block_size;
    uint32_t flags;
    void *context;
    x86_64_block_transfer_fn read;
    x86_64_block_transfer_fn write;
};

int x86_64_block_read(struct x86_64_block_device *device, uint64_t first_block,
                      size_t block_count, void *buffer);
int x86_64_block_write(struct x86_64_block_device *device, uint64_t first_block,
                       size_t block_count, const void *buffer);
int x86_64_block_conformance_test(void);

#endif
