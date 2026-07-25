#include "console.h"
#include "font.h"

void *memmove(void *dst, const void *src, unsigned long n);

#define GLYPH_H 16

static volatile uint8_t *g_fb;
static uint32_t g_pitch;
static uint32_t g_w;
static uint32_t g_h;
static uint32_t g_x;
static uint32_t g_y;
static uint32_t g_fg = 0x00FFFFFF;
static uint32_t g_bg = 0x00000000;
static int g_suppress;

static inline void put_px(uint32_t x, uint32_t y, uint32_t c)
{
    *(volatile uint32_t *)(g_fb + (unsigned long)y * g_pitch + (unsigned long)x * 4) = c;
}

void console_init(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp)
{
    (void)bpp;
    g_fb = (volatile uint8_t *)(unsigned long)fb_addr;
    g_pitch = pitch;
    g_w = width;
    g_h = height;
    g_x = 0;
    g_y = 0;
    console_clear();
}

void console_set_color(uint32_t fg, uint32_t bg)
{
    g_fg = fg;
    g_bg = bg;
}

void console_clear(void)
{
    for (uint32_t y = 0; y < g_h; y++)
    {
        for (uint32_t x = 0; x < g_w; x++)
        {
            put_px(x, y, g_bg);
        }
    }
    g_x = 0;
    g_y = 0;
}

static void scroll(void)
{
    unsigned long shift = (unsigned long)GLYPH_H * g_pitch;
    unsigned long total = (unsigned long)g_h * g_pitch;
    memmove((void *)g_fb, (void *)(g_fb + shift), total - shift);
    for (uint32_t y = g_h - GLYPH_H; y < g_h; y++)
    {
        for (uint32_t x = 0; x < g_w; x++)
        {
            put_px(x, y, g_bg);
        }
    }
}

static void newline(void)
{
    g_x = 0;
    g_y += GLYPH_H;
    if (g_y + GLYPH_H > g_h)
    {
        scroll();
        g_y -= GLYPH_H;
    }
}

static void put_cp(uint32_t cp)
{
    if (cp == '\n')
    {
        newline();
        return;
    }
    if (cp == '\r')
    {
        g_x = 0;
        return;
    }

    int w;
    const uint8_t *g = font_glyph(cp, &w);
    if (g_x + (uint32_t)w > g_w)
    {
        newline();
    }
    for (int row = 0; row < GLYPH_H; row++)
    {
        uint8_t b0 = g[row * 2];
        uint8_t b1 = g[row * 2 + 1];
        for (int col = 0; col < w; col++)
        {
            uint8_t bit;
            if (col < 8)
            {
                bit = (b0 >> (7 - col)) & 1;
            }
            else
            {
                bit = (b1 >> (7 - (col - 8))) & 1;
            }
            put_px(g_x + (uint32_t)col, g_y + (uint32_t)row, bit ? g_fg : g_bg);
        }
    }
    g_x += (uint32_t)w;
}

void console_set_suppress(int on)
{
    g_suppress = on ? 1 : 0;
}

void console_write(const char *utf8)
{
    if (g_suppress)
        return;
    const uint8_t *s = (const uint8_t *)utf8;
    while (*s)
    {
        uint32_t cp;
        uint8_t c = *s;
        if (c < 0x80)
        {
            cp = c;
            s += 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
            s += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (uint32_t)(s[2] & 0x3F);
            s += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) | ((uint32_t)(s[2] & 0x3F) << 6) | (uint32_t)(s[3] & 0x3F);
            s += 4;
        }
        else
        {
            cp = '?';
            s += 1;
        }
        put_cp(cp);
    }
}

void console_backspace(void)
{
    if (g_suppress)
        return;
    if (g_x >= 8)
    {
        g_x -= 8;
    }
    else if (g_y >= GLYPH_H)
    {
        g_y -= GLYPH_H;
        g_x = g_w - 8;
    }
    else
    {
        return;
    }
    for (uint32_t row = 0; row < GLYPH_H; row++)
        for (uint32_t col = 0; col < 8; col++)
            put_px(g_x + col, g_y + row, g_bg);
}
