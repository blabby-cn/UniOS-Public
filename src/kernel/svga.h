#ifndef UNI_SVGA_H
#define UNI_SVGA_H

#include <stdint.h>

struct svga_info
{
    uint16_t io_base;
    uint64_t fb_phys;
    uint32_t vram_size;
    uint32_t fb_size;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t id;
};

struct svga_mode
{
    uint64_t base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
};

int svga_init(struct svga_info *out);
int svga_set_mode(uint32_t width, uint32_t height, uint32_t bpp, struct svga_mode *out);
int svga_fifo_init(void);
int svga_fifo_active(void);
uint32_t svga_caps(void);
void svga_update(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void svga_sync(void);

#endif
