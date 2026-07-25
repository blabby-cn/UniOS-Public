#include "pmm.h"
#include "multiboot2.h"
#include "serial.h"
#include "kprintf.h"

#include <stdint.h>

#define PMM_PAGE_SIZE 4096
#define PMM_PAGE_MASK ((uint64_t)PMM_PAGE_SIZE - 1)
#define PMM_MAX_ADDR (4ULL * 1024 * 1024 * 1024)

static uint32_t *g_bitmap = 0;
static uint32_t g_bm_words = 0;
static uint32_t g_total_pages = 0;
static uint32_t g_used_pages = 0;

extern uint8_t _end[];

static inline uint32_t page_of(uint64_t addr)
{
    return (uint32_t)(addr / PMM_PAGE_SIZE);
}

static void bm_set(uint32_t i)
{
    g_bitmap[i >> 5] |= (1u << (i & 31u));
}

static void bm_clr(uint32_t i)
{
    g_bitmap[i >> 5] &= ~(1u << (i & 31u));
}

static int bm_tst(uint32_t i)
{
    return (g_bitmap[i >> 5] >> (i & 31u)) & 1u;
}

static void free_range(uint64_t base, uint64_t len)
{
    uint32_t p0 = page_of(base);
    uint32_t p1 = page_of(base + len);
    for (uint32_t p = p0; p < p1 && p < g_total_pages; p++) bm_clr(p);
}

static void used_range(uint64_t base, uint64_t len)
{
    uint32_t p0 = page_of(base);
    uint32_t p1 = page_of(base + len);
    for (uint32_t p = p0; p < p1 && p < g_total_pages; p++) bm_set(p);
}

void pmm_init(uint32_t info_addr)
{
    struct mb2_mmap *mm = mb2_find_mmap(info_addr);
    if (mm == 0)
    {
        serial_write("PMM: FATAL no mmap tag\n");
        return;
    }
    struct mb2_fb *fb = mb2_find_fb(info_addr);
    uint64_t fb_addr = fb ? fb->addr : 0;
    uint64_t fb_size = fb ? (uint64_t)fb->height * fb->pitch : 0;
    uint64_t fb_lo = fb_addr;
    uint64_t fb_hi = fb_addr + fb_size;

    uint64_t max_addr = 0;
    uint64_t best_base = 0;
    uint64_t best_len = 0;
    uint8_t *e = (uint8_t *)(mm + 1);
    uint8_t *mm_end = (uint8_t *)mm + mm->size;
    while (e + 24 <= mm_end && mm->entry_size != 0)
    {
        struct mb2_mmap_entry *ent = (struct mb2_mmap_entry *)e;
        if (ent->mtype == 1)
        {
            uint64_t top = ent->base + ent->length;
            if (top > max_addr && top <= PMM_MAX_ADDR)
            {
                max_addr = top;
            }
            if (ent->length > best_len)
            {
                best_len = ent->length;
                best_base = ent->base;
            }
        }
        e += mm->entry_size;
    }
    if (max_addr == 0)
    {
        max_addr = PMM_MAX_ADDR;
    }
    if (best_len == 0)
    {
        serial_write("PMM: FATAL no available memory\n");
        return;
    }

    uint64_t nbits = max_addr / PMM_PAGE_SIZE;
    uint64_t bm_bytes = (nbits + 7) / 8;
    uint64_t bm_phys = best_base + best_len - bm_bytes;
    bm_phys &= ~PMM_PAGE_MASK;
    if (bm_phys < best_base)
    {
        bm_phys = best_base;
    }
    g_bitmap = (uint32_t *)(unsigned long)bm_phys;
    g_bm_words = (uint32_t)((bm_bytes + 3) / 4);
    g_total_pages = (uint32_t)nbits;

    for (uint32_t i = 0; i < g_bm_words; i++)
    {
        g_bitmap[i] = 0xFFFFFFFFu;
    }

    e = (uint8_t *)(mm + 1);
    while (e + 24 <= mm_end && mm->entry_size != 0)
    {
        struct mb2_mmap_entry *ent = (struct mb2_mmap_entry *)e;
        if (ent->mtype == 1)
        {
            uint64_t b = ent->base;
            uint64_t l = ent->length;
            if (b < PMM_MAX_ADDR)
            {
                if (b + l > PMM_MAX_ADDR)
                {
                    l = PMM_MAX_ADDR - b;
                }
                uint64_t s = b;
                uint64_t en = b + l;
                if (fb_size && s < fb_hi && en > fb_lo)
                {
                    if (s < fb_lo) free_range(s, fb_lo - s);
                    if (en > fb_hi) free_range(fb_hi, en - fb_hi);
                }
                else
                {
                    free_range(s, l);
                }
            }
        }
        e += mm->entry_size;
    }

    uint64_t kend = ((uint64_t)(unsigned long)_end + PMM_PAGE_MASK) & ~PMM_PAGE_MASK;
    used_range(0, kend);
    if (fb_size) used_range(fb_lo, fb_size);
    used_range(bm_phys, bm_bytes);

    g_used_pages = 0;
    for (uint32_t p = 0; p < g_total_pages; p++)
    {
        if (bm_tst(p))
        {
            g_used_pages++;
        }
    }

    serial_write("PMM: init ok\n");
    kprintf("PMM: pages=%u used=%u free=%u bitmap=0x%x\n",
            g_total_pages, g_used_pages, g_total_pages - g_used_pages,
            (uint32_t)bm_phys);
}

void *pmm_alloc(void)
{
    for (uint32_t p = 0; p < g_total_pages; p++)
    {
        if (!bm_tst(p))
        {
            bm_set(p);
            g_used_pages++;
            return (void *)(unsigned long)(p * PMM_PAGE_SIZE);
        }
    }
    return (void *)0;
}

void pmm_free(void *p)
{
    uint32_t idx = page_of((uint64_t)(unsigned long)p);
    if (idx < g_total_pages && bm_tst(idx))
    {
        bm_clr(idx);
        g_used_pages--;
    }
}

void pmm_stats(uint64_t *total, uint64_t *used, uint64_t *free)
{
    *total = g_total_pages;
    *used = g_used_pages;
    *free = g_total_pages - g_used_pages;
}
