#ifndef SVG_H
#define SVG_H

#include <stdint.h>

typedef struct svg_ctx svg_ctx;

svg_ctx *svg_init(void);
void svg_destroy(svg_ctx *ctx);
void *svg_load(svg_ctx *ctx, const char *name, int *w, int *h);
void svg_render(svg_ctx *ctx, void *image, uint8_t *dst, int w, int h, int pitch);
void svg_unload(void *image);

int svg_asset_count(void);
const char *svg_asset_name(int index);
const char *svg_asset_data(int index);

#endif