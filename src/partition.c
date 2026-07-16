#include "partition.h"

#include "devices.h"

#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

enum { MAX_PARTITIONS = 16, MBR_ENTRY_COUNT = 4 };

struct PACKED mbr_entry {
    uint8_t status, first_chs[3], type, last_chs[3];
    uint32_t first_sector, sector_count;
};

static struct partition partitions[MAX_PARTITIONS];
static size_t found_partitions;

void partition_init(void)
{
    found_partitions = 0;
    for (size_t device_index = 0; device_index < block_device_count(); ++device_index) {
        struct block_device *device = block_device_get(device_index);
        if (device->sector_size != 512) continue;
        uint8_t sector[512];
        if (block_cached_read(device, 0, sector) != 0 ||
            sector[510] != 0x55 || sector[511] != 0xAA) continue;
        const struct mbr_entry *entries = (const struct mbr_entry *)(sector + 446);
        for (size_t i = 0; i < MBR_ENTRY_COUNT && found_partitions < MAX_PARTITIONS; ++i) {
            uint64_t first = entries[i].first_sector;
            uint64_t count = entries[i].sector_count;
            if (entries[i].type == 0 || count == 0 || first >= device->sector_count ||
                count > device->sector_count - first) continue;
            partitions[found_partitions++] =
                (struct partition){device, first, count, entries[i].type};
        }
    }
    serial_write(found_partitions != 0
        ? "SplintOS: validated MBR partition discovered\r\n"
        : "SplintOS: no valid MBR partitions found\r\n");
}

size_t partition_count(void) { return found_partitions; }
const struct partition *partition_get(size_t index)
{ return index < found_partitions ? &partitions[index] : NULL; }

int partition_read(const struct partition *partition, uint64_t sector, void *buffer)
{
    if (partition == NULL || sector >= partition->sector_count) return -1;
    return block_cached_read(partition->device, partition->first_sector + sector, buffer);
}

int partition_write(const struct partition *partition, uint64_t sector,
                    const void *buffer)
{
    if (partition == NULL || sector >= partition->sector_count) return -1;
    return block_cached_write(partition->device, partition->first_sector + sector, buffer);
}

int partition_flush(const struct partition *partition)
{ return partition == NULL ? -1 : block_cache_flush(partition->device); }
