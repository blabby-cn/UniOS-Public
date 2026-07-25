#include "vfs.h"
#include "ata.h"

void *memcpy(void *dst, const void *src, unsigned long n);

static int g_ready;

int vfs_init(void)
{
    if (ata_init())
        return -1;
    if (fat32_mount())
        return -2;
    g_ready = 1;
    return 0;
}

int vfs_stat(const char *path, struct vfs_stat *out)
{
    if (!g_ready)
        return -1;
    struct fat32_dirent d;
    if (fat32_stat(path, &d))
        return -1;
    memcpy(out->name, d.name, FAT32_NAME_MAX);
    out->size = d.size;
    out->type = (d.attr & 0x10) ? VFS_DIR : VFS_FILE;
    return 0;
}

struct list_ctx
{
    vfs_list_cb cb;
    void *ctx;
};

static void list_thunk(const struct fat32_dirent *e, void *ctx)
{
    struct list_ctx *lc = (struct list_ctx *)ctx;
    struct vfs_stat st;
    memcpy(st.name, e->name, FAT32_NAME_MAX);
    st.size = e->size;
    st.type = (e->attr & 0x10) ? VFS_DIR : VFS_FILE;
    lc->cb(&st, lc->ctx);
}

int vfs_list(const char *path, vfs_list_cb cb, void *ctx)
{
    if (!g_ready)
        return -1;
    struct list_ctx lc;
    lc.cb = cb;
    lc.ctx = ctx;
    return fat32_list(path, list_thunk, &lc);
}

int64_t vfs_read(const char *path, void *buf, uint32_t maxlen)
{
    if (!g_ready)
        return -1;
    return fat32_read(path, buf, maxlen);
}

int vfs_write(const char *path, const void *data, uint32_t len)
{
    if (!g_ready)
        return -1;
    return fat32_write(path, data, len);
}

int vfs_mkdir(const char *path)
{
    if (!g_ready)
        return -1;
    return fat32_mkdir(path);
}
