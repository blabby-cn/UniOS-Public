#include "udll.h"
#include "kheap.h"
#include "vfs.h"
#include "kprintf.h"
#include "util.h"
#include "wm.h"
#include "ui.h"
#include "gfx.h"
#include "sched.h"

#define UDLL_MAGIC 0x4C4C4455
#define UDLL_VERSION 1

#define UDLL_FIX_ADDR64 0
#define UDLL_FIX_CALL32 1
#define UDLL_FIX_ADDR32 2

#define UDLL_TRAMP_SIZE 16

struct udll_hdr
{
    uint32_t magic;
    uint32_t version;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t reloc_count;
    uint32_t fixup_count;
    uint32_t export_count;
    uint8_t reserved[36];
};

struct udll_fixup
{
    uint32_t offset;
    uint8_t type;
    uint8_t pad[3];
    int32_t addend;
    char name[32];
};

struct udll_export
{
    char name[32];
    uint32_t offset;
};

struct UdllHandle
{
    uint8_t *base;
    uint32_t total_size;
    struct udll_export *exports;
    uint32_t export_count;
};

struct KernSym
{
    const char *name;
    void *addr;
};

static const struct KernSym g_kern_syms[] = {
    {"wm_create_dialog", (void *)wm_create_dialog},
    {"wm_destroy_window", (void *)wm_destroy_window},
    {"wm_close_window", (void *)wm_close_window},
    {"wm_window_set_close", (void *)wm_window_set_close},
    {"wm_window_set_click", (void *)wm_window_set_click},
    {"wm_window_add_widget", (void *)wm_window_add_widget},
    {"wm_window_anchor", (void *)wm_window_anchor},
    {"wm_window_set_title", (void *)wm_window_set_title},
    {"wm_show", (void *)wm_show},
    {"wm_focus", (void *)wm_focus},
    {"wm_mark_dirty", (void *)wm_mark_dirty},
    {"ui_button_init", (void *)ui_button_init},
    {"ui_textinput_init", (void *)ui_textinput_init},
    {"ui_dropdown_init", (void *)ui_dropdown_init},
    {"ui_dropdown_add", (void *)ui_dropdown_add},
    {"gfx_fill", (void *)gfx_fill},
    {"gfx_text", (void *)gfx_text},
    {"gfx_rect", (void *)gfx_rect},
    {"gfx_text_width", (void *)gfx_text_width},
    {"gfx_fill_blend", (void *)gfx_fill_blend},
    {"vfs_list", (void *)vfs_list},
    {"vfs_read", (void *)vfs_read},
    {"vfs_write", (void *)vfs_write},
    {"vfs_stat", (void *)vfs_stat},
    {"kmalloc", (void *)kmalloc},
    {"kfree", (void *)kfree},
    {"memset", (void *)memset},
    {"memcpy", (void *)memcpy},
    {"kprintf", (void *)kprintf},
    {"sched_ticks", (void *)sched_ticks},
    {"strcmp", (void *)strcmp},
    {"strlen", (void *)strlen},
    {"strncmp", (void *)strncmp},
    {"strncpy", (void *)strncpy},
    {"strstr", (void *)strstr},
    {0, 0}
};

static void *kern_lookup(const char *name)
{
    uint32_t i = 0;
    while (g_kern_syms[i].name)
    {
        const char *a = g_kern_syms[i].name;
        const char *b = name;
        while (*a && *b && *a == *b)
        {
            a++;
            b++;
        }
        if (*a == 0 && *b == 0)
            return g_kern_syms[i].addr;
        i++;
    }
    return 0;
}

static int name_eq(const char *a, const char *b)
{
    while (*a && *b && *a == *b)
    {
        a++;
        b++;
    }
    return (*a == 0 && *b == 0);
}

struct UdllHandle *udll_load(const char *path)
{
    struct udll_hdr hdr;
    int64_t n = vfs_read(path, &hdr, sizeof(hdr));
    if (n < (int64_t)sizeof(hdr))
    {
        kprintf("udll: cannot read header from %s\n", path);
        return 0;
    }
    if (hdr.magic != UDLL_MAGIC)
    {
        kprintf("udll: bad magic 0x%x\n", hdr.magic);
        return 0;
    }
    if (hdr.version != UDLL_VERSION)
    {
        kprintf("udll: bad version %u\n", hdr.version);
        return 0;
    }

    uint32_t reloc_bytes = hdr.reloc_count * 4;
    uint32_t fixup_bytes = hdr.fixup_count * sizeof(struct udll_fixup);
    uint32_t export_bytes = hdr.export_count * sizeof(struct udll_export);
    uint32_t file_size = 64 + reloc_bytes + fixup_bytes + export_bytes
                       + hdr.text_size + hdr.data_size;

    uint8_t *raw = kmalloc(file_size);
    if (!raw)
    {
        kprintf("udll: cannot alloc %u for raw\n", file_size);
        return 0;
    }
    n = vfs_read(path, raw, file_size);
    if (n < (int64_t)file_size)
    {
        kprintf("udll: short read %lld/%u\n", (long long)n, file_size);
        kfree(raw);
        return 0;
    }

    uint8_t *p = raw + 64;
    uint32_t *relocs = (uint32_t *)p;
    p += reloc_bytes;
    struct udll_fixup *fixups = (struct udll_fixup *)p;
    p += fixup_bytes;
    struct udll_export *exports = (struct udll_export *)p;
    p += export_bytes;
    uint8_t *text = p;
    p += hdr.text_size;
    uint8_t *data_sec = p;

    uint32_t n_call32 = 0;
    uint32_t i;
    for (i = 0; i < hdr.fixup_count; i++)
    {
        if (fixups[i].type == UDLL_FIX_CALL32)
            n_call32++;
    }

    uint32_t mem_size = hdr.text_size + hdr.data_size + hdr.bss_size;
    uint32_t tramp_size = n_call32 * UDLL_TRAMP_SIZE;
    uint32_t alloc_size = mem_size + tramp_size + export_bytes + 32;

    uint8_t *base = kmalloc(alloc_size);
    if (!base)
    {
        kprintf("udll: cannot alloc %u for image\n", alloc_size);
        kfree(raw);
        return 0;
    }

    if (hdr.text_size)
        memcpy(base, text, hdr.text_size);
    if (hdr.data_size)
        memcpy(base + hdr.text_size, data_sec, hdr.data_size);
    if (hdr.bss_size)
        memset(base + hdr.text_size + hdr.data_size, 0, hdr.bss_size);

    uint8_t *tramp_base = base + mem_size;
    struct udll_export *exp_copy = (struct udll_export *)(tramp_base + tramp_size);
    if (export_bytes)
        memcpy(exp_copy, exports, export_bytes);

    uint64_t base_addr = (uint64_t)(uintptr_t)base;

    for (i = 0; i < hdr.reloc_count; i++)
    {
        uint32_t off = relocs[i];
        if (off + 8 <= mem_size)
        {
            uint64_t val = *(uint64_t *)(base + off);
            val += base_addr;
            *(uint64_t *)(base + off) = val;
        }
    }

    uint32_t tramp_idx = 0;
    for (i = 0; i < hdr.fixup_count; i++)
    {
        struct udll_fixup *f = &fixups[i];
        void *sym = kern_lookup(f->name);
        if (!sym)
        {
            kprintf("udll: unresolved: %s\n", f->name);
            kfree(raw);
            kfree(base);
            return 0;
        }
        uint64_t sym_addr = (uint64_t)(uintptr_t)sym;
        uint32_t off = f->offset;

        if (f->type == UDLL_FIX_ADDR64)
        {
            if (off + 8 <= mem_size)
                *(uint64_t *)(base + off) = sym_addr + (int64_t)f->addend;
        }
        else if (f->type == UDLL_FIX_CALL32)
        {
            uint8_t *t = tramp_base + tramp_idx * UDLL_TRAMP_SIZE;
            t[0] = 0x48;
            t[1] = 0xB8;
            *(uint64_t *)(t + 2) = sym_addr;
            t[10] = 0xFF;
            t[11] = 0xE0;

            uint64_t tramp_addr = (uint64_t)(uintptr_t)t;
            if (off + 4 <= mem_size)
            {
                int64_t val = (int64_t)tramp_addr + (int64_t)f->addend
                            - (int64_t)(base_addr + off);
                *(int32_t *)(base + off) = (int32_t)val;
            }
            tramp_idx++;
        }
        else if (f->type == UDLL_FIX_ADDR32)
        {
            if (off + 4 <= mem_size)
                *(uint32_t *)(base + off) = (uint32_t)(sym_addr + (int64_t)f->addend);
        }
    }

    kfree(raw);

    struct UdllHandle *h = kmalloc(sizeof(struct UdllHandle));
    if (!h)
    {
        kfree(base);
        return 0;
    }
    h->base = base;
    h->total_size = alloc_size;
    h->exports = exp_copy;
    h->export_count = hdr.export_count;

    kprintf("udll: loaded %s (text=%u data=%u bss=%u reloc=%u fixup=%u exp=%u tramp=%u)\n",
            path, hdr.text_size, hdr.data_size, hdr.bss_size,
            hdr.reloc_count, hdr.fixup_count, hdr.export_count, n_call32);

    return h;
}

void *udll_get_proc(struct UdllHandle *h, const char *name)
{
    if (!h)
        return 0;
    uint32_t i;
    for (i = 0; i < h->export_count; i++)
    {
        if (name_eq(h->exports[i].name, name))
            return (void *)(h->base + h->exports[i].offset);
    }
    return 0;
}

void udll_unload(struct UdllHandle *h)
{
    if (!h)
        return;
    kfree(h->base);
    kfree(h);
}
