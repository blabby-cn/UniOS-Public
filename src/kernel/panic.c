#include "panic.h"
#include "console.h"
#include "kprintf.h"
#include "serial.h"
#include "fbdump.h"

static const char *exc_names[32] = {
    "Divide Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Fault", "General Protection", "Page Fault", "Reserved",
    "x87 FP Error", "Alignment Check", "Machine Check", "SIMD FP",
    "Virtualization", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Security", "Reserved"};

void panic_exception(struct int_frame *f)
{
    __asm__ volatile("cli");

    uint64_t cr2, cr3;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    console_set_color(0x00FFFFFF, 0x00600000);
    console_clear();
    console_set_color(0x00FFD0D0, 0x00600000);
    kputs("\n  KERNEL PANIC\n\n");
    console_set_color(0x00FFFFFF, 0x00600000);
    kprintf("  exception %u: %s\n", (uint32_t)f->vector, exc_names[f->vector & 31]);
    kprintf("  error code: 0x%x\n\n", (uint32_t)f->error);
    kprintf("  rip=%p  cs=0x%x  rflags=%p\n", (void *)f->rip, (uint32_t)f->cs, (void *)f->rflags);
    kprintf("  rsp=%p  ss=0x%x\n\n", (void *)f->rsp, (uint32_t)f->ss);
    kprintf("  rax=%p rbx=%p rcx=%p\n", (void *)f->rax, (void *)f->rbx, (void *)f->rcx);
    kprintf("  rdx=%p rsi=%p rdi=%p\n", (void *)f->rdx, (void *)f->rsi, (void *)f->rdi);
    kprintf("  rbp=%p r8 =%p r9 =%p\n", (void *)f->rbp, (void *)f->r8, (void *)f->r9);
    kprintf("  r10=%p r11=%p r12=%p\n", (void *)f->r10, (void *)f->r11, (void *)f->r12);
    kprintf("  r13=%p r14=%p r15=%p\n\n", (void *)f->r13, (void *)f->r14, (void *)f->r15);
    kprintf("  cr2=%p cr3=%p\n\n", (void *)cr2, (void *)cr3);
    kputs("  system halted\n");

    serial_write("PANIC fb dump begin\n");
    fb_dump_current();
    serial_write("PANIC fb dump done\n");

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}
