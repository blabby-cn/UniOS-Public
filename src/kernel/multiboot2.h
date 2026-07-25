#ifndef UNI_MB2_H
#define UNI_MB2_H

#include <stdint.h>

#define MB2_TAG_END 0
#define MB2_TAG_FB 8
#define MB2_TAG_MMAP 6

struct mb2_tag
{
    uint32_t type;
    uint32_t size;
};

struct mb2_fb
{
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t fb_type;
    uint16_t reserved;
};

static inline struct mb2_fb *mb2_find_fb(uint32_t info_addr)
{
    uint8_t *base = (uint8_t *)(unsigned long)info_addr;
    uint32_t total = *(uint32_t *)base;
    uint8_t *end = base + total;
    uint8_t *cur = base + 8;
    while (cur < end)
    {
        struct mb2_tag *t = (struct mb2_tag *)cur;
        if (t->type == MB2_TAG_END)
        {
            break;
        }
        if (t->type == MB2_TAG_FB)
        {
            return (struct mb2_fb *)cur;
        }
        cur += (t->size + 7u) & ~7u;
    }
    return (struct mb2_fb *)0;
}

struct mb2_mmap
{
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct mb2_mmap_entry
{
    uint64_t base;
    uint64_t length;
    uint32_t mtype;
    uint32_t reserved;
};

static inline struct mb2_mmap *mb2_find_mmap(uint32_t info_addr)
{
    uint8_t *base = (uint8_t *)(unsigned long)info_addr;
    uint32_t total = *(uint32_t *)base;
    uint8_t *end = base + total;
    uint8_t *cur = base + 8;
    while (cur < end)
    {
        struct mb2_tag *t = (struct mb2_tag *)cur;
        if (t->type == MB2_TAG_END)
        {
            break;
        }
        if (t->type == MB2_TAG_MMAP)
        {
            return (struct mb2_mmap *)cur;
        }
        cur += (t->size + 7u) & ~7u;
    }
    return (struct mb2_mmap *)0;
}

#endif
