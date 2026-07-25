#ifndef UNI_WM_H
#define UNI_WM_H

#include <stdint.h>
#include "gfx.h"
#include "ui.h"

#define WM_MAX_WINDOWS 12
#define WM_TITLE_H 30
#define WM_TITLE_MAX 48
#define WM_WIN_DIALOG 1

enum
{
    WIN_FREE = 0,
    WIN_NORMAL = 1,
    WIN_MAXIMIZED = 2,
    WIN_MINIMIZED = 3,
    WIN_HIDDEN = 4
};

struct UiWindow;

typedef void (*wm_paint_fn)(struct UiWindow *win, struct gfx_surface *content);
typedef void (*wm_bg_fn)(struct gfx_surface *screen);
typedef void (*wm_bar_fn)(struct gfx_surface *screen);
typedef void (*wm_barclick_fn)(int32_t x);
typedef void (*wm_overlay_fn)(struct gfx_surface *screen);
typedef int (*wm_overlay_click_fn)(int32_t x, int32_t y);
typedef int (*wm_content_click_fn)(struct UiWindow *win, int32_t x, int32_t y);
typedef void (*wm_window_iter_fn)(struct UiWindow *win, void *ctx);
typedef int (*wm_close_fn)(struct UiWindow *win);

struct UiWindow
{
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    char title[WM_TITLE_MAX];
    uint32_t accent;
    uint8_t state;
    uint8_t restore_state;
    uint8_t has_saved;
    uint8_t focused;
    uint8_t flags;
    int32_t sx;
    int32_t sy;
    int32_t sw;
    int32_t sh;
    struct gfx_surface content;
    struct UiWidget *widgets;
    wm_paint_fn paint;
    wm_content_click_fn on_click;
    wm_close_fn on_close;
    void *user;
    uint8_t *icon;
    int32_t icon_w;
    int32_t icon_h;
};

int wm_init(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height);
struct UiWindow *wm_create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t accent, wm_paint_fn paint);
struct UiWindow *wm_create_dialog(const char *title, int32_t x, int32_t y, int32_t w, int32_t h, wm_paint_fn paint);
void wm_window_add_widget(struct UiWindow *win, struct UiWidget *widget);
void wm_window_anchor(struct UiWindow *win, struct UiWidget *w, uint8_t flags);
void wm_window_resize_content(struct UiWindow *win);
void wm_layout_widgets(struct UiWindow *win);
void wm_set_background(wm_bg_fn fn);
void wm_set_taskbar(int32_t height, wm_bar_fn paint, wm_barclick_fn click);
void wm_set_overlay(wm_overlay_fn fn);
void wm_set_overlay_click(wm_overlay_click_fn fn);
void wm_focus(struct UiWindow *win);
void wm_show(struct UiWindow *win, int visible);
void wm_minimize(struct UiWindow *win);
void wm_window_set_icon(struct UiWindow *win, uint8_t *rgba, int32_t iw, int32_t ih);
void wm_window_set_title(struct UiWindow *win, const char *title);
void wm_window_set_click(struct UiWindow *win, wm_content_click_fn fn);
void wm_window_set_close(struct UiWindow *win, wm_close_fn fn);
void wm_close_window(struct UiWindow *win);
void wm_destroy_window(struct UiWindow *win);
struct UiWindow *wm_focused(void);
struct UiWindow *wm_window_at(int32_t x, int32_t y);
int close_hit(int32_t px, int32_t py, int32_t cx, int32_t cy);
int wm_has_dialog(void);
void wm_for_each_taskbar(wm_window_iter_fn fn, void *ctx);
void wm_inject_mouse(int32_t x, int32_t y, uint8_t buttons);
void wm_inject_wheel(int32_t delta);
void wm_inject_key(char c);
void wm_pump_hardware(void);
void wm_composite(void);
void wm_mark_dirty(void);
uint32_t wm_frame_count(void);
int32_t wm_cursor_x(void);
int32_t wm_cursor_y(void);
uint32_t wm_window_count(void);
uint32_t wm_total_windows(void);

#endif
