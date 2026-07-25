#ifndef UNI_GFX_H
#define UNI_GFX_H

#include <stdint.h>

struct gfx_surface
{
    uint8_t *base;
    uint32_t w;
    uint32_t h;
    uint32_t pitch;
};

void gfx_fill(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void gfx_fill_blend(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, uint8_t alpha);
void gfx_rect(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void gfx_hgrad(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c0, uint32_t c1);
void gfx_vgrad(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c0, uint32_t c1);
void gfx_round_fill(struct gfx_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
void gfx_text(struct gfx_surface *s, int32_t x, int32_t y, const char *utf8, uint32_t color);
int32_t gfx_text_width(const char *utf8);
void gfx_blit(struct gfx_surface *dst, int32_t dx, int32_t dy, const struct gfx_surface *src, int32_t sx, int32_t sy, int32_t w, int32_t h);
void gfx_disc(struct gfx_surface *s, int32_t cx, int32_t cy, int32_t r, uint32_t color);

#endif
