#include "fbdump.h"
#include "serial.h"

static uint64_t cur_addr;
static uint32_t cur_pitch;
static uint32_t cur_width;
static uint32_t cur_height;

void fb_dump(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height)
{
    cur_addr = fb_addr;
    cur_pitch = pitch;
    cur_width = width;
    cur_height = height;

    serial2_init();
    const volatile uint8_t *fb = (const volatile uint8_t *)(unsigned long)fb_addr;
    for (uint32_t y = 0; y < height; y++)
    {
        const volatile uint8_t *row = fb + (unsigned long)y * pitch;
        for (uint32_t x = 0; x < width; x++)
        {
            serial2_putb(row[x * 4 + 0]);
            serial2_putb(row[x * 4 + 1]);
            serial2_putb(row[x * 4 + 2]);
            serial2_putb(row[x * 4 + 3]);
        }
    }
}

void fb_set_current(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height)
{
    cur_addr = fb_addr;
    cur_pitch = pitch;
    cur_width = width;
    cur_height = height;
}

void fb_dump_current(void)
{
    if (cur_addr)
    {
        fb_dump(cur_addr, cur_pitch, cur_width, cur_height);
    }
}

uint64_t fb_get_addr(void) { return cur_addr; }
uint32_t fb_get_pitch(void) { return cur_pitch; }
uint32_t fb_get_width(void) { return cur_width; }
uint32_t fb_get_height(void) { return cur_height; }
