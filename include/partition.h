#ifndef SPLINTOS_PARTITION_H
#define SPLINTOS_PARTITION_H

#include "block.h"

#include <stddef.h>
#include <stdint.h>

struct partition {
    struct block_device *device;
    uint64_t first_sector;
    uint64_t sector_count;
    uint8_t type;
};

void partition_init(void);
size_t partition_count(void);
const struct partition *partition_get(size_t index);
int partition_read(const struct partition *partition, uint64_t sector, void *buffer);
int partition_write(const struct partition *partition, uint64_t sector,
                    const void *buffer);
int partition_flush(const struct partition *partition);

#endif
