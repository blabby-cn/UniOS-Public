#include <stddef.h>
#include <stdint.h>
#include "math_impl.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#include "svg.h"
#include "kheap.h"
#include "util.h"

extern const char svg_data_computer[];
extern const char svg_data_computer_end[];
extern const char svg_data_folder[];
extern const char svg_data_folder_end[];
extern const char svg_data_gear[];
extern const char svg_data_gear_end[];
extern const char svg_data_task[];
extern const char svg_data_task_end[];
extern const char svg_data_file_normal[];
extern const char svg_data_file_normal_end[];
extern const char svg_data_file_document[];
extern const char svg_data_file_document_end[];

typedef struct {
    const char *name;
    const char *data;
} svg_asset;

static const svg_asset g_assets[] = {
    {"computer",      (const char *)(uintptr_t)svg_data_computer},
    {"folder",        (const char *)(uintptr_t)svg_data_folder},
    {"gear",          (const char *)(uintptr_t)svg_data_gear},
    {"task",          (const char *)(uintptr_t)svg_data_task},
    {"file_normal",   (const char *)(uintptr_t)svg_data_file_normal},
    {"file_document", (const char *)(uintptr_t)svg_data_file_document},
};
static const int g_asset_count = 6;

int svg_asset_count(void) { return g_asset_count; }

const char *svg_asset_name(int index)
{
    if (index < 0 || index >= g_asset_count) return 0;
    return g_assets[index].name;
}

const char *svg_asset_data(int index)
{
    if (index < 0 || index >= g_asset_count) return 0;
    return g_assets[index].data;
}

struct svg_ctx {
    NSVGrasterizer *rast;
};

svg_ctx *svg_init(void)
{
    svg_ctx *ctx = kmalloc(sizeof(svg_ctx));
    if (!ctx) return 0;
    ctx->rast = nsvgCreateRasterizer();
    if (!ctx->rast) { kfree(ctx); return 0; }
    return ctx;
}

void svg_destroy(svg_ctx *ctx)
{
    if (!ctx) return;
    if (ctx->rast) nsvgDeleteRasterizer(ctx->rast);
    kfree(ctx);
}

void *svg_load(svg_ctx *ctx, const char *name, int *w, int *h)
{
    (void)ctx;
    int i;
    for (i = 0; i < g_asset_count; i++)
    {
        if (strcmp(g_assets[i].name, name) == 0)
        {
            char *buf = kmalloc(4096);
            if (!buf) return 0;
            unsigned long sz;
            const char *src = g_assets[i].data;
            for (sz = 0; sz < 4095 && src[sz]; sz++) buf[sz] = src[sz];
            buf[sz] = 0;
            NSVGimage *img = nsvgParse(buf, "px", 96.0f);
            kfree(buf);
            if (!img) return 0;
            *w = (int)img->width;
            *h = (int)img->height;
            return img;
        }
    }
    return 0;
}

void svg_render(svg_ctx *ctx, void *image, uint8_t *dst, int w, int h, int pitch)
{
    if (!ctx || !image || !dst) return;
    NSVGimage *img = (NSVGimage *)image;
    float scale_x = (float)w / img->width;
    float scale_y = (float)h / img->height;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    float off_x = ((float)w - img->width * scale) * 0.5f;
    float off_y = ((float)h - img->height * scale) * 0.5f;
    nsvgRasterize(ctx->rast, img, off_x, off_y, scale, dst, w, h, pitch);
}

void svg_unload(void *image)
{
    if (!image) return;
    nsvgDelete((NSVGimage *)image);
}
