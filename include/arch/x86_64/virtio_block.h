#ifndef SPLINTOS_ARCH_X86_64_VIRTIO_BLOCK_H
#define SPLINTOS_ARCH_X86_64_VIRTIO_BLOCK_H
#include <stddef.h>
#include <stdint.h>
int x86_64_virtio_block_init(void);
int x86_64_virtio_block_conformance_test(void);
int x86_64_virtio_block_read(uint64_t sector, void *buffer, size_t sectors);
int x86_64_virtio_block_write(uint64_t sector, const void *buffer, size_t sectors);
int x86_64_virtio_block_flush(void);
#endif
