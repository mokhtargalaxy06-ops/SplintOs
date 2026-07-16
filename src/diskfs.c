#include "diskfs.h"

#include "devices.h"
#include "partition.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

enum {
    SECTOR_SIZE = 512,
    FILE_COUNT = 8,
    NAME_SIZE = 32,
    SECTORS_PER_FILE = 8,
    DIRECTORY_SECTOR = 1,
    BITMAP_SECTOR = 2,
    DATA_START = 3,
    FEATURE_ALLOCATION_BITMAP = 1,
    STATE_CLEAN = 0,
    STATE_DIRTY = 1,
};

struct PACKED superblock {
    char magic[8];
    uint32_t version, entry_count, sectors_per_file;
    uint32_t directory_sector, bitmap_sector, data_start;
    uint32_t feature_flags, state, directory_checksum, bitmap_checksum;
    uint8_t reserved[SECTOR_SIZE - 48];
};

struct PACKED directory_entry {
    char name[NAME_SIZE];
    uint32_t size, first_sector, sector_count;
};

struct PACKED directory_sector {
    struct directory_entry entries[FILE_COUNT];
    uint8_t reserved[SECTOR_SIZE - FILE_COUNT * sizeof(struct directory_entry)];
};

static const struct partition *mounted_partition;
static struct directory_sector directory;
static uint8_t allocation_bitmap[SECTOR_SIZE];
static struct superblock mounted_superblock;
static bool formatted_on_mount;
static bool mounted;
static bool mounted_read_only;
static uint8_t full_disk_payload[DISKFS_FILE_SIZE];

static bool bytes_equal(const void *left, const void *right, size_t count)
{
    const uint8_t *a = left, *b = right;
    while (count-- != 0) if (*a++ != *b++) return false;
    return true;
}

static uint32_t checksum32(const void *data, size_t count)
{
    const uint8_t *bytes = data;
    uint32_t hash = 2166136261U;
    while (count-- != 0) { hash ^= *bytes++; hash *= 16777619U; }
    return hash;
}

static bool sector_allocated(uint32_t sector)
{
    return (allocation_bitmap[sector / 8] & (uint8_t)(1U << (sector % 8))) != 0;
}

static void set_sector_allocated(uint32_t sector, bool allocated)
{
    uint8_t mask = (uint8_t)(1U << (sector % 8));
    if (allocated) allocation_bitmap[sector / 8] |= mask;
    else allocation_bitmap[sector / 8] &= (uint8_t)~mask;
}

static int commit_metadata(void)
{
    mounted_superblock.state = STATE_DIRTY;
    if (partition_write(mounted_partition, 0, &mounted_superblock) != 0 ||
        partition_flush(mounted_partition) != 0 ||
        partition_write(mounted_partition, BITMAP_SECTOR, allocation_bitmap) != 0 ||
        partition_write(mounted_partition, DIRECTORY_SECTOR, &directory) != 0 ||
        partition_flush(mounted_partition) != 0) {
        mounted = false; return -1;
    }
    mounted_superblock.directory_checksum = checksum32(&directory, sizeof(directory));
    mounted_superblock.bitmap_checksum = checksum32(allocation_bitmap,
                                                     sizeof(allocation_bitmap));
    mounted_superblock.state = STATE_CLEAN;
    if (partition_write(mounted_partition, 0, &mounted_superblock) != 0 ||
        partition_flush(mounted_partition) != 0) {
        mounted = false; return -1;
    }
    return 0;
}

static bool text_equal(const char *left, const char *right)
{
    size_t i = 0;
    while (left[i] != '\0' && left[i] == right[i]) ++i;
    return left[i] == right[i];
}

static bool name_equal(const char stored[NAME_SIZE], const char *name)
{
    size_t i = 0;
    while (i < NAME_SIZE && stored[i] != '\0' && stored[i] == name[i]) ++i;
    return i < NAME_SIZE && stored[i] == '\0' && name[i] == '\0';
}

static uint32_t sectors_for_size(size_t size)
{ return size == 0 ? 0 : (uint32_t)((size + SECTOR_SIZE - 1) / SECTOR_SIZE); }

static bool ranges_overlap(uint32_t first_a, uint32_t count_a,
                           uint32_t first_b, uint32_t count_b)
{
    return count_a != 0 && count_b != 0 && first_a < first_b + count_b &&
           first_b < first_a + count_a;
}

static bool directory_valid(uint8_t expected_bitmap[SECTOR_SIZE])
{
    for (size_t i = 0; i < SECTOR_SIZE; ++i) expected_bitmap[i] = 0;
    for (uint32_t sector = 0; sector < DATA_START; ++sector)
        expected_bitmap[sector / 8] |= (uint8_t)(1U << (sector % 8));
    for (size_t i = 0; i < FILE_COUNT; ++i) {
        const struct directory_entry *entry = &directory.entries[i];
        if (entry->name[0] == '\0') {
            if (entry->size != 0 || entry->first_sector != 0 || entry->sector_count != 0)
                return false;
            continue;
        }
        size_t length = 0;
        while (length < NAME_SIZE && entry->name[length] != '\0') ++length;
        uint32_t expected = sectors_for_size(entry->size);
        if (length == NAME_SIZE || entry->size > DISKFS_FILE_SIZE ||
            entry->sector_count != expected ||
            (expected != 0 && entry->first_sector < DATA_START) ||
            entry->first_sector > mounted_partition->sector_count ||
            entry->sector_count > mounted_partition->sector_count - entry->first_sector)
            return false;
        for (uint32_t sector = 0; sector < entry->sector_count; ++sector) {
            uint32_t absolute = entry->first_sector + sector;
            expected_bitmap[absolute / 8] |= (uint8_t)(1U << (absolute % 8));
        }
        for (size_t j = i + 1; j < FILE_COUNT; ++j) {
            if (directory.entries[j].name[0] == '\0') continue;
            if (name_equal(directory.entries[j].name, entry->name) ||
                ranges_overlap(entry->first_sector, entry->sector_count,
                    directory.entries[j].first_sector, directory.entries[j].sector_count))
                return false;
        }
    }
    for (uint32_t sector = mounted_partition->sector_count;
         sector < sizeof(allocation_bitmap) * 8U; ++sector)
        expected_bitmap[sector / 8] |= (uint8_t)(1U << (sector % 8));
    return true;
}

static int format_partition(void)
{
    mounted_read_only = false;
    mounted_superblock = (struct superblock){
        {'S','P','L','F','S','4','\0','\0'}, 4, FILE_COUNT,
        SECTORS_PER_FILE, DIRECTORY_SECTOR, BITMAP_SECTOR, DATA_START,
        FEATURE_ALLOCATION_BITMAP, STATE_CLEAN, 0, 0, {0}};
    directory = (struct directory_sector){0};
    for (size_t i = 0; i < sizeof(allocation_bitmap); ++i) allocation_bitmap[i] = 0;
    for (uint32_t sector = 0; sector < DATA_START; ++sector)
        set_sector_allocated(sector, true);
    for (uint32_t sector = mounted_partition->sector_count;
         sector < sizeof(allocation_bitmap) * 8U; ++sector)
        set_sector_allocated(sector, true);
    mounted_superblock.directory_checksum = checksum32(&directory, sizeof(directory));
    mounted_superblock.bitmap_checksum = checksum32(allocation_bitmap,
                                                     sizeof(allocation_bitmap));
    return partition_write(mounted_partition, DIRECTORY_SECTOR, &directory) == 0 &&
           partition_write(mounted_partition, BITMAP_SECTOR, allocation_bitmap) == 0 &&
           partition_flush(mounted_partition) == 0 &&
           partition_write(mounted_partition, 0, &mounted_superblock) == 0 &&
           partition_flush(mounted_partition) == 0 ? 0 : -1;
}

static int mount_partition(void)
{
    mounted_read_only = false;
    if (partition_read(mounted_partition, 0, &mounted_superblock) != 0) return -1;
    static const char magic[8] = {'S','P','L','F','S','4','\0','\0'};
    if (!bytes_equal(mounted_superblock.magic, magic, sizeof(magic))) {
        if (!text_equal(mounted_partition->device->name, "ramblk0")) return -1;
        formatted_on_mount = true;
        if (format_partition() != 0 ||
            partition_read(mounted_partition, 0, &mounted_superblock) != 0)
            return -1;
    }
    if (mounted_superblock.version != 4 || mounted_superblock.entry_count != FILE_COUNT ||
        mounted_superblock.sectors_per_file != SECTORS_PER_FILE ||
        mounted_superblock.directory_sector != DIRECTORY_SECTOR ||
        mounted_superblock.bitmap_sector != BITMAP_SECTOR ||
        mounted_superblock.data_start != DATA_START ||
        mounted_superblock.feature_flags != FEATURE_ALLOCATION_BITMAP ||
        (mounted_superblock.state != STATE_CLEAN &&
         mounted_superblock.state != STATE_DIRTY) ||
        mounted_partition->sector_count <= DATA_START ||
        mounted_partition->sector_count > sizeof(allocation_bitmap) * 8U)
        return -1;
    if (partition_read(mounted_partition, DIRECTORY_SECTOR, &directory) != 0 ||
        partition_read(mounted_partition, BITMAP_SECTOR, allocation_bitmap) != 0)
        return -1;
    if (mounted_superblock.directory_checksum != checksum32(&directory, sizeof(directory))) {
        serial_write("SplintOS: diskfs directory checksum mismatch\r\n"); return -1;
    }
    uint8_t expected_bitmap[SECTOR_SIZE];
    if (!directory_valid(expected_bitmap)) {
        serial_write("SplintOS: diskfs directory structure invalid\r\n"); return -1;
    }
    if (mounted_superblock.state == STATE_DIRTY) {
        for (size_t i = 0; i < sizeof(allocation_bitmap); ++i)
            allocation_bitmap[i] = expected_bitmap[i];
        mounted_read_only = true;
        return 0;
    }
    if (mounted_superblock.bitmap_checksum != checksum32(allocation_bitmap,
                                                          sizeof(allocation_bitmap))) {
        serial_write("SplintOS: diskfs bitmap checksum mismatch\r\n"); return -1;
    }
    if (!bytes_equal(expected_bitmap, allocation_bitmap, sizeof(allocation_bitmap))) {
        serial_write("SplintOS: diskfs allocation map mismatch\r\n"); return -1;
    }
    return 0;
}

int diskfs_write_file(const char *name, const void *data, size_t size)
{
    if (!mounted || mounted_read_only || mounted_partition == NULL ||
        name == NULL || data == NULL ||
        size > DISKFS_FILE_SIZE) return -1;
    size_t name_length = 0;
    while (name[name_length] != '\0') if (++name_length >= NAME_SIZE) return -1;
    size_t slot = FILE_COUNT;
    for (size_t i = 0; i < FILE_COUNT; ++i) {
        if (directory.entries[i].name[0] == '\0' && slot == FILE_COUNT) slot = i;
        if (name_equal(directory.entries[i].name, name)) { slot = i; break; }
    }
    if (slot == FILE_COUNT) return -1;
    uint32_t needed = sectors_for_size(size);
    uint32_t first = 0;
    if (needed != 0) {
        for (uint32_t candidate = DATA_START;
             candidate <= mounted_partition->sector_count - needed; ++candidate) {
            bool free = true;
            for (uint32_t sector = 0; sector < needed; ++sector)
                if (sector_allocated(candidate + sector) &&
                    !(directory.entries[slot].name[0] != '\0' &&
                      candidate + sector >= directory.entries[slot].first_sector &&
                      candidate + sector < directory.entries[slot].first_sector +
                                           directory.entries[slot].sector_count)) {
                    free = false; break;
                }
            if (free) { first = candidate; break; }
        }
        if (first == 0) return -1;
    }
    uint8_t sector[SECTOR_SIZE];
    for (size_t block = 0; block < needed; ++block) {
        for (size_t i = 0; i < SECTOR_SIZE; ++i) {
            size_t offset = block * SECTOR_SIZE + i;
            sector[i] = offset < size ? ((const uint8_t *)data)[offset] : 0;
        }
        if (partition_write(mounted_partition, first + block, sector) != 0)
            return -1;
    }
    struct directory_entry previous = directory.entries[slot];
    for (uint32_t sector = 0; sector < previous.sector_count; ++sector)
        set_sector_allocated(previous.first_sector + sector, false);
    for (uint32_t sector = 0; sector < needed; ++sector)
        set_sector_allocated(first + sector, true);
    directory.entries[slot] = (struct directory_entry){0};
    for (size_t i = 0; i <= name_length; ++i) directory.entries[slot].name[i] = name[i];
    directory.entries[slot].size = size;
    directory.entries[slot].first_sector = first;
    directory.entries[slot].sector_count = needed;
    return commit_metadata();
}

int diskfs_read_file(const char *name, void *data, size_t capacity)
{
    if (!mounted || mounted_partition == NULL || name == NULL || data == NULL) return -1;
    for (size_t i = 0; i < FILE_COUNT; ++i) {
        if (!name_equal(directory.entries[i].name, name)) continue;
        size_t size = directory.entries[i].size;
        if (size > capacity) return -1;
        uint8_t sector[SECTOR_SIZE];
        size_t copied = 0;
        for (size_t block = 0; copied < size; ++block) {
            if (partition_read(mounted_partition,
                               directory.entries[i].first_sector + block, sector) != 0)
                return -1;
            size_t count = size - copied < SECTOR_SIZE ? size - copied : SECTOR_SIZE;
            for (size_t j = 0; j < count; ++j) ((uint8_t *)data)[copied + j] = sector[j];
            copied += count;
        }
        return (int)size;
    }
    return -1;
}

int diskfs_flush(void)
{ return !mounted || mounted_read_only || mounted_partition == NULL
    ? -1 : partition_flush(mounted_partition); }

int diskfs_unmount(void)
{
    if (!mounted || mounted_partition == NULL) return -1;
    if (!mounted_read_only && partition_flush(mounted_partition) != 0) return -1;
    mounted = false;
    return 0;
}

int diskfs_list(struct diskfs_entry *entries, size_t capacity)
{
    if (!mounted || mounted_partition == NULL || (capacity != 0 && entries == NULL)) return -1;
    size_t count = 0;
    for (size_t i = 0; i < FILE_COUNT && count < capacity; ++i) {
        if (directory.entries[i].name[0] == '\0') continue;
        size_t j = 0;
        while (j + 1 < DISKFS_NAME_SIZE && directory.entries[i].name[j] != '\0') {
            entries[count].name[j] = directory.entries[i].name[j]; ++j;
        }
        entries[count].name[j] = '\0';
        entries[count].size = directory.entries[i].size;
        ++count;
    }
    return (int)count;
}

int diskfs_unlink(const char *name)
{
    if (!mounted || mounted_read_only || name == NULL) return -1;
    for (size_t i = 0; i < FILE_COUNT; ++i) {
        if (!name_equal(directory.entries[i].name, name)) continue;
        struct directory_entry previous = directory.entries[i];
        for (uint32_t sector = 0; sector < previous.sector_count; ++sector)
            set_sector_allocated(previous.first_sector + sector, false);
        directory.entries[i] = (struct directory_entry){0};
        if (commit_metadata() != 0) {
            directory.entries[i] = previous;
            for (uint32_t sector = 0; sector < previous.sector_count; ++sector)
                set_sector_allocated(previous.first_sector + sector, true);
            return -1;
        }
        return 0;
    }
    return -1;
}

int diskfs_rename(const char *old_name, const char *new_name)
{
    if (!mounted || mounted_read_only || old_name == NULL || new_name == NULL) return -1;
    size_t new_length = 0;
    while (new_name[new_length] != '\0') if (++new_length >= NAME_SIZE) return -1;
    size_t source = FILE_COUNT, target = FILE_COUNT;
    for (size_t i = 0; i < FILE_COUNT; ++i) {
        if (name_equal(directory.entries[i].name, new_name)) target = i;
        if (name_equal(directory.entries[i].name, old_name)) source = i;
    }
    if (source == FILE_COUNT || source == target) return -1;
    struct directory_entry previous_source = directory.entries[source];
    struct directory_entry previous_target = {0};
    if (target != FILE_COUNT) {
        previous_target = directory.entries[target];
        for (uint32_t sector = 0; sector < previous_target.sector_count; ++sector)
            set_sector_allocated(previous_target.first_sector + sector, false);
        directory.entries[target] = (struct directory_entry){0};
    }
    for (size_t i = 0; i < NAME_SIZE; ++i) directory.entries[source].name[i] = 0;
    for (size_t i = 0; i <= new_length; ++i)
        directory.entries[source].name[i] = new_name[i];
    if (commit_metadata() != 0) {
        directory.entries[source] = previous_source;
        if (target != FILE_COUNT) {
            directory.entries[target] = previous_target;
            for (uint32_t sector = 0; sector < previous_target.sector_count; ++sector)
                set_sector_allocated(previous_target.first_sector + sector, true);
        }
        return -1;
    }
    return 0;
}

static bool metadata_failure_conformance(void)
{
    static const char payload[] = "fault";
    bool rejected_partial_commit = false;
    for (size_t writes = 0; writes < 6 && !rejected_partial_commit; ++writes) {
        mounted = true;
        if (format_partition() != 0) return false;
        block_test_fail_writes_after(mounted_partition->device, writes);
        int result = diskfs_write_file("fault.txt", payload, sizeof(payload));
        block_test_clear_write_failure();
        mounted = true;
        if (result != 0 && mount_partition() != 0)
            rejected_partial_commit = true;
    }
    mounted = true;
    return rejected_partial_commit && format_partition() == 0 &&
           mount_partition() == 0;
}

static bool full_disk_conformance(void)
{
    const struct partition *original = mounted_partition;
    struct partition constrained = *original;
    constrained.sector_count = DATA_START + 7U * SECTORS_PER_FILE + 7U;
    mounted_partition = &constrained;
    mounted = true;
    for (size_t i = 0; i < sizeof(full_disk_payload); ++i)
        full_disk_payload[i] = (uint8_t)(i * 13U + 7U);
    static const char *const names[FILE_COUNT] = {
        "full0", "full1", "full2", "full3", "full4", "full5", "full6", "reused"
    };
    bool passed = format_partition() == 0;
    for (size_t i = 0; i < FILE_COUNT - 1 && passed; ++i)
        passed = diskfs_write_file(names[i], full_disk_payload,
                                   sizeof(full_disk_payload)) == 0;
    if (passed)
        passed = diskfs_write_file(names[FILE_COUNT - 1], full_disk_payload,
                                   sizeof(full_disk_payload)) != 0;
    if (passed) passed = diskfs_unlink(names[0]) == 0;
    if (passed)
        passed = diskfs_write_file(names[FILE_COUNT - 1], full_disk_payload,
                                   sizeof(full_disk_payload)) == 0;
    mounted_partition = original;
    mounted = true;
    return format_partition() == 0 && mount_partition() == 0 && passed;
}

void diskfs_init(void)
{
    formatted_on_mount = false;
    mounted = false;
    mounted_read_only = false;
    mounted_partition = partition_get(0);
    for (size_t i = 0; i < partition_count(); ++i) {
        const struct partition *candidate = partition_get(i);
        if (candidate != NULL && text_equal(candidate->device->name, "virtblk0")) {
            mounted_partition = candidate; break;
        }
    }
    if (mounted_partition == NULL || mount_partition() != 0) {
        serial_write("SplintOS: diskfs unavailable\r\n"); return;
    }
    mounted = true;
    if (mounted_read_only) {
        serial_write("SplintOS: dirty diskfs mounted read-only on virtblk0\r\n");
        return;
    }
    if (text_equal(mounted_partition->device->name, "ramblk0")) {
        if (!metadata_failure_conformance()) {
            mounted = false;
            serial_write("SplintOS: diskfs metadata fault test failed\r\n"); return;
        }
        serial_write("SplintOS: diskfs interrupted commit rejection online\r\n");
        if (!full_disk_conformance()) {
            mounted = false;
            serial_write("SplintOS: diskfs full-disk test failed\r\n"); return;
        }
        serial_write("SplintOS: diskfs full-disk reclamation online\r\n");
    }
    static const char message[] = "diskfs multi-sector remount data";
    int conformance_write = diskfs_write_file("conformance.txt", message, sizeof(message));
    int conformance_flush = conformance_write == 0 ? diskfs_flush() : -1;
    int conformance_unmount = conformance_flush == 0 ? diskfs_unmount() : -1;
    char inaccessible;
    int conformance_offline = conformance_unmount == 0
        ? diskfs_read_file("conformance.txt", &inaccessible, 1) : 0;
    int conformance_remount = conformance_unmount == 0 && conformance_offline == -1
        ? mount_partition() : -1;
    if (conformance_remount == 0) mounted = true;
    if (conformance_write != 0 || conformance_flush != 0 || conformance_remount != 0) {
        if (conformance_write != 0)
            serial_write("SplintOS: diskfs conformance data commit failed\r\n");
        else if (conformance_flush != 0)
            serial_write("SplintOS: diskfs conformance flush failed\r\n");
        else if (conformance_unmount != 0 || conformance_offline != -1)
            serial_write("SplintOS: diskfs clean unmount validation failed\r\n");
        else
            serial_write("SplintOS: diskfs conformance remount validation failed\r\n");
        serial_write("SplintOS: diskfs conformance write failed\r\n"); return;
    }
    char result[sizeof(message)];
    int count = diskfs_read_file("conformance.txt", result, sizeof(result));
    if (count != (int)sizeof(message) || !bytes_equal(result, message, sizeof(message))) {
        serial_write("SplintOS: diskfs remount mismatch\r\n"); return;
    }
    if (text_equal(mounted_partition->device->name, "virtblk0"))
        serial_write("SplintOS: existing diskfs mounted on virtblk0\r\n");
    else serial_write("SplintOS: diskfs format, flush and remount online\r\n");
}
