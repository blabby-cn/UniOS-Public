#include "kheap.h"
#include "pmm.h"
#include "vmm.h"
#include "util.h"

#include <stdint.h>

#define ALIGN 16
#define MIN_BLK 32
#define HDR_USED 1

static uint64_t g_base;
static uint32_t g_mapped_pages;
static uint32_t g_total;
static uint32_t g_used;
static uint32_t g_allocs;

static uint32_t align_up(uint32_t x, uint32_t a)
{
    return (x + (a - 1)) & ~(a - 1);
}

static uint32_t blk_size(uint32_t *hdr)
{
    return (*hdr) & ~0x7u;
}

static int blk_used(uint32_t *hdr)
{
    return (*hdr) & HDR_USED;
}

static uint32_t *next_blk(uint32_t *hdr)
{
    uint32_t s = blk_size(hdr);
    return (uint32_t *)((uint8_t *)hdr + s);
}

static void set_blk(uint32_t *hdr, uint32_t size, int used)
{
    *hdr = (size & ~0x7u) | (used ? HDR_USED : 0);
    uint32_t *f = (uint32_t *)((uint8_t *)hdr + size - 4);
    *f = *hdr;
}

static uint32_t *heap_end(void)
{
    return (uint32_t *)(g_base + g_total);
}

static void map_more(uint32_t pages)
{
    for (uint32_t i = 0; i < pages; i++)
    {
        uint64_t v = g_base + (uint64_t)g_mapped_pages * 4096;
        void *p = pmm_alloc();
        if (!p) return;
        vmm_map(v, (uint64_t)(uintptr_t)p, VMM_FLAG_RW | VMM_FLAG_PRESENT);
        g_mapped_pages++;
    }
    g_total = g_mapped_pages * 4096;
}

static void grow_heap(uint32_t min_bytes)
{
    uint32_t before = g_total;
    uint32_t pages = KHEAP_GROW_PAGES;
    uint32_t need_pages = (min_bytes + 4095) / 4096 + 1;
    if (need_pages > pages)
        pages = need_pages;
    map_more(pages);
    if (g_total == before) return;
    uint32_t nsz = g_total - before;
    uint32_t *nb = (uint32_t *)(g_base + before);
    set_blk(nb, nsz, 0);

    uint32_t *prev = 0;
    uint32_t *cur = (uint32_t *)g_base;
    while (cur < nb)
    {
        prev = cur;
        cur = next_blk(cur);
    }
    if (prev && cur == nb && !blk_used(prev))
    {
        uint32_t merged = blk_size(prev) + nsz;
        set_blk(prev, merged, 0);
    }
}

void kheap_init(uint64_t base, uint32_t initial_pages)
{
    g_base = base;
    g_mapped_pages = 0;
    g_total = 0;
    g_used = 0;
    g_allocs = 0;
    map_more(initial_pages);
    if (g_total == 0) return;
    set_blk((uint32_t *)g_base, g_total, 0);
}

void *kmalloc(uint32_t size)
{
    if (size == 0) return 0;
    uint32_t need = align_up(size, 8) + 12;
    uint32_t asz = align_up(need, ALIGN);
    if (asz < MIN_BLK) asz = MIN_BLK;
    uint32_t *end = heap_end();
    for (;;)
    {
        uint32_t *b = (uint32_t *)g_base;
        while (b < end)
        {
            uint32_t bs = blk_size(b);
            if (!blk_used(b) && bs >= asz)
            {
                if (bs >= asz + MIN_BLK)
                {
                    uint32_t *nb = (uint32_t *)((uint8_t *)b + asz);
                    set_blk(nb, bs - asz, 0);
                    set_blk(b, asz, 1);
                }
                else
                {
                    set_blk(b, bs, 1);
                }
                g_used += blk_size(b);
                g_allocs++;
                return (void *)((uint8_t *)b + 8);
            }
            b = next_blk(b);
        }
        uint32_t before = g_total;
        grow_heap(asz);
        if (g_total == before) return 0;
        end = heap_end();
    }
}

void kfree(void *ptr)
{
    if (!ptr) return;
    uint32_t *b = (uint32_t *)((uint8_t *)ptr - 8);
    if (!blk_used(b)) return;
    g_used -= blk_size(b);
    g_allocs--;
    set_blk(b, blk_size(b), 0);
    uint32_t *n = next_blk(b);
    if (n < heap_end() && !blk_used(n))
    {
        uint32_t s = blk_size(b) + blk_size(n);
        set_blk(b, s, 0);
    }
    uint32_t *prev = 0;
    uint32_t *cur = (uint32_t *)g_base;
    while (cur < b)
    {
        prev = cur;
        cur = next_blk(cur);
    }
    if (prev && !blk_used(prev))
    {
        uint32_t s = blk_size(prev) + blk_size(b);
        set_blk(prev, s, 0);
    }
}

void *krealloc(void *ptr, uint32_t size)
{
    if (!ptr) return kmalloc(size);
    if (size == 0) { kfree(ptr); return 0; }
    uint32_t *b = (uint32_t *)((uint8_t *)ptr - 8);
    uint32_t old_size = blk_size(b) - 8;
    if (old_size >= size) return ptr;
    void *new_ptr = kmalloc(size);
    if (!new_ptr) return 0;
    memcpy(new_ptr, ptr, old_size);
    kfree(ptr);
    return new_ptr;
}

uint32_t kheap_total(void)
{
    return g_total;
}

uint32_t kheap_used(void)
{
    return g_used;
}

uint32_t kheap_free(void)
{
    return g_total - g_used;
}

uint32_t kheap_allocs(void)
{
    return g_allocs;
}
