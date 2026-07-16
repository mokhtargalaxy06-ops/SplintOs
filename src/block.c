#include "block.h"

#include "devices.h"
#include "virtio_block.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MAX_BLOCK_DEVICES = 8,
    RAM_SECTOR_SIZE = 512,
    RAM_SECTOR_COUNT = 256,
    CACHE_ENTRY_COUNT = 16,
    CACHE_SECTOR_SIZE = 512,
};

struct cache_entry {
    bool used, dirty;
    struct block_device *device;
    uint64_t sector;
    uint32_t age;
    uint8_t data[CACHE_SECTOR_SIZE];
};

static struct block_device *devices[MAX_BLOCK_DEVICES];
static size_t device_count;
static uint8_t ram_storage[RAM_SECTOR_SIZE * RAM_SECTOR_COUNT];
static struct cache_entry cache[CACHE_ENTRY_COUNT];
static uint32_t cache_age;
static struct block_device *write_failure_device;
static size_t writes_before_failure;

static bool request_valid(const struct block_device *device, uint64_t sector,
                          size_t count)
{
    return device != NULL && count != 0 && sector < device->sector_count &&
        count <= device->sector_count - sector;
}

int block_register(struct block_device *device)
{
    if (device == NULL || device->name == NULL || device->sector_size == 0 ||
        device->sector_count == 0 || device->operations == NULL ||
        device->operations->read == NULL || device_count == MAX_BLOCK_DEVICES)
        return -1;
    devices[device_count++] = device;
    return 0;
}

int block_read(struct block_device *device, uint64_t sector,
               void *buffer, size_t count)
{
    if (buffer == NULL || !request_valid(device, sector, count)) return -1;
    return device->operations->read(device, sector, buffer, count);
}

int block_write(struct block_device *device, uint64_t sector,
                const void *buffer, size_t count)
{
    if (buffer == NULL || !request_valid(device, sector, count) ||
        device->read_only || device->operations->write == NULL) return -1;
    if (device == write_failure_device) {
        if (writes_before_failure == 0) return -1;
        --writes_before_failure;
    }
    return device->operations->write(device, sector, buffer, count);
}

void block_test_fail_writes_after(struct block_device *device, size_t successful_writes)
{
    write_failure_device = device;
    writes_before_failure = successful_writes;
}

void block_test_clear_write_failure(void)
{
    write_failure_device = NULL;
    writes_before_failure = 0;
}

int block_flush(struct block_device *device)
{
    if (device == NULL) return -1;
    return device->operations->flush == NULL ? 0 : device->operations->flush(device);
}

static int cache_writeback(struct cache_entry *entry)
{
    if (!entry->used || !entry->dirty) return 0;
    if (block_write(entry->device, entry->sector, entry->data, 1) != 0) return -1;
    entry->dirty = false;
    return 0;
}

static struct cache_entry *cache_get(struct block_device *device, uint64_t sector,
                                     bool load)
{
    struct cache_entry *victim = &cache[0];
    for (size_t i = 0; i < CACHE_ENTRY_COUNT; ++i) {
        if (cache[i].used && cache[i].device == device && cache[i].sector == sector) {
            cache[i].age = ++cache_age;
            return &cache[i];
        }
        if (!cache[i].used || (victim->used && cache[i].age < victim->age))
            victim = &cache[i];
    }
    if (cache_writeback(victim) != 0) return NULL;
    *victim = (struct cache_entry){0};
    victim->used = true;
    victim->device = device;
    victim->sector = sector;
    victim->age = ++cache_age;
    if (load && block_read(device, sector, victim->data, 1) != 0) {
        victim->used = false;
        return NULL;
    }
    return victim;
}

void block_cache_init(void)
{
    for (size_t i = 0; i < CACHE_ENTRY_COUNT; ++i) cache[i] = (struct cache_entry){0};
    cache_age = 0;
}

int block_cached_read(struct block_device *device, uint64_t sector, void *buffer)
{
    if (buffer == NULL || device == NULL || device->sector_size != CACHE_SECTOR_SIZE ||
        sector >= device->sector_count) return -1;
    struct cache_entry *entry = cache_get(device, sector, true);
    if (entry == NULL) return -1;
    for (size_t i = 0; i < CACHE_SECTOR_SIZE; ++i) ((uint8_t *)buffer)[i] = entry->data[i];
    return 0;
}

int block_cached_write(struct block_device *device, uint64_t sector,
                       const void *buffer)
{
    if (buffer == NULL || device == NULL || device->read_only ||
        device->sector_size != CACHE_SECTOR_SIZE || sector >= device->sector_count)
        return -1;
    struct cache_entry *entry = cache_get(device, sector, false);
    if (entry == NULL) return -1;
    for (size_t i = 0; i < CACHE_SECTOR_SIZE; ++i) entry->data[i] = ((const uint8_t *)buffer)[i];
    entry->dirty = true;
    return 0;
}

int block_cache_flush(struct block_device *device)
{
    for (size_t i = 0; i < CACHE_ENTRY_COUNT; ++i)
        if (cache[i].used && cache[i].device == device && cache_writeback(&cache[i]) != 0)
            return -1;
    return block_flush(device);
}

size_t block_device_count(void) { return device_count; }
struct block_device *block_device_get(size_t index)
{ return index < device_count ? devices[index] : NULL; }

static int ram_transfer(uint64_t sector, void *buffer, size_t count, bool write)
{
    size_t offset = (size_t)sector * RAM_SECTOR_SIZE;
    size_t length = count * RAM_SECTOR_SIZE;
    uint8_t *storage = ram_storage + offset;
    uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) {
        if (write) storage[i] = bytes[i];
        else bytes[i] = storage[i];
    }
    return 0;
}

static int ram_read(struct block_device *device, uint64_t sector,
                    void *buffer, size_t count)
{
    (void)device;
    return ram_transfer(sector, buffer, count, false);
}

static int ram_write(struct block_device *device, uint64_t sector,
                     const void *buffer, size_t count)
{
    (void)device;
    return ram_transfer(sector, (void *)buffer, count, true);
}

static int ram_flush(struct block_device *device) { (void)device; return 0; }

static const struct block_operations ram_operations = {
    ram_read, ram_write, ram_flush
};
static struct block_device ram_device = {
    "ramblk0", RAM_SECTOR_SIZE, RAM_SECTOR_COUNT, false,
    &ram_operations, NULL
};

void block_init(void)
{
    device_count = 0;
    block_test_clear_write_failure();
    block_cache_init();
    if (block_register(&ram_device) != 0) {
        serial_write("SplintOS: block subsystem failed\r\n");
        return;
    }
    uint8_t write_buffer[RAM_SECTOR_SIZE], read_buffer[RAM_SECTOR_SIZE];
    for (size_t i = 0; i < RAM_SECTOR_SIZE; ++i)
        write_buffer[i] = (uint8_t)(i ^ 0xA5U);
    if (block_write(&ram_device, 0, write_buffer, 1) != 0 ||
        block_flush(&ram_device) != 0 ||
        block_read(&ram_device, 0, read_buffer, 1) != 0) {
        serial_write("SplintOS: block conformance I/O failed\r\n");
        return;
    }
    for (size_t i = 0; i < RAM_SECTOR_SIZE; ++i) {
        if (read_buffer[i] != write_buffer[i]) {
            serial_write("SplintOS: block conformance mismatch\r\n");
            return;
        }
    }
    for (size_t i = 0; i < RAM_SECTOR_SIZE; ++i) write_buffer[i] = 0;
    write_buffer[446 + 4] = 0x83;
    write_buffer[446 + 8] = 8;
    write_buffer[446 + 12] = 128;
    write_buffer[510] = 0x55; write_buffer[511] = 0xAA;
    if (block_cached_write(&ram_device, 0, write_buffer) != 0 ||
        block_cache_flush(&ram_device) != 0)
        serial_write("SplintOS: block cache conformance failed\r\n");
    else {
        serial_write("SplintOS: generic block layer, cache and ramblk0 online\r\n");
        write_buffer[0] = 0x5A;
        if (block_cached_write(&ram_device, 1, write_buffer) != 0) {
            serial_write("SplintOS: block fault test setup failed\r\n");
        } else {
            block_test_fail_writes_after(&ram_device, 0);
            int injected_result = block_cache_flush(&ram_device);
            block_test_clear_write_failure();
            if (injected_result == 0 || block_cache_flush(&ram_device) != 0)
                serial_write("SplintOS: block fault propagation failed\r\n");
            else
                serial_write("SplintOS: deterministic block write faults online\r\n");
        }
    }
    virtio_block_init();
}
