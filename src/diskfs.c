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
    DATA_START = 2,
};

struct PACKED superblock {
    char magic[8];
    uint32_t version, entry_count, sectors_per_file, data_start, directory_checksum;
    uint8_t reserved[SECTOR_SIZE - 28];
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
static struct superblock mounted_superblock;
static bool formatted_on_mount;
static bool mounted;

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

static int commit_directory(void)
{
    if (partition_write(mounted_partition, 1, &directory) != 0 ||
        partition_flush(mounted_partition) != 0) return -1;
    mounted_superblock.directory_checksum = checksum32(&directory, sizeof(directory));
    return partition_write(mounted_partition, 0, &mounted_superblock) == 0 &&
           partition_flush(mounted_partition) == 0 ? 0 : -1;
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

static bool directory_valid(void)
{
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
        for (size_t j = i + 1; j < FILE_COUNT; ++j) {
            if (directory.entries[j].name[0] == '\0') continue;
            if (name_equal(directory.entries[j].name, entry->name) ||
                ranges_overlap(entry->first_sector, entry->sector_count,
                    directory.entries[j].first_sector, directory.entries[j].sector_count))
                return false;
        }
    }
    return true;
}

static int format_partition(void)
{
    mounted_superblock = (struct superblock){
        {'S','P','L','F','S','3','\0','\0'}, 3, FILE_COUNT,
        SECTORS_PER_FILE, DATA_START, 0, {0}};
    directory = (struct directory_sector){0};
    mounted_superblock.directory_checksum = checksum32(&directory, sizeof(directory));
    return partition_write(mounted_partition, 1, &directory) == 0 &&
           partition_flush(mounted_partition) == 0 &&
           partition_write(mounted_partition, 0, &mounted_superblock) == 0 &&
           partition_flush(mounted_partition) == 0 ? 0 : -1;
}

static int mount_partition(void)
{
    if (partition_read(mounted_partition, 0, &mounted_superblock) != 0) return -1;
    static const char magic[8] = {'S','P','L','F','S','3','\0','\0'};
    if (!bytes_equal(mounted_superblock.magic, magic, sizeof(magic))) {
        if (!text_equal(mounted_partition->device->name, "ramblk0")) return -1;
        formatted_on_mount = true;
        if (format_partition() != 0 ||
            partition_read(mounted_partition, 0, &mounted_superblock) != 0)
            return -1;
    }
    if (mounted_superblock.version != 3 || mounted_superblock.entry_count != FILE_COUNT ||
        mounted_superblock.sectors_per_file != SECTORS_PER_FILE ||
        mounted_superblock.data_start != DATA_START ||
        mounted_partition->sector_count < DATA_START + FILE_COUNT * SECTORS_PER_FILE)
        return -1;
    return partition_read(mounted_partition, 1, &directory) == 0 &&
        mounted_superblock.directory_checksum == checksum32(&directory, sizeof(directory)) &&
        directory_valid() ? 0 : -1;
}

int diskfs_write_file(const char *name, const void *data, size_t size)
{
    if (!mounted || mounted_partition == NULL || name == NULL || data == NULL ||
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
            for (size_t i = 0; i < FILE_COUNT; ++i) {
                if (i == slot || directory.entries[i].name[0] == '\0') continue;
                if (ranges_overlap(candidate, needed, directory.entries[i].first_sector,
                                   directory.entries[i].sector_count)) { free = false; break; }
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
    directory.entries[slot] = (struct directory_entry){0};
    for (size_t i = 0; i <= name_length; ++i) directory.entries[slot].name[i] = name[i];
    directory.entries[slot].size = size;
    directory.entries[slot].first_sector = first;
    directory.entries[slot].sector_count = needed;
    return commit_directory();
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
{ return !mounted || mounted_partition == NULL ? -1 : partition_flush(mounted_partition); }

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
    if (!mounted || name == NULL) return -1;
    for (size_t i = 0; i < FILE_COUNT; ++i) {
        if (!name_equal(directory.entries[i].name, name)) continue;
        struct directory_entry previous = directory.entries[i];
        directory.entries[i] = (struct directory_entry){0};
        if (commit_directory() != 0) {
            directory.entries[i] = previous;
            return -1;
        }
        return 0;
    }
    return -1;
}

int diskfs_rename(const char *old_name, const char *new_name)
{
    if (!mounted || old_name == NULL || new_name == NULL) return -1;
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
        directory.entries[target] = (struct directory_entry){0};
    }
    for (size_t i = 0; i < NAME_SIZE; ++i) directory.entries[source].name[i] = 0;
    for (size_t i = 0; i <= new_length; ++i)
        directory.entries[source].name[i] = new_name[i];
    if (commit_directory() != 0) {
        directory.entries[source] = previous_source;
        if (target != FILE_COUNT) directory.entries[target] = previous_target;
        return -1;
    }
    return 0;
}

void diskfs_init(void)
{
    formatted_on_mount = false;
    mounted = false;
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
    static const char message[] = "diskfs multi-sector remount data";
    if (diskfs_write_file("conformance.txt", message, sizeof(message)) != 0 ||
        diskfs_flush() != 0 || mount_partition() != 0) {
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
