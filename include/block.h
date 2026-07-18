#ifndef SPLINTOS_BLOCK_H
#define SPLINTOS_BLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"

struct block_device;

struct block_operations {
    int (*read)(struct block_device *device, uint64_t sector,
                void *buffer, size_t sector_count);
    int (*write)(struct block_device *device, uint64_t sector,
                 const void *buffer, size_t sector_count);
    int (*flush)(struct block_device *device);
};

struct block_device {
    const char *name;
    uint32_t sector_size;
    uint64_t sector_count;
    bool read_only;
    const struct block_operations *operations;
    void *driver_data;
};

void block_init(void);
int block_register(struct block_device *device);
size_t block_device_count(void);
struct block_device *block_device_get(size_t index);
int block_read(struct block_device *device, uint64_t sector,
               void *buffer, size_t sector_count);
int block_write(struct block_device *device, uint64_t sector,
                const void *buffer, size_t sector_count);
int block_flush(struct block_device *device);
void block_cache_init(void);
int block_cached_read(struct block_device *device, uint64_t sector, void *buffer);
int block_cached_write(struct block_device *device, uint64_t sector,
                       const void *buffer);
int block_cache_flush(struct block_device *device);
void block_test_fail_writes_after(struct block_device *device, size_t successful_writes);
void block_test_clear_write_failure(void);

#endif
