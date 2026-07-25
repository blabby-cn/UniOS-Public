#include "tooltip.h"
#include "wm.h"
#include "ui.h"

#define TT_DELAY 40
#define TT_OFF_X 14
#define TT_OFF_Y 20
#define TT_MARGIN 6
#define TT_H 20

static char g_buf[UI_TEXT_MAX];
static int32_t g_mx, g_my, g_lx, g_ly;
static uint64_t g_since;
static uint8_t g_shown, g_has;

static int tip_eq(const char *a, const char *b)
{
    for (int i = 0; i < UI_TEXT_MAX - 1; i++)
    {
        if (a[i] != b[i]) return 0;
        if (a[i] == 0) return 1;
    }
    return 1;
}

static void tip_cpy(char *dst, const char *src)
{
    int i = 0;
    for (; src[i] && i < UI_TEXT_MAX - 1; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

void tooltip_init(void)
{
    g_buf[0] = 0;
    g_shown = 0;
    g_has = 0;
    g_lx = -1;
    g_ly = -1;
}

void tooltip_set(const char *text, int32_t mx, int32_t my, uint64_t tick)
{
    g_mx = mx;
    g_my = my;
    if (!text || text[0] == 0)
    {
        if (g_shown)
        {
            g_shown = 0;
            wm_mark_dirty();
        }
        g_has = 0;
        return;
    }
    if (!g_has || !tip_eq(g_buf, text))
    {
        tip_cpy(g_buf, text);
        g_buf[UI_TEXT_MAX - 1] = 0;
        g_since = tick;
        g_has = 1;
        if (g_shown)
        {
            g_shown = 0;
            wm_mark_dirty();
        }
        return;
    }
    if (!g_shown)
    {
        if (tick - g_since >= TT_DELAY)
        {
            g_shown = 1;
            wm_mark_dirty();
        }
        return;
    }
    if (g_mx != g_lx || g_my != g_ly)
    {
        g_lx = g_mx;
        g_ly = g_my;
        wm_mark_dirty();
    }
}

void tooltip_paint(struct gfx_surface *s)
{
    if (!g_shown || g_buf[0] == 0)
        return;
    int32_t tw = gfx_text_width(g_buf);
    int32_t w = tw + TT_MARGIN * 2;
    int32_t x = g_mx + TT_OFF_X;
    int32_t y = g_my + TT_OFF_Y;
    if (x + w > (int32_t)s->w)
        x = g_mx - TT_OFF_X - w;
    if (y + TT_H > (int32_t)s->h)
        y = g_my - TT_OFF_Y - TT_H;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    gfx_fill(s, x, y, w, TT_H, 0x001A1A1A);
    gfx_rect(s, x, y, w, TT_H, 0x00555555);
    gfx_text(s, x + TT_MARGIN, y + 4, g_buf, 0x00E6E6E6);
}
