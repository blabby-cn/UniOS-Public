#include "gfx.h"
#include "font.h"

#define GLYPH_H 16

static inline void px(struct gfx_surface *s, int32_t x, int32_t y, uint32_t c)
{
    if (x < 0 || y < 0 || x >= (int32_t)s->w || y >= (int32_t)s->h)
        return;
    *(uint32_t *)(s->base + (uint64_t)y * s->pitch + (uint64_t)x * 4) = c;
}

static inline uint32_t blend(uint32_t dst, uint32_t src, uint8_t a)
{
    uint32_t rb = ((src & 0x00FF00FF) * a + (dst & 0x00FF00FF) * (255 - a)) >> 8;
    uint32_t g = ((src & 0x0000FF00) * a + (dst & 0x0000FF00) * (255 - a)) >> 8;
    return (rb & 0x00FF00FF) | (g & 0x0000FF00);
}

void gfx_fill(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = x + w;
    int32_t y1 = y + h;
    if (x1 > (int32_t)s->w)
        x1 = (int32_t)s->w;
    if (y1 > (int32_t)s->h)
        y1 = (int32_t)s->h;
    for (int32_t yy = y0; yy < y1; yy++)
    {
        uint32_t *row = (uint32_t *)(s->base + (uint64_t)yy * s->pitch);
        for (int32_t xx = x0; xx < x1; xx++)
            row[xx] = color;
    }
}

void gfx_fill_blend(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, uint8_t alpha)
{
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = x + w;
    int32_t y1 = y + h;
    if (x1 > (int32_t)s->w)
        x1 = (int32_t)s->w;
    if (y1 > (int32_t)s->h)
        y1 = (int32_t)s->h;
    for (int32_t yy = y0; yy < y1; yy++)
    {
        uint32_t *row = (uint32_t *)(s->base + (uint64_t)yy * s->pitch);
        for (int32_t xx = x0; xx < x1; xx++)
            row[xx] = blend(row[xx], color, alpha);
    }
}

void gfx_rect(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
    gfx_fill(s, x, y, w, 1, color);
    gfx_fill(s, x, y + h - 1, w, 1, color);
    gfx_fill(s, x, y, 1, h, color);
    gfx_fill(s, x + w - 1, y, 1, h, color);
}

static inline uint32_t lerp_c(uint32_t c0, uint32_t c1, uint32_t t, uint32_t tmax)
{
    uint32_t r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
    uint32_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint32_t r = r0 + (r1 - r0) * t / tmax;
    uint32_t g = g0 + (g1 - g0) * t / tmax;
    uint32_t b = b0 + (b1 - b0) * t / tmax;
    return (r << 16) | (g << 8) | b;
}

void gfx_hgrad(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c0, uint32_t c1)
{
    if (w <= 1)
    {
        gfx_fill(s, x, y, w, h, c0);
        return;
    }
    for (int32_t i = 0; i < w; i++)
        gfx_fill(s, x + i, y, 1, h, lerp_c(c0, c1, (uint32_t)i, (uint32_t)(w - 1)));
}

void gfx_vgrad(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c0, uint32_t c1)
{
    if (h <= 1)
    {
        gfx_fill(s, x, y, w, h, c0);
        return;
    }
    for (int32_t i = 0; i < h; i++)
        gfx_fill(s, x, y + i, w, 1, lerp_c(c0, c1, (uint32_t)i, (uint32_t)(h - 1)));
}

void gfx_round_fill(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color)
{
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;
    for (int32_t yy = 0; yy < h; yy++)
    {
        int32_t dx = 0;
        if (yy < r)
        {
            int32_t dy = r - yy;
            int32_t lim = r * r - dy * dy;
            int32_t q = 0;
            while ((q + 1) * (q + 1) <= lim)
                q++;
            dx = r - q;
        }
        else if (yy >= h - r)
        {
            int32_t dy = yy - (h - r - 1);
            int32_t lim = r * r - dy * dy;
            int32_t q = 0;
            while ((q + 1) * (q + 1) <= lim)
                q++;
            dx = r - q;
        }
        gfx_fill(s, x + dx, y + yy, w - dx * 2, 1, color);
    }
}

void gfx_disc(struct gfx_surface *s, int32_t cx, int32_t cy, int32_t r, uint32_t color)
{
    for (int32_t yy = -r; yy <= r; yy++)
        for (int32_t xx = -r; xx <= r; xx++)
            if (xx * xx + yy * yy <= r * r)
                px(s, cx + xx, cy + yy, color);
}

static uint32_t decode_utf8(const uint8_t **ps)
{
    const uint8_t *s = *ps;
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
    *ps = s;
    return cp;
}

void gfx_text(struct gfx_surface *s, int32_t x, int32_t y, const char *utf8, uint32_t color)
{
    const uint8_t *p = (const uint8_t *)utf8;
    int32_t cx = x;
    while (*p)
    {
        uint32_t cp = decode_utf8(&p);
        int w;
        const uint8_t *g = font_glyph(cp, &w);
        for (int row = 0; row < GLYPH_H; row++)
        {
            uint8_t b0 = g[row * 2];
            uint8_t b1 = g[row * 2 + 1];
            for (int col = 0; col < w; col++)
            {
                uint8_t bit = col < 8 ? (b0 >> (7 - col)) & 1 : (b1 >> (7 - (col - 8))) & 1;
                if (bit)
                    px(s, cx + col, y + row, color);
            }
        }
        cx += w;
    }
}

int32_t gfx_text_width(const char *utf8)
{
    const uint8_t *p = (const uint8_t *)utf8;
    int32_t total = 0;
    while (*p)
    {
        uint32_t cp = decode_utf8(&p);
        int w;
        font_glyph(cp, &w);
        total += w;
    }
    return total;
}

void gfx_blit(struct gfx_surface *dst, int32_t dx, int32_t dy, const struct gfx_surface *src, int32_t sx, int32_t sy, int32_t w, int32_t h)
{
    for (int32_t yy = 0; yy < h; yy++)
    {
        int32_t ty = dy + yy;
        int32_t fy = sy + yy;
        if (ty < 0 || fy < 0 || ty >= (int32_t)dst->h || fy >= (int32_t)src->h)
            continue;
        for (int32_t xx = 0; xx < w; xx++)
        {
            int32_t tx = dx + xx;
            int32_t fx = sx + xx;
            if (tx < 0 || fx < 0 || tx >= (int32_t)dst->w || fx >= (int32_t)src->w)
                continue;
            *(uint32_t *)(dst->base + (uint64_t)ty * dst->pitch + (uint64_t)tx * 4) =
                *(uint32_t *)(src->base + (uint64_t)fy * src->pitch + (uint64_t)fx * 4);
        }
    }
}
