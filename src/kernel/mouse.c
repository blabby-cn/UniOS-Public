#include "mouse.h"
#include "io.h"
#include "idt.h"
#include "pic.h"
#include "kprintf.h"
#include "serial.h"

#define PS2_DATA 0x60
#define PS2_CMD 0x64

static int32_t g_x;
static int32_t g_y;
static uint8_t g_buttons;
static volatile uint64_t g_events;
static uint32_t g_w;
static uint32_t g_h;

static uint8_t g_packet[4];
static uint8_t g_cycle;
static uint8_t g_wheel_en;
static int32_t g_wheel;
static uint8_t g_sync_drops;

static void ps2_wait_in(void)
{
    for (uint32_t i = 0; i < 100000; i++)
        if ((inb(PS2_CMD) & 2) == 0)
            return;
}

static void ps2_wait_out(void)
{
    for (uint32_t i = 0; i < 100000; i++)
        if ((inb(PS2_CMD) & 1) != 0)
            return;
}

static void mouse_cmd(uint8_t cmd)
{
    ps2_wait_in();
    outb(PS2_CMD, 0xD4);
    ps2_wait_in();
    outb(PS2_DATA, cmd);
    ps2_wait_out();
    (void)inb(PS2_DATA);
}

void mouse_resync(void)
{
    g_cycle = 0;
    g_sync_drops = 0;
    mouse_cmd(0xF5);
    mouse_cmd(0xF4);
    kprintf("mouse: stream reset to resync\n");
}

static void mouse_irq(struct int_frame *f)
{
    (void)f;
    uint8_t st = inb(PS2_CMD);
    if ((st & 0x21) != 0x21)
        return;
    uint8_t data = inb(PS2_DATA);

    if (g_cycle == 0 && (data & 0x08) == 0)
    {
        if (g_sync_drops < 255)
            g_sync_drops++;
        if (g_sync_drops >= 2)
            mouse_resync();
        return;
    }
    g_sync_drops = 0;

    g_packet[g_cycle] = data;
    g_cycle++;
    uint8_t need = g_wheel_en ? 4 : 3;
    if (g_cycle < need)
        return;
    g_cycle = 0;

    uint8_t flags = g_packet[0];
    if (flags & 0xC0)
        return;

    int32_t dx = (int32_t)g_packet[1] - (int32_t)((flags << 4) & 0x100);
    int32_t dy = (int32_t)g_packet[2] - (int32_t)((flags << 3) & 0x100);

    g_x += dx;
    g_y -= dy;
    if (g_x < 0)
        g_x = 0;
    if (g_y < 0)
        g_y = 0;
    if (g_x >= (int32_t)g_w)
        g_x = (int32_t)g_w - 1;
    if (g_y >= (int32_t)g_h)
        g_y = (int32_t)g_h - 1;

    g_buttons = flags & 7;
    g_events++;

    if (g_wheel_en)
    {
        int8_t dz = (int8_t)g_packet[3];
        g_wheel += dz;
    }

    if ((g_events & 31) == 0)
        kprintf("mouse x=%d y=%d btn=%u events=%u wheel=%d\n",
                g_x, g_y, (uint32_t)g_buttons, (uint32_t)g_events, (int)g_wheel);
}

void mouse_init(uint32_t screen_w, uint32_t screen_h)
{
    g_w = screen_w;
    g_h = screen_h;
    g_x = (int32_t)(screen_w / 2);
    g_y = (int32_t)(screen_h / 2);

    ps2_wait_in();
    outb(PS2_CMD, 0xA8);

    ps2_wait_in();
    outb(PS2_CMD, 0x20);
    ps2_wait_out();
    uint8_t cfg = inb(PS2_DATA);
    cfg |= 2;
    cfg &= (uint8_t)~0x20;
    ps2_wait_in();
    outb(PS2_CMD, 0x60);
    ps2_wait_in();
    outb(PS2_DATA, cfg);

    mouse_cmd(0xF6);
    mouse_cmd(0xF4);

    g_wheel_en = 0;
    g_wheel = 0;
    mouse_cmd(0xF3);
    mouse_cmd(200);
    mouse_cmd(0xF3);
    mouse_cmd(100);
    mouse_cmd(0xF3);
    mouse_cmd(80);
    ps2_wait_in();
    outb(PS2_CMD, 0xD4);
    ps2_wait_in();
    outb(PS2_DATA, 0xF2);
    ps2_wait_out();
    (void)inb(PS2_DATA);
    ps2_wait_out();
    uint8_t id = inb(PS2_DATA);
    if (id == 3 || id == 4)
    {
        g_wheel_en = 1;
        kprintf("mouse: wheel enabled (id=%u)\n", (uint32_t)id);
    }
    else
    {
        kprintf("mouse: wheel not supported (id=%u), 3-byte packet\n", (uint32_t)id);
    }
    mouse_cmd(0xF4);

    irq_set_handler(12, mouse_irq);
    pic_unmask(12);
}

int32_t mouse_wheel(void)
{
    int32_t v = g_wheel;
    g_wheel = 0;
    return v;
}

int32_t mouse_x(void)
{
    return g_x;
}

int32_t mouse_y(void)
{
    return g_y;
}

uint8_t mouse_buttons(void)
{
    return g_buttons;
}

uint64_t mouse_event_count(void)
{
    return g_events;
}
