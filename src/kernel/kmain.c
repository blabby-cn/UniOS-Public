#include <stdint.h>

#include "multiboot2.h"
#include "serial.h"
#include "console.h"
#include "kprintf.h"
#include "fbdump.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "svg.h"
#include "pmm.h"
#include "vmm.h"
#include "kheap.h"
#include "sched.h"
#include "ata.h"
#include "vfs.h"
#include "pci.h"
#include "svga.h"
#include "mouse.h"
#include "gfx.h"
#include "ui.h"
#include "wm.h"
#include "desktop.h"

#define MB2_MAGIC 0x36D76289

extern uint8_t upk_calculator_start[];
extern uint8_t upk_calculator_end[];
extern uint8_t upk_notepad_start[];
extern uint8_t upk_notepad_end[];
extern uint8_t upk_terminal_start[];
extern uint8_t upk_terminal_end[];
extern uint8_t upk_clock_start[];
extern uint8_t upk_clock_end[];

static void halt(void)
{
    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

void *memset(void *dst, int c, unsigned long n);

static void ls_print(const struct vfs_stat *st, void *ctx)
{
    (void)ctx;
    if (st->type == VFS_DIR)
        kprintf("  [dir]  %s\n", st->name);
    else
        kprintf("  %6u  %s\n", st->size, st->name);
}

static char g_fsbuf[4096];

static void step8_fs_test(void)
{
    serial_write("DBG: step8_fs_test entry\n");
    console_set_color(0x00FFD27F, 0x00101828);
    kputs("Step 8: Filesystem (ATA PIO + FAT32 + VFS)\n");
    console_set_color(0x00FFFFFF, 0x00101828);

    int rc = vfs_init();
    if (rc)
    {
        kprintf("vfs_init FAILED rc=%d\n", rc);
        return;
    }
    kprintf("disk: %u sectors, FAT32 mounted\n", ata_sectors());
    kprintf("cluster size=%u total clusters=%u\n",
            fat32_cluster_size(), fat32_total_clusters());

    kputs("listing / :\n");
    vfs_list("/", ls_print, 0);
    kputs("listing /var :\n");
    vfs_list("/var", ls_print, 0);

    int64_t n = vfs_read("/etc/motd", g_fsbuf, sizeof(g_fsbuf) - 1);
    if (n > 0)
    {
        g_fsbuf[n] = 0;
        kprintf("read /etc/motd (%u bytes):\n%s", (uint32_t)n, g_fsbuf);
    }
    else
    {
        kputs("read /etc/motd FAILED\n");
    }

    n = vfs_read("/var/documents/welcome-to-unios.txt", g_fsbuf, sizeof(g_fsbuf) - 1);
    kprintf("read lfn file : %s (%u bytes)\n", n > 0 ? "OK" : "FAIL", (uint32_t)(n > 0 ? n : 0));

    static const char pattern[] = "UniOS kernel wrote this file in step 8. ";
    uint32_t plen = 0;
    while (pattern[plen])
        plen++;
    uint32_t total = 0;
    while (total + plen < 700)
    {
        for (uint32_t i = 0; i < plen; i++)
            g_fsbuf[total + i] = pattern[i];
        total += plen;
    }
    g_fsbuf[total++] = '\n';

    rc = vfs_write("/var/documents/step8.txt", g_fsbuf, total);
    kprintf("write step8.txt (%u bytes, 2 clusters): %s\n", total, rc == 0 ? "OK" : "FAIL");

    static char verify[1024];
    n = vfs_read("/var/documents/step8.txt", verify, sizeof(verify));
    int match = (n == (int64_t)total);
    if (match)
        for (uint32_t i = 0; i < total; i++)
            if (verify[i] != g_fsbuf[i])
            {
                match = 0;
                break;
            }
    kprintf("read-back verify: %s\n", match ? "OK" : "FAIL");

    rc = vfs_mkdir("/var/documents/kernel_dir");
    kprintf("mkdir kernel_dir: %s\n", rc == 0 ? "OK" : "FAIL");
    rc = vfs_write("/var/documents/kernel_dir/inner.txt", "created inside kernel_dir\n", 26);
    kprintf("write inner.txt : %s\n", rc == 0 ? "OK" : "FAIL");

    kputs("listing /var/documents :\n");
    vfs_list("/var/documents", ls_print, 0);

    kprintf("free clusters : %u\n", fat32_free_clusters());
    kputs("step8 filesystem test done\n");
}

static void task_red(void)
{
    for (;;)
    {
        struct task *self = sched_current();
        self->runs++;
        desktop_thread_runs[0] = self->runs;
        task_yield();
    }
}

static void task_green(void)
{
    for (;;)
    {
        struct task *self = sched_current();
        self->runs++;
        desktop_thread_runs[1] = self->runs;
        task_yield();
    }
}

static void task_blue(void)
{
    for (;;)
    {
        struct task *self = sched_current();
        self->runs++;
        desktop_thread_runs[2] = self->runs;
        task_yield();
    }
}

void kmain(uint32_t magic, uint32_t info_addr)
{
    serial_init();

    if (magic != MB2_MAGIC)
    {
        serial_write("FATAL: bad multiboot2 magic\n");
        halt();
    }

    struct mb2_fb *fb = mb2_find_fb(info_addr);
    if (fb == 0 || fb->bpp != 32)
    {
        serial_write("FATAL: no 32bpp linear framebuffer\n");
        halt();
    }

    uint64_t a_addr = fb->addr;
    uint32_t a_w = fb->width;
    uint32_t a_h = fb->height;
    uint32_t a_pitch = fb->pitch;

    console_init(a_addr, a_pitch, a_w, a_h, fb->bpp);
    fb_set_current(a_addr, a_pitch, a_w, a_h);
    kprintf("FBINFO %u %u %u %u\n", fb->width, fb->height, fb->pitch, (uint32_t)fb->bpp);

    struct svga_info si;
    if (svga_init(&si) == 0)
    {
        serial_write("SVGA II found\n");
        kprintf("svga io=0x%x fb_phys=0x%llx vram=%u fbsize=%u max=%ux%u id=0x%x\n",
                (uint32_t)si.io_base, (unsigned long long)si.fb_phys,
                si.vram_size, si.fb_size, si.max_width, si.max_height, si.id);
        struct svga_mode sm;
        uint32_t tw = 1280, th = 720;
        if (tw > si.max_width) tw = si.max_width;
        if (th > si.max_height) th = si.max_height;
        if (svga_set_mode(tw, th, 32, &sm) == 0 && sm.base && sm.pitch)
        {
            a_addr = sm.base;
            a_w = sm.width;
            a_h = sm.height;
            a_pitch = sm.pitch;
            console_init(a_addr, a_pitch, a_w, a_h, 32);
            fb_set_current(a_addr, a_pitch, a_w, a_h);
            kprintf("SVGA runtime mode set: %ux%u pitch=%u base=0x%llx (was %ux%u)\n",
                    a_w, a_h, a_pitch, (unsigned long long)a_addr,
                    fb->width, fb->height);
            if (svga_fifo_init() == 0)
                kprintf("SVGA FIFO enabled: caps=0x%x hw accel command queue active\n", svga_caps());
            else
                serial_write("SVGA FIFO init failed, updates fall back to direct fb\n");
        }
        else
        {
            serial_write("SVGA set_mode failed, keeping boot framebuffer\n");
        }
    }
    else
    {
        serial_write("SVGA II not present, keeping boot framebuffer\n");
    }

    pmm_init(info_addr);

    gdt_init();
    serial_write("GDT+TSS loaded\n");
    idt_init();
    serial_write("IDT loaded\n");
    pic_init();
    serial_write("PIC remapped 32-47\n");
    keyboard_init();
    serial_write("interrupts still masked during init tests\n");

    console_set_color(0x00E0E0FF, 0x00101828);
    kputs("UniOS 0.1.0  x86_64\n");

    uint64_t pt = 0, pu = 0, pf = 0;
    pmm_stats(&pt, &pu, &pf);
    console_set_color(0x00FFD27F, 0x00101828);
    kputs("Step 4: Physical Memory Manager\n");
    console_set_color(0x00FFFFFF, 0x00101828);
    kprintf("total pages : %u\n", (uint32_t)pt);
    kprintf("used  pages : %u\n", (uint32_t)pu);
    kprintf("free  pages : %u\n", (uint32_t)pf);
    void *pa = pmm_alloc();
    void *pb = pmm_alloc();
    kprintf("alloc -> 0x%x, 0x%x\n", (uint32_t)(unsigned long)pa, (uint32_t)(unsigned long)pb);
    pmm_stats(&pt, &pu, &pf);
    kprintf("after alloc : free=%u\n", (uint32_t)pf);
    pmm_free(pa);
    pmm_stats(&pt, &pu, &pf);
    kprintf("after free  : free=%u\n", (uint32_t)pf);

    console_set_color(0x00FFD27F, 0x00101828);
    kputs("Step 5: Paging + Kernel Heap\n");
    console_set_color(0x00FFFFFF, 0x00101828);

    vmm_init();
    uint64_t heap_base = 0xFFFF800000000000ull;
    kheap_init(heap_base, 64);
    kprintf("kheap base=0x%llx total=%u free=%u\n",
            (unsigned long long)heap_base, kheap_total(), kheap_free());

    void *ha = kmalloc(120);
    void *hb = kmalloc(200);
    void *hc = kmalloc(48);
    if (ha && hb && hc)
    {
        memset(ha, 0xAB, 120);
        memset(hb, 0xCD, 200);
        memset(hc, 0xEF, 48);
        int ok = 1;
        for (int i = 0; i < 120; i++) if (((uint8_t *)ha)[i] != 0xAB) ok = 0;
        for (int i = 0; i < 200; i++) if (((uint8_t *)hb)[i] != 0xCD) ok = 0;
        for (int i = 0; i < 48; i++) if (((uint8_t *)hc)[i] != 0xEF) ok = 0;
        kprintf("pattern check : %s\n", ok ? "OK" : "FAIL");
    }
    else
    {
        kputs("pattern check : FAIL (alloc null)\n");
    }
    kprintf("after 3 alloc : used=%u free=%u allocs=%u\n",
            kheap_used(), kheap_free(), kheap_allocs());

    kfree(ha);
    kfree(hc);
    kprintf("after free 2  : used=%u free=%u allocs=%u\n",
            kheap_used(), kheap_free(), kheap_allocs());

    void *hbig = kmalloc(70000);
    kprintf("big alloc 70000: %s free=%u\n",
            hbig ? "OK" : "FAIL", kheap_free());
    if (hbig)
    {
        memset(hbig, 0x55, 70000);
        int ok = 1;
        for (int i = 0; i < 70000; i++) if (((uint8_t *)hbig)[i] != 0x55) { ok = 0; break; }
        kprintf("big pattern   : %s\n", ok ? "OK" : "FAIL");
        kfree(hbig);
    }
    kprintf("after big free: free=%u\n", kheap_free());

    __asm__ volatile("sti");
    serial_write("interrupts enabled AFTER kheap tests\n");

    console_set_color(0x0080FF80, 0x00101828);
    kputs("Step 3.5: SVG vector graphics (nanosvg integrated, 6 icons embedded)\n");
    console_set_color(0x00FFFFFF, 0x00101828);

    step8_fs_test();

    console_set_color(0x00FFD27F, 0x00101828);
    kputs("Step 9: Input + Graphics mode (SVGA II + PS/2 mouse)\n");
    console_set_color(0x00FFFFFF, 0x00101828);
    mouse_init(a_w, a_h);
    kprintf("mouse initialized, cursor at %d,%d\n", mouse_x(), mouse_y());

    console_set_color(0x00FFD27F, 0x00101828);
    kputs("Step 6+7: Multitasking + User mode (ring3)\n");
    console_set_color(0x00FFFFFF, 0x00101828);

    sched_init();
    task_create_user_blob("calc", upk_calculator_start,
        (uint64_t)(upk_calculator_end - upk_calculator_start), 0x8000000000ULL);
    task_create_user_blob("clock", upk_clock_start,
        (uint64_t)(upk_clock_end - upk_clock_start), 0x8000030000ULL);
    task_create("red", task_red);
    task_create("green", task_green);
    task_create("blue", task_blue);

    kprintf("tasks=%u (idle + 2 ring3 apps + 3 kthreads)\n", sched_task_count());

    console_set_color(0x00FFD27F, 0x00101828);
    kputs("Step 10+11: Window system + Desktop\n");
    console_set_color(0x00FFFFFF, 0x00101828);

    if (wm_init(a_addr, a_pitch, a_w, a_h) != 0)
    {
        serial_write("FATAL: wm_init failed\n");
        halt();
    }
    if (desktop_init(a_w, a_h) != 0)
    {
        serial_write("FATAL: desktop_init failed\n");
        halt();
    }
    console_set_suppress(1);
    keyboard_set_sink(wm_inject_key);
    kprintf("wm ready: %u windows, backbuffer %ux%u, fifo=%s\n",
            wm_window_count(), a_w, a_h, svga_fifo_active() ? "on" : "off");

    pit_init(100);

    for (;;)
    {
        task_yield();

        desktop_tick(sched_ticks());
        if (!desktop_in_console())
            wm_composite();
    }
}
