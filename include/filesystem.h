#ifndef SPLINTOS_FILESYSTEM_H
#define SPLINTOS_FILESYSTEM_H

#include <stddef.h>
#include <stdint.h>

enum vfs_open_flags {
    VFS_READ = 1,
    VFS_WRITE = 2,
    VFS_CREATE = 4,
    VFS_APPEND = 8,
    VFS_TRUNCATE = 16,
};

enum vfs_node_type {
    VFS_FILE,
    VFS_DIRECTORY,
    VFS_DEVICE,
};

struct vfs_directory_entry {
    char name[32];
    uint32_t type;
    uint32_t size;
    uint16_t mode, reserved;
    uint32_t owner;
};

struct vfs_timestamp_entry {
    char name[32];
    uint32_t type, size;
    uint16_t mode, reserved;
    uint32_t owner;
    uint32_t birth_time, modification_time, change_time;
};

_Static_assert(sizeof(struct vfs_timestamp_entry) == 60,
               "timestamp entry ABI changed");
_Static_assert(sizeof(struct vfs_directory_entry) == 48,
               "directory entry ABI changed");

void filesystem_init(void);
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
int vfs_create(const char *path);
int vfs_open(const char *path, uint32_t flags);
int vfs_close(int descriptor);
int vfs_read(int descriptor, void *buffer, size_t count);
int vfs_write(int descriptor, const void *buffer, size_t count);
int vfs_seek(int descriptor, size_t offset);
int vfs_truncate(int descriptor, size_t size);
int vfs_fsync(int descriptor);
int vfs_unlink(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_list(const char *path, struct vfs_directory_entry *entries, size_t capacity);
int vfs_stat(const char *path, struct vfs_directory_entry *entry);
int vfs_stat_timestamps(const char *path, struct vfs_timestamp_entry *entry);
int vfs_chmod(const char *path, uint16_t mode);

#endif
