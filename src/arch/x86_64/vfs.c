#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/vfs.h"
#include "arch/x86_64/virtio_block.h"

enum { MAX_NODES = 24, NAME_SIZE = 32, FILE_CAPACITY = 1024, NO_NODE = 0xff };
struct node {
    uint64_t size;
    uint32_t mode;
    uint8_t used, type, parent;
    char name[NAME_SIZE];
    uint8_t data[FILE_CAPACITY];
};
static struct node nodes[MAX_NODES];

#define PACKED __attribute__((packed))
struct PACKED persistent_record {
    uint32_t magic, version, length, checksum;
    uint8_t data[496];
};
_Static_assert(sizeof(struct persistent_record) == 512, "persistent VFS record size");

static int equal(const char *name, const char *part, size_t length)
{
    size_t i = 0; while (i < length && name[i] == part[i]) ++i;
    return i == length && name[i] == '\0';
}
static uint8_t child(uint8_t parent, const char *name, size_t length)
{
    for (uint8_t i = 1; i < MAX_NODES; ++i)
        if (nodes[i].used && nodes[i].parent == parent &&
            equal(nodes[i].name, name, length)) return i;
    return NO_NODE;
}
static uint8_t resolve(const char *path)
{
    if (path == NULL || path[0] != '/') return NO_NODE;
    uint8_t current = 0; size_t position = 1;
    while (path[position] != '\0') {
        while (path[position] == '/') ++position;
        if (path[position] == '\0') break;
        size_t start = position;
        while (path[position] != '\0' && path[position] != '/') ++position;
        if (nodes[current].type != X86_64_VFS_DIRECTORY) return NO_NODE;
        current = child(current, path + start, position - start);
        if (current == NO_NODE) return NO_NODE;
    }
    return current;
}
static int split(const char *path, uint8_t *parent, const char **name, size_t *length)
{
    if (path == NULL || path[0] != '/') return 0;
    size_t end = 0; while (path[end] != '\0') ++end;
    while (end > 1 && path[end - 1] == '/') --end;
    size_t start = end; while (start > 0 && path[start - 1] != '/') --start;
    if (start == end || end - start >= NAME_SIZE || start >= 128) return 0;
    char parent_path[128]; size_t parent_length = start > 1 ? start - 1 : 1;
    for (size_t i = 0; i < parent_length; ++i) parent_path[i] = path[i];
    parent_path[parent_length] = '\0';
    *parent = resolve(parent_path); *name = path + start; *length = end - start;
    return *parent != NO_NODE && nodes[*parent].type == X86_64_VFS_DIRECTORY;
}
static int create(const char *path, uint8_t type)
{
    if (resolve(path) != NO_NODE) return 0;
    uint8_t parent; const char *name; size_t length;
    if (!split(path, &parent, &name, &length)) return 0;
    for (uint8_t i = 1; i < MAX_NODES; ++i) if (!nodes[i].used) {
        nodes[i] = (struct node){0}; nodes[i].used = 1; nodes[i].type = type;
        nodes[i].parent = parent; nodes[i].mode = type == X86_64_VFS_DIRECTORY ? 0755 : 0644;
        for (size_t j = 0; j < length; ++j) nodes[i].name[j] = name[j];
        nodes[i].name[length] = '\0'; return 1;
    }
    return 0;
}
void x86_64_vfs_init(void)
{
    for (size_t i = 0; i < MAX_NODES; ++i) nodes[i] = (struct node){0};
    nodes[0].used = 1; nodes[0].type = X86_64_VFS_DIRECTORY; nodes[0].mode = 0755;
    nodes[0].parent = NO_NODE; nodes[0].name[0] = '/';
}
int x86_64_vfs_mkdir(const char *path) { return create(path, X86_64_VFS_DIRECTORY); }
int x86_64_vfs_create(const char *path) { return create(path, X86_64_VFS_FILE); }
int x86_64_vfs_write(const char *path, uint64_t offset, const void *data, size_t size)
{
    uint8_t index = resolve(path);
    if (index == NO_NODE || nodes[index].type != X86_64_VFS_FILE || data == NULL ||
        offset > FILE_CAPACITY || size > FILE_CAPACITY - (size_t)offset) return -1;
    const uint8_t *source = data;
    for (size_t i = 0; i < size; ++i) nodes[index].data[(size_t)offset + i] = source[i];
    uint64_t end = offset + size; if (end > nodes[index].size) nodes[index].size = end;
    return (int)size;
}
int x86_64_vfs_read(const char *path, uint64_t offset, void *data, size_t size)
{
    uint8_t index = resolve(path);
    if (index == NO_NODE || nodes[index].type != X86_64_VFS_FILE || data == NULL) return -1;
    if (offset >= nodes[index].size) return 0;
    uint64_t available = nodes[index].size - offset; if (size > available) size = (size_t)available;
    uint8_t *target = data;
    for (size_t i = 0; i < size; ++i) target[i] = nodes[index].data[(size_t)offset + i];
    return (int)size;
}
int x86_64_vfs_stat(const char *path, struct x86_64_vfs_stat *stat)
{
    uint8_t index = resolve(path); if (index == NO_NODE || stat == NULL) return 0;
    *stat = (struct x86_64_vfs_stat){nodes[index].size, nodes[index].mode, nodes[index].type}; return 1;
}
int x86_64_vfs_conformance_test(void)
{
    static const char message[] = "SplintOS x86_64 VFS"; char output[sizeof(message)] = {0};
    struct x86_64_vfs_stat stat;
    x86_64_vfs_init();
    if (!x86_64_vfs_mkdir("/etc") || !x86_64_vfs_create("/etc/motd") ||
        x86_64_vfs_create("/etc/motd") ||
        x86_64_vfs_write("/etc/motd", 0, message, sizeof(message)) != sizeof(message) ||
        x86_64_vfs_read("/etc/motd", 0, output, sizeof(output)) != sizeof(output) ||
        !x86_64_vfs_stat("/etc/motd", &stat) || stat.size != sizeof(message) ||
        x86_64_vfs_write("/etc/motd", FILE_CAPACITY, message, 1) >= 0) return 0;
    for (size_t i = 0; i < sizeof(message); ++i) if (message[i] != output[i]) return 0;
    return 1;
}

static uint32_t record_checksum(const struct persistent_record *record)
{
    uint32_t hash = UINT32_C(2166136261);
    hash = (hash ^ record->version) * UINT32_C(16777619);
    hash = (hash ^ record->length) * UINT32_C(16777619);
    for (uint32_t i = 0; i < record->length; ++i)
        hash = (hash ^ record->data[i]) * UINT32_C(16777619);
    return hash;
}

int x86_64_vfs_mount_virtio(void)
{
    enum { RECORD_MAGIC = 0x36534653, RECORD_VERSION = 1, RECORD_SECTOR = 2 };
    static const uint8_t initial[] = "SplintOS x86_64 persistent VFS online\n";
    struct persistent_record record = {0};
    if (!x86_64_virtio_block_read(RECORD_SECTOR, &record, 1)) return 0;
    int recovered = record.magic == RECORD_MAGIC && record.version == RECORD_VERSION &&
        record.length != 0 && record.length <= sizeof(record.data) &&
        record.checksum == record_checksum(&record);
    if (!recovered) {
        record = (struct persistent_record){RECORD_MAGIC, RECORD_VERSION,
                                            sizeof(initial), 0, {0}};
        for (size_t i = 0; i < sizeof(initial); ++i) record.data[i] = initial[i];
        record.checksum = record_checksum(&record);
        if (!x86_64_virtio_block_write(RECORD_SECTOR, &record, 1) ||
            !x86_64_virtio_block_flush()) return 0;
    }
    if (!x86_64_vfs_mkdir("/disk") || !x86_64_vfs_create("/disk/state") ||
        x86_64_vfs_write("/disk/state", 0, record.data, record.length) !=
            (int)record.length) return 0;
    uint8_t verify[496];
    if (x86_64_vfs_read("/disk/state", 0, verify, record.length) !=
        (int)record.length) return 0;
    for (uint32_t i = 0; i < record.length; ++i)
        if (verify[i] != record.data[i]) return 0;
    return recovered ? 2 : 1;
}
