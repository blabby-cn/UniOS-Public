#include "idt.h"
#include "gdt.h"
#include "pic.h"
#include "panic.h"
#include "kprintf.h"
#include "serial.h"

struct idt_entry
{
    uint16_t off_low;
    uint16_t sel;
    uint8_t ist;
    uint8_t flags;
    uint16_t off_mid;
    uint32_t off_high;
    uint32_t reserved;
} __attribute__((packed));

struct idtr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static irq_handler_t irq_handlers[16];

extern uint64_t isr_stub_table[48];
extern void isr_timer(void);
extern void isr_yield(void);
extern void isr_syscall(void);
extern void idt_flush(struct idtr *ir);

static void idt_set(uint8_t vec, uint64_t handler, uint8_t ist, uint8_t dpl)
{
    idt[vec].off_low = handler & 0xFFFF;
    idt[vec].sel = GDT_SEL_KCODE;
    idt[vec].ist = ist;
    idt[vec].flags = (uint8_t)(0x8E | ((dpl & 3) << 5));
    idt[vec].off_mid = (handler >> 16) & 0xFFFF;
    idt[vec].off_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vec].reserved = 0;
}

void idt_init(void)
{
    for (int i = 0; i < 48; i++)
    {
        idt_set((uint8_t)i, isr_stub_table[i], i == 8 ? 1 : 0, 0);
    }

    struct idtr ir;
    ir.limit = sizeof(idt) - 1;
    ir.base = (uint64_t)idt;
    idt_flush(&ir);

    idt_set(32, (uint64_t)isr_timer, 0, 0);
    idt_set(0x40, (uint64_t)isr_yield, 0, 0);
    idt_set(0x80, (uint64_t)isr_syscall, 0, 3);
}

void irq_set_handler(uint8_t irq, irq_handler_t h)
{
    if (irq < 16)
    {
        irq_handlers[irq] = h;
    }
}

void isr_dispatch(struct int_frame *f)
{
    if (f->vector < 32)
    {
        if (f->vector == 3)
        {
            kprintf("EXC 3 breakpoint at rip=%p, resuming\n", (void *)f->rip);
            return;
        }
        panic_exception(f);
    }

    uint8_t irq = (uint8_t)(f->vector - 32);
    if (irq_handlers[irq])
    {
        irq_handlers[irq](f);
    }
    pic_eoi(irq);
}
