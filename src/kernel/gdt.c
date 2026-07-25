#include "gdt.h"

struct tss64
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;
} __attribute__((packed));

struct gdtr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static uint64_t gdt[8];
static struct tss64 tss;
static uint8_t kernel_stack0[16384] __attribute__((aligned(16)));
static uint8_t df_stack[8192] __attribute__((aligned(16)));

extern void gdt_flush(struct gdtr *gr);
extern void tss_flush(uint16_t sel);

void gdt_init(void)
{
    tss.rsp0 = (uint64_t)&kernel_stack0[sizeof(kernel_stack0)];
    tss.ist1 = (uint64_t)&df_stack[sizeof(df_stack)];
    tss.iopb = sizeof(struct tss64);

    gdt[0] = 0;
    gdt[1] = 0x00AF9A000000FFFFULL;
    gdt[2] = 0x00CF92000000FFFFULL;
    gdt[3] = 0;
    gdt[4] = 0;
    gdt[5] = 0x00AF9A000000FFFFULL | (3ULL << 45);
    gdt[6] = 0x00CF92000000FFFFULL | (3ULL << 45);
    gdt[7] = 0;

    uint64_t base = (uint64_t)&tss;
    uint32_t limit = sizeof(struct tss64) - 1;
    gdt[3] = (limit & 0xFFFFULL) | ((base & 0xFFFFFFULL) << 16) |
             (0x89ULL << 40) | (((limit >> 16) & 0xFULL) << 48) |
             (((base >> 24) & 0xFFULL) << 56);
    gdt[4] = (base >> 32) & 0xFFFFFFFFULL;

    struct gdtr gr;
    gr.limit = sizeof(gdt) - 1;
    gr.base = (uint64_t)gdt;
    gdt_flush(&gr);
    tss_flush(GDT_SEL_TSS);
}

void tss_set_rsp0(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}
