#include "filesystem.h"

#include "devices.h"
#include "diskfs.h"
#include "memory.h"
#include "scheduler.h"
#include "security.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MAX_NODES = 128,
    MAX_DESCRIPTORS = 32,
    NAME_LENGTH = 32,
    NO_NODE = 0xFFFF,
};

enum device_kind { DEVICE_NONE, DEVICE_NULL, DEVICE_SERIAL };

struct vfs_node {
    bool used;
    enum vfs_node_type type;
    enum device_kind device;
    uint16_t parent;
    char name[NAME_LENGTH];
    uint8_t *data;
    size_t size;
    size_t capacity;
    uint16_t mode;
    uint32_t owner;
    bool disk_backed;
    bool disk_mount;
};

struct file_descriptor {
    bool used;
    uint16_t node;
    uint32_t flags;
    size_t offset;
};

static struct vfs_node nodes[MAX_NODES];
static struct file_descriptor descriptors[MAX_DESCRIPTORS];

extern const uint8_t _binary_build_user_hello_elf_start[];
extern const uint8_t _binary_build_user_hello_elf_end[];
extern const uint8_t _binary_build_user_cat_elf_start[];
extern const uint8_t _binary_build_user_cat_elf_end[];
extern const uint8_t _binary_build_user_runner_elf_start[];
extern const uint8_t _binary_build_user_runner_elf_end[];
extern const uint8_t _binary_build_user_sh_elf_start[];
extern const uint8_t _binary_build_user_sh_elf_end[];
extern const uint8_t _binary_build_user_fdtest_elf_start[];
extern const uint8_t _binary_build_user_fdtest_elf_end[];
extern const uint8_t _binary_build_user_echo_elf_start[];
extern const uint8_t _binary_build_user_echo_elf_end[];
extern const uint8_t _binary_build_user_pipetest_elf_start[];
extern const uint8_t _binary_build_user_pipetest_elf_end[];
extern const uint8_t _binary_build_user_wc_elf_start[];
extern const uint8_t _binary_build_user_wc_elf_end[];
extern const uint8_t _binary_build_user_ls_elf_start[], _binary_build_user_ls_elf_end[];
extern const uint8_t _binary_build_user_mem_elf_start[], _binary_build_user_mem_elf_end[];
extern const uint8_t _binary_build_user_uptime_elf_start[], _binary_build_user_uptime_elf_end[];
extern const uint8_t _binary_build_user_ps_elf_start[], _binary_build_user_ps_elf_end[];
extern const uint8_t _binary_build_user_heaptest_elf_start[], _binary_build_user_heaptest_elf_end[];
extern const uint8_t _binary_build_user_disk_elf_start[], _binary_build_user_disk_elf_end[];

static size_t string_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') ++length;
    return length;
}

static bool name_equal(const char *left, const char *right, size_t right_length)
{
    size_t i = 0;
    while (i < right_length && left[i] == right[i]) ++i;
    return i == right_length && left[i] == '\0';
}

static void name_copy(char destination[NAME_LENGTH], const char *source, size_t length)
{
    if (length >= NAME_LENGTH) length = NAME_LENGTH - 1;
    size_t i;
    for (i = 0; i < length; ++i) destination[i] = source[i];
    destination[i] = '\0';
}

static void bytes_copy(void *destination, const void *source, size_t count)
{
    uint8_t *to = destination;
    const uint8_t *from = source;
    while (count-- != 0) *to++ = *from++;
}

static uint16_t child_find(uint16_t parent, const char *name, size_t length)
{
    for (uint16_t i = 1; i < MAX_NODES; ++i)
        if (nodes[i].used && nodes[i].parent == parent && name_equal(nodes[i].name, name, length))
            return i;
    return NO_NODE;
}

static bool permitted(const struct vfs_node *node, uint16_t owner_bit, uint16_t other_bit);

static uint16_t path_resolve(const char *path)
{
    if (path == NULL || path[0] != '/') return NO_NODE;
    uint16_t current = 0;
    size_t position = 1;
    while (path[position] != '\0') {
        while (path[position] == '/') ++position;
        if (path[position] == '\0') break;
        size_t start = position;
        while (path[position] != '\0' && path[position] != '/') ++position;
        if (!permitted(&nodes[current], 0100, 0001)) return NO_NODE;
        current = child_find(current, path + start, position - start);
        if (current == NO_NODE) return NO_NODE;
    }
    return current;
}

static bool permitted(const struct vfs_node *node, uint16_t owner_bit, uint16_t other_bit)
{
    uint32_t uid = task_current_uid();
    if (uid == 0) return true;
    return (node->mode & (uid == node->owner ? owner_bit : other_bit)) != 0;
}

static bool split_parent(const char *path, uint16_t *parent, const char **name, size_t *length)
{
    if (path == NULL || path[0] != '/') return false;
    size_t end = string_length(path);
    while (end > 1 && path[end - 1] == '/') --end;
    size_t start = end;
    while (start > 0 && path[start - 1] != '/') --start;
    if (start == end) return false;
    char parent_path[128];
    if (start >= sizeof(parent_path)) return false;
    size_t parent_length = start > 1 ? start - 1 : 1;
    for (size_t i = 0; i < parent_length; ++i) parent_path[i] = path[i];
    parent_path[parent_length] = '\0';
    *parent = path_resolve(parent_path);
    *name = path + start;
    *length = end - start;
    return *parent != NO_NODE && nodes[*parent].type == VFS_DIRECTORY && *length < NAME_LENGTH;
}

static int node_create(const char *path, enum vfs_node_type type)
{
    if (path_resolve(path) != NO_NODE) return -1;
    uint16_t parent;
    const char *name;
    size_t length;
    if (!split_parent(path, &parent, &name, &length)) return -1;
    if (!permitted(&nodes[parent], 0200, 0002)) return -1;
    for (uint16_t i = 1; i < MAX_NODES; ++i) {
        if (nodes[i].used) continue;
        nodes[i] = (struct vfs_node){0};
        nodes[i].used = true;
        nodes[i].type = type;
        nodes[i].parent = parent;
        nodes[i].owner = task_current_uid();
        nodes[i].mode = type == VFS_DIRECTORY ? 0755 : 0644;
        if (type == VFS_FILE && nodes[parent].disk_mount) {
            nodes[i].disk_backed = true;
            char disk_name[NAME_LENGTH];
            name_copy(disk_name, name, length);
            if (diskfs_write_file(disk_name, "", 0) != 0) {
                nodes[i].used = false;
                return -1;
            }
        }
        name_copy(nodes[i].name, name, length);
        return 0;
    }
    return -1;
}

int vfs_mkdir(const char *path) { return node_create(path, VFS_DIRECTORY); }
int vfs_create(const char *path) { return node_create(path, VFS_FILE); }

int vfs_open(const char *path, uint32_t flags)
{
    uint16_t node = path_resolve(path);
    if (node == NO_NODE && (flags & VFS_CREATE) != 0) {
        if (vfs_create(path) != 0) return -1;
        node = path_resolve(path);
    }
    if (node == NO_NODE || nodes[node].type == VFS_DIRECTORY) return -1;
    if ((flags & VFS_READ) != 0 && !permitted(&nodes[node], 0400, 0004)) return -1;
    if ((flags & VFS_WRITE) != 0 && !permitted(&nodes[node], 0200, 0002)) return -1;
    if (nodes[node].type == VFS_DEVICE && task_current_uid() != 0 &&
        !security_has_capability(CAP_DEVICE_ACCESS)) return -1;
    if ((flags & VFS_TRUNCATE) != 0 && (flags & VFS_WRITE) != 0 &&
        nodes[node].type == VFS_FILE) {
        nodes[node].size = 0;
        if (nodes[node].disk_backed && diskfs_write_file(nodes[node].name, "", 0) != 0)
            return -1;
    }
    for (int fd = 0; fd < MAX_DESCRIPTORS; ++fd) {
        if (descriptors[fd].used) continue;
        descriptors[fd].used = true;
        descriptors[fd].node = node;
        descriptors[fd].flags = flags;
        descriptors[fd].offset = (flags & VFS_APPEND) != 0 ? nodes[node].size : 0;
        return fd;
    }
    return -1;
}

int vfs_close(int fd)
{
    if (fd < 0 || fd >= MAX_DESCRIPTORS || !descriptors[fd].used) return -1;
    descriptors[fd].used = false;
    return 0;
}

int vfs_read(int fd, void *buffer, size_t count)
{
    if (fd < 0 || fd >= MAX_DESCRIPTORS || !descriptors[fd].used || buffer == NULL ||
        (descriptors[fd].flags & VFS_READ) == 0) return -1;
    struct file_descriptor *file = &descriptors[fd];
    struct vfs_node *node = &nodes[file->node];
    if (node->type == VFS_DEVICE) return 0;
    if (file->offset >= node->size) return 0;
    if (count > node->size - file->offset) count = node->size - file->offset;
    if (node->disk_backed) {
        uint8_t *contents = kmalloc(DISKFS_FILE_SIZE);
        if (contents == NULL) return -1;
        int size = diskfs_read_file(node->name, contents, DISKFS_FILE_SIZE);
        if (size < 0 || (size_t)size != node->size) { kfree(contents); return -1; }
        bytes_copy(buffer, contents + file->offset, count);
        kfree(contents);
    } else {
        bytes_copy(buffer, node->data + file->offset, count);
    }
    file->offset += count;
    return (int)count;
}

static bool file_reserve(struct vfs_node *node, size_t required)
{
    if (required <= node->capacity) return true;
    size_t capacity = node->capacity == 0 ? 64 : node->capacity;
    while (capacity < required) capacity *= 2;
    uint8_t *data = kmalloc(capacity);
    if (data == NULL) return false;
    if (node->data != NULL) {
        bytes_copy(data, node->data, node->size);
        kfree(node->data);
    }
    node->data = data;
    node->capacity = capacity;
    return true;
}

int vfs_write(int fd, const void *buffer, size_t count)
{
    if (fd < 0 || fd >= MAX_DESCRIPTORS || !descriptors[fd].used || buffer == NULL ||
        (descriptors[fd].flags & VFS_WRITE) == 0) return -1;
    struct file_descriptor *file = &descriptors[fd];
    struct vfs_node *node = &nodes[file->node];
    if (node->type == VFS_DEVICE) {
        if (node->device == DEVICE_SERIAL) {
            const char *characters = buffer;
            for (size_t i = 0; i < count; ++i) {
                char text[2] = {characters[i], '\0'};
                serial_write(text);
            }
        }
        return (int)count;
    }
    if (node->disk_backed) {
        if (file->offset + count > DISKFS_FILE_SIZE) return -1;
        uint8_t *contents = kmalloc(DISKFS_FILE_SIZE);
        if (contents == NULL) return -1;
        for (size_t i = 0; i < DISKFS_FILE_SIZE; ++i) contents[i] = 0;
        if (node->size != 0) {
            int size = diskfs_read_file(node->name, contents, DISKFS_FILE_SIZE);
            if (size < 0 || (size_t)size != node->size) { kfree(contents); return -1; }
        }
        bytes_copy(contents + file->offset, buffer, count);
        size_t new_size = file->offset + count > node->size ? file->offset + count : node->size;
        if (diskfs_write_file(node->name, contents, new_size) != 0) {
            kfree(contents); return -1;
        }
        kfree(contents);
    } else {
        if (!file_reserve(node, file->offset + count)) return -1;
        bytes_copy(node->data + file->offset, buffer, count);
    }
    file->offset += count;
    if (file->offset > node->size) node->size = file->offset;
    return (int)count;
}

int vfs_seek(int fd, size_t offset)
{
    if (fd < 0 || fd >= MAX_DESCRIPTORS || !descriptors[fd].used) return -1;
    if (offset > nodes[descriptors[fd].node].size) return -1;
    descriptors[fd].offset = offset;
    return 0;
}

int vfs_fsync(int fd)
{
    if (fd < 0 || fd >= MAX_DESCRIPTORS || !descriptors[fd].used) return -1;
    struct vfs_node *node = &nodes[descriptors[fd].node];
    if (node->type != VFS_FILE) return -1;
    return node->disk_backed ? diskfs_flush() : 0;
}

int vfs_unlink(const char *path)
{
    uint16_t node = path_resolve(path);
    if (node == NO_NODE || nodes[node].type != VFS_FILE ||
        !permitted(&nodes[node], 0200, 0002)) return -1;
    for (int fd = 0; fd < MAX_DESCRIPTORS; ++fd)
        if (descriptors[fd].used && descriptors[fd].node == node) return -1;
    if (nodes[node].disk_backed && diskfs_unlink(nodes[node].name) != 0) return -1;
    if (nodes[node].data != NULL) kfree(nodes[node].data);
    nodes[node] = (struct vfs_node){0};
    return 0;
}

int vfs_rename(const char *old_path, const char *new_path)
{
    uint16_t node = path_resolve(old_path);
    uint16_t destination = path_resolve(new_path);
    if (node == NO_NODE || node == 0 || node == destination ||
        !permitted(&nodes[node], 0200, 0002)) return -1;
    uint16_t new_parent;
    const char *new_name;
    size_t new_length;
    if (!split_parent(new_path, &new_parent, &new_name, &new_length) ||
        new_parent != nodes[node].parent ||
        !permitted(&nodes[new_parent], 0200, 0002)) return -1;
    if (destination != NO_NODE) {
        if (nodes[destination].type != VFS_FILE ||
            nodes[destination].parent != new_parent ||
            nodes[destination].disk_backed != nodes[node].disk_backed) return -1;
        for (int fd = 0; fd < MAX_DESCRIPTORS; ++fd)
            if (descriptors[fd].used && descriptors[fd].node == destination) return -1;
    }
    if (nodes[node].disk_backed) {
        char replacement[NAME_LENGTH];
        name_copy(replacement, new_name, new_length);
        if (diskfs_rename(nodes[node].name, replacement) != 0) return -1;
    }
    if (destination != NO_NODE) {
        if (nodes[destination].data != NULL) kfree(nodes[destination].data);
        nodes[destination] = (struct vfs_node){0};
    }
    name_copy(nodes[node].name, new_name, new_length);
    return 0;
}

int vfs_list(const char *path, struct vfs_directory_entry *entries, size_t capacity)
{
    uint16_t directory = path_resolve(path);
    if (directory == NO_NODE || nodes[directory].type != VFS_DIRECTORY || entries == NULL) return -1;
    if (!permitted(&nodes[directory], 0400, 0004)) return -1;
    size_t count = 0;
    for (uint16_t i = 1; i < MAX_NODES && count < capacity; ++i) {
        if (!nodes[i].used || nodes[i].parent != directory) continue;
        name_copy(entries[count].name, nodes[i].name, string_length(nodes[i].name));
        entries[count].type = nodes[i].type;
        entries[count].size = nodes[i].size;
        entries[count].mode = nodes[i].mode;
        entries[count].owner = nodes[i].owner;
        ++count;
    }
    return (int)count;
}

int vfs_chmod(const char *path, uint16_t mode)
{
    uint16_t node = path_resolve(path);
    if (node == NO_NODE || mode > 0777) return -1;
    if (task_current_uid() != nodes[node].owner &&
        !security_has_capability(CAP_CHANGE_PERMISSIONS)) return -1;
    nodes[node].mode = mode;
    security_audit("filesystem permissions changed");
    return 0;
}

static void initial_file(const char *path, const char *contents)
{
    int fd = vfs_open(path, VFS_WRITE | VFS_CREATE);
    if (fd >= 0) {
        (void)vfs_write(fd, contents, string_length(contents));
        (void)vfs_close(fd);
    }
}

static void initial_binary(const char *path, const void *data, size_t size)
{
    int fd = vfs_open(path, VFS_WRITE | VFS_CREATE);
    if (fd >= 0) {
        (void)vfs_write(fd, data, size);
        (void)vfs_close(fd);
    }
}

void filesystem_init(void)
{
    nodes[0].used = true;
    nodes[0].type = VFS_DIRECTORY;
    nodes[0].parent = NO_NODE;
    nodes[0].owner = 0;
    nodes[0].mode = 0755;
    name_copy(nodes[0].name, "/", 1);
    (void)vfs_mkdir("/dev");
    (void)vfs_mkdir("/etc");
    (void)vfs_mkdir("/tmp");
    (void)vfs_mkdir("/bin");
    (void)vfs_mkdir("/disk");
    uint16_t disk_node = path_resolve("/disk");
    if (disk_node != NO_NODE) {
        struct diskfs_entry entries[12];
        int count = diskfs_list(entries, 12);
        for (int i = 0; i < count; ++i) {
            char path[NAME_LENGTH + 7] = "/disk/";
            size_t j = 0;
            while (entries[i].name[j] != '\0') { path[6 + j] = entries[i].name[j]; ++j; }
            path[6 + j] = '\0';
            if (node_create(path, VFS_FILE) == 0) {
                uint16_t child = path_resolve(path);
                if (child != NO_NODE) {
                    nodes[child].disk_backed = true;
                    nodes[child].size = entries[i].size;
                }
            }
        }
        nodes[disk_node].disk_mount = true;
    }
    (void)vfs_chmod("/tmp", 0777);
    (void)node_create("/dev/null", VFS_DEVICE);
    uint16_t null_node = path_resolve("/dev/null");
    if (null_node != NO_NODE) nodes[null_node].device = DEVICE_NULL;
    (void)node_create("/dev/serial", VFS_DEVICE);
    uint16_t serial_node = path_resolve("/dev/serial");
    if (serial_node != NO_NODE) nodes[serial_node].device = DEVICE_SERIAL;
    initial_file("/etc/motd", "Welcome to SplintOS - created in Morocco.\n");
    initial_file("/README", "SplintOS in-memory filesystem is online.\n");
    initial_binary("/bin/hello", _binary_build_user_hello_elf_start,
                   (size_t)(_binary_build_user_hello_elf_end -
                            _binary_build_user_hello_elf_start));
    initial_binary("/bin/cat", _binary_build_user_cat_elf_start,
                   (size_t)(_binary_build_user_cat_elf_end -
                            _binary_build_user_cat_elf_start));
    initial_binary("/bin/runner", _binary_build_user_runner_elf_start,
                   (size_t)(_binary_build_user_runner_elf_end -
                            _binary_build_user_runner_elf_start));
    initial_binary("/bin/sh", _binary_build_user_sh_elf_start,
                   (size_t)(_binary_build_user_sh_elf_end -
                            _binary_build_user_sh_elf_start));
    initial_binary("/bin/fdtest", _binary_build_user_fdtest_elf_start,
                   (size_t)(_binary_build_user_fdtest_elf_end -
                            _binary_build_user_fdtest_elf_start));
    initial_binary("/bin/echo", _binary_build_user_echo_elf_start,
                   (size_t)(_binary_build_user_echo_elf_end -
                            _binary_build_user_echo_elf_start));
    initial_binary("/bin/pipetest", _binary_build_user_pipetest_elf_start,
                   (size_t)(_binary_build_user_pipetest_elf_end -
                            _binary_build_user_pipetest_elf_start));
    initial_binary("/bin/wc", _binary_build_user_wc_elf_start,
                   (size_t)(_binary_build_user_wc_elf_end -
                            _binary_build_user_wc_elf_start));
    initial_binary("/bin/ls", _binary_build_user_ls_elf_start,
                   (size_t)(_binary_build_user_ls_elf_end - _binary_build_user_ls_elf_start));
    initial_binary("/bin/mem", _binary_build_user_mem_elf_start,
                   (size_t)(_binary_build_user_mem_elf_end - _binary_build_user_mem_elf_start));
    initial_binary("/bin/uptime", _binary_build_user_uptime_elf_start,
                   (size_t)(_binary_build_user_uptime_elf_end - _binary_build_user_uptime_elf_start));
    initial_binary("/bin/ps", _binary_build_user_ps_elf_start,
                   (size_t)(_binary_build_user_ps_elf_end - _binary_build_user_ps_elf_start));
    initial_binary("/bin/heaptest", _binary_build_user_heaptest_elf_start,
                   (size_t)(_binary_build_user_heaptest_elf_end - _binary_build_user_heaptest_elf_start));
    initial_binary("/bin/disk", _binary_build_user_disk_elf_start,
                   (size_t)(_binary_build_user_disk_elf_end - _binary_build_user_disk_elf_start));
    serial_write("SplintOS: writable RAM filesystem mounted at /\r\n");
}
