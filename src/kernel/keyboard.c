#include "keyboard.h"
#include "io.h"
#include "idt.h"
#include "pic.h"
#include "kprintf.h"
#include "serial.h"

static const char map_lo[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
    0, 0, 0, 0, 0};

static const char map_hi[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
    0, 0, 0, 0, 0};

static int shift;
static int caps;
static int ctrl;
static int e0_flag;
static volatile uint64_t events;
static void (*g_sink)(char c);
static char g_last_key;

#define KB_BUF_SIZE 64
static char kb_buf[KB_BUF_SIZE];
static volatile uint32_t kb_head;
static volatile uint32_t kb_tail;

void keyboard_set_sink(void (*fn)(char c))
{
    g_sink = fn;
}

int keyboard_ctrl_pressed(void) { return ctrl; }

int keyboard_hotkey_ctrl_r(void)
{
    if (ctrl && (g_last_key == 'r' || g_last_key == 'R'))
    {
        g_last_key = 0;
        ctrl = 0;
        return 1;
    }
    return 0;
}

int keyboard_hotkey_ctrl_esc(void)
{
    if (ctrl && g_last_key == 27)
    {
        g_last_key = 0;
        ctrl = 0;
        return 1;
    }
    return 0;
}

int keyboard_hotkey_ctrl_s(void)
{
    if (ctrl && g_last_key == 19)
    {
        g_last_key = 0;
        ctrl = 0;
        return 1;
    }
    return 0;
}

static void kbd_irq(struct int_frame *f)
{
    (void)f;
    uint8_t st = inb(0x64);
    if (st & 0x20)
        return;
    uint8_t sc = inb(0x60);
    events++;

    if (sc == 0x2A || sc == 0x36)
    {
        shift = 1;
        return;
    }
    if (sc == 0xAA || sc == 0xB6)
    {
        shift = 0;
        return;
    }
    if (sc == 0x3A)
    {
        caps = !caps;
        return;
    }
    if (sc == 0x1D)
    {
        ctrl = 1;
        return;
    }
    if (sc == 0x9D)
    {
        ctrl = 0;
        return;
    }
    if (sc == 0xE0)
    {
        e0_flag = 1;
        return;
    }
    if (sc & 0x80)
    {
        return;
    }

    char c = 0;
    if (e0_flag)
    {
        e0_flag = 0;
        switch (sc)
        {
        case 0x48: c = (char)128; break;
        case 0x50: c = (char)129; break;
        case 0x4B: c = (char)130; break;
        case 0x4D: c = (char)131; break;
        case 0x47: c = (char)132; break;
        case 0x4F: c = (char)133; break;
        default: c = 0; break;
        }
    }
    else
    {
        c = shift ? map_hi[sc & 0x7F] : map_lo[sc & 0x7F];
        if (c >= 'a' && c <= 'z' && caps)
            c = (char)(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z' && caps && shift)
            c = (char)(c - 'A' + 'a');
    }
    if (c)
    {
        if (ctrl && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 1);
        g_last_key = c;
        if (g_sink)
        {
            g_sink(c);
        }
        {
            uint32_t next = (kb_head + 1) % KB_BUF_SIZE;
            if (next != kb_tail)
            {
                kb_buf[kb_head] = c;
                kb_head = next;
            }
        }
    }
}

void keyboard_init(void)
{
    while (inb(0x64) & 1)
    {
        (void)inb(0x60);
    }
    irq_set_handler(1, kbd_irq);
    pic_unmask(1);
}

uint64_t keyboard_event_count(void)
{
    return events;
}

int keyboard_poll_read(char *c)
{
    if (kb_head == kb_tail) return 0;
    *c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return 1;
}
