#ifndef UNI_GDT_H
#define UNI_GDT_H

#include <stdint.h>

#define GDT_SEL_KCODE 0x08
#define GDT_SEL_KDATA 0x10
#define GDT_SEL_TSS 0x18
#define GDT_SEL_UCODE 0x2B
#define GDT_SEL_UDATA 0x33

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);

#endif
