#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/block.h"

enum { BLOCK_WRITABLE = 1U, TEST_BLOCK_SIZE = 512, TEST_BLOCKS = 16 };
static uint8_t test_storage[TEST_BLOCK_SIZE * TEST_BLOCKS];

static int request_valid(const struct x86_64_block_device *device,
                         uint64_t first_block, size_t block_count,
                         const void *buffer)
{
    if (device == NULL || buffer == NULL || block_count == 0 ||
        device->block_size == 0 || block_count > UINT64_MAX) return 0;
    uint64_t count = (uint64_t)block_count;
    return first_block < device->block_count &&
           count <= device->block_count - first_block &&
           count <= SIZE_MAX / device->block_size;
}

int x86_64_block_read(struct x86_64_block_device *device, uint64_t first_block,
                      size_t block_count, void *buffer)
{
    if (!request_valid(device, first_block, block_count, buffer) ||
        device->read == NULL) return -1;
    return device->read(device, first_block, block_count, buffer);
}

int x86_64_block_write(struct x86_64_block_device *device, uint64_t first_block,
                       size_t block_count, const void *buffer)
{
    if (!request_valid(device, first_block, block_count, buffer) ||
        device->write == NULL || (device->flags & BLOCK_WRITABLE) == 0) return -1;
    return device->write(device, first_block, block_count, (void *)buffer);
}

static int ram_transfer(struct x86_64_block_device *device, uint64_t first_block,
                        size_t block_count, void *buffer, int write)
{
    uint8_t *bytes = buffer;
    uint8_t *storage = device->context;
    size_t offset = (size_t)first_block * device->block_size;
    size_t length = block_count * device->block_size;
    for (size_t i = 0; i < length; ++i) {
        if (write) storage[offset + i] = bytes[i];
        else bytes[i] = storage[offset + i];
    }
    return 0;
}

static int ram_read(struct x86_64_block_device *device, uint64_t first_block,
                    size_t block_count, void *buffer)
{ return ram_transfer(device, first_block, block_count, buffer, 0); }
static int ram_write(struct x86_64_block_device *device, uint64_t first_block,
                     size_t block_count, void *buffer)
{ return ram_transfer(device, first_block, block_count, buffer, 1); }

int x86_64_block_conformance_test(void)
{
    struct x86_64_block_device device = {
        TEST_BLOCKS, TEST_BLOCK_SIZE, BLOCK_WRITABLE, test_storage,
        ram_read, ram_write
    };
    uint8_t output[TEST_BLOCK_SIZE], input[TEST_BLOCK_SIZE];
    for (size_t i = 0; i < sizeof(input); ++i) input[i] = (uint8_t)(i ^ 0xa5U);
    if (x86_64_block_write(&device, 3, 1, input) != 0 ||
        x86_64_block_read(&device, 3, 1, output) != 0) return 0;
    for (size_t i = 0; i < sizeof(input); ++i)
        if (input[i] != output[i]) return 0;
    if (x86_64_block_read(&device, TEST_BLOCKS, 1, output) == 0 ||
        x86_64_block_read(&device, TEST_BLOCKS - 1U, 2, output) == 0 ||
        x86_64_block_read(&device, UINT64_MAX, 1, output) == 0) return 0;
    device.flags = 0;
    return x86_64_block_write(&device, 0, 1, input) != 0;
}
