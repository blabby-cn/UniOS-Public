#ifndef UNI_FAT32_H
#define UNI_FAT32_H

#include <stdint.h>

#define FAT32_NAME_MAX 64

struct fat32_dirent
{
    char name[FAT32_NAME_MAX];
    uint32_t size;
    uint32_t first_cluster;
    uint8_t attr;
};

typedef void (*fat32_list_cb)(const struct fat32_dirent *e, void *ctx);

int fat32_mount(void);
int fat32_list(const char *path, fat32_list_cb cb, void *ctx);
int64_t fat32_read(const char *path, void *buf, uint32_t maxlen);
int fat32_write(const char *path, const void *data, uint32_t len);
int fat32_mkdir(const char *path);
int fat32_stat(const char *path, struct fat32_dirent *out);
uint32_t fat32_cluster_size(void);
uint32_t fat32_total_clusters(void);
uint32_t fat32_free_clusters(void);

#endif
