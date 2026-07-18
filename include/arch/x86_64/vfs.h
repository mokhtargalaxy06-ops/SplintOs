#ifndef SPLINTOS_ARCH_X86_64_VFS_H
#define SPLINTOS_ARCH_X86_64_VFS_H
#include <stddef.h>
#include <stdint.h>
enum x86_64_vfs_type { X86_64_VFS_FILE, X86_64_VFS_DIRECTORY };
struct x86_64_vfs_stat { uint64_t size; uint32_t mode; uint8_t type; };
void x86_64_vfs_init(void);
int x86_64_vfs_mkdir(const char *path);
int x86_64_vfs_create(const char *path);
int x86_64_vfs_write(const char *path, uint64_t offset, const void *data, size_t size);
int x86_64_vfs_read(const char *path, uint64_t offset, void *data, size_t size);
int x86_64_vfs_stat(const char *path, struct x86_64_vfs_stat *stat);
int x86_64_vfs_conformance_test(void);
int x86_64_vfs_mount_virtio(void);
#endif
