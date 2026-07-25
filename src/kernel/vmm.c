#include "vmm.h"
#include "pmm.h"

#define PTE_P 0x1
#define PTE_RW 0x2
#define PTE_U 0x4
#define PTE_NX 0x8000000000000000ULL

static uint64_t *g_pml4;

static uint64_t *table_get(uint64_t *tbl, uint32_t idx, uint64_t flags)
{
    uint64_t e = tbl[idx];
    if (!(e & PTE_P))
    {
        void *p = pmm_alloc();
        if (!p) return 0;
        uint64_t pa = (uint64_t)(unsigned long)p;
        tbl[idx] = pa | PTE_P | PTE_RW | (flags & PTE_U);
        uint64_t *np = (uint64_t *)pa;
        for (uint32_t i = 0; i < 512; i++) np[i] = 0;
        return np;
    }
    return (uint64_t *)(e & 0xFFFFFFFFFFFFF000ULL);
}

void vmm_init(void)
{
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    g_pml4 = (uint64_t *)(cr3 & 0xFFFFFFFFFFFFF000ULL);
}

uint64_t vmm_pml4(void)
{
    return (uint64_t)(unsigned long)g_pml4;
}

void vmm_map(uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    uint32_t i1 = (uint32_t)((vaddr >> 39) & 0x1FF);
    uint32_t i2 = (uint32_t)((vaddr >> 30) & 0x1FF);
    uint32_t i3 = (uint32_t)((vaddr >> 21) & 0x1FF);
    uint32_t i4 = (uint32_t)((vaddr >> 12) & 0x1FF);
    uint64_t f = (flags & (PTE_U | PTE_NX)) | PTE_P | PTE_RW;
    uint64_t *pdpt = table_get(g_pml4, i1, f);
    if (!pdpt) return;
    uint64_t *pd = table_get(pdpt, i2, f);
    if (!pd) return;
    uint64_t *pt = table_get(pd, i3, f);
    if (!pt) return;
    pt[i4] = (paddr & 0xFFFFFFFFFFFFF000ULL) | f;
    __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

void vmm_unmap(uint64_t vaddr)
{
    uint32_t i1 = (uint32_t)((vaddr >> 39) & 0x1FF);
    uint32_t i2 = (uint32_t)((vaddr >> 30) & 0x1FF);
    uint32_t i3 = (uint32_t)((vaddr >> 21) & 0x1FF);
    uint32_t i4 = (uint32_t)((vaddr >> 12) & 0x1FF);
    if (!(g_pml4[i1] & PTE_P)) return;
    uint64_t *pdpt = (uint64_t *)(g_pml4[i1] & 0xFFFFFFFFFFFFF000ULL);
    if (!(pdpt[i2] & PTE_P)) return;
    uint64_t *pd = (uint64_t *)(pdpt[i2] & 0xFFFFFFFFFFFFF000ULL);
    if (!(pd[i3] & PTE_P)) return;
    uint64_t *pt = (uint64_t *)(pd[i3] & 0xFFFFFFFFFFFFF000ULL);
    pt[i4] = 0;
    __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

uint64_t vmm_pte(uint64_t vaddr)
{
    uint32_t i1 = (uint32_t)((vaddr >> 39) & 0x1FF);
    uint32_t i2 = (uint32_t)((vaddr >> 30) & 0x1FF);
    uint32_t i3 = (uint32_t)((vaddr >> 21) & 0x1FF);
    uint32_t i4 = (uint32_t)((vaddr >> 12) & 0x1FF);
    if (!(g_pml4[i1] & PTE_P))
        return 0;
    uint64_t *pdpt = (uint64_t *)(g_pml4[i1] & 0xFFFFFFFFFFFFF000ULL);
    if (!(pdpt[i2] & PTE_P))
        return 0;
    uint64_t *pd = (uint64_t *)(pdpt[i2] & 0xFFFFFFFFFFFFF000ULL);
    if (!(pd[i3] & PTE_P))
        return 0;
    uint64_t *pt = (uint64_t *)(pd[i3] & 0xFFFFFFFFFFFFF000ULL);
    return pt[i4];
}
