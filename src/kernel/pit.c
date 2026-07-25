#include "pit.h"
#include "io.h"
#include "idt.h"
#include "pic.h"

static volatile uint64_t ticks;

static void pit_irq(struct int_frame *f)
{
    (void)f;
    ticks++;
}

void pit_init(uint32_t hz)
{
    uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    irq_set_handler(0, pit_irq);
    pic_unmask(0);
}

uint64_t pit_ticks(void)
{
    return ticks;
}

void pit_tick_inc(void)
{
    ticks++;
}
