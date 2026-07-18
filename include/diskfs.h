#ifndef SPLINTOS_DISKFS_H
#define SPLINTOS_DISKFS_H

#include <stddef.h>

enum { DISKFS_NAME_SIZE = 32, DISKFS_FILE_SIZE = 4096 };

struct diskfs_entry {
    char name[DISKFS_NAME_SIZE];
    size_t size;
    unsigned int birth_time, modification_time, change_time;
};

void diskfs_init(void);
int diskfs_write_file(const char *name, const void *data, size_t size);
int diskfs_read_file(const char *name, void *data, size_t capacity);
int diskfs_flush(void);
int diskfs_unmount(void);
int diskfs_list(struct diskfs_entry *entries, size_t capacity);
int diskfs_unlink(const char *name);
int diskfs_rename(const char *old_name, const char *new_name);
int diskfs_stat(const char *name, struct diskfs_entry *entry);
int diskfs_touch_metadata(const char *name);
int diskfs_migrate_legacy(void);

#endif
