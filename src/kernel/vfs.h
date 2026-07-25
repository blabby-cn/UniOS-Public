#ifndef UNI_VFS_H
#define UNI_VFS_H

#include <stdint.h>
#include "fat32.h"

#define VFS_DIR 1
#define VFS_FILE 2

struct vfs_stat
{
    char name[FAT32_NAME_MAX];
    uint32_t size;
    uint8_t type;
};

typedef void (*vfs_list_cb)(const struct vfs_stat *st, void *ctx);

int vfs_init(void);
int vfs_stat(const char *path, struct vfs_stat *out);
int vfs_list(const char *path, vfs_list_cb cb, void *ctx);
int64_t vfs_read(const char *path, void *buf, uint32_t maxlen);
int vfs_write(const char *path, const void *data, uint32_t len);
int vfs_mkdir(const char *path);

#endif
