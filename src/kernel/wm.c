#include "wm.h"
#include "tooltip.h"
#include "kheap.h"
#include "pmm.h"
#include "kprintf.h"
#include "svga.h"
#include "sched.h"

extern int ui_hover_changed(void);

void *memset(void *dst, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);

static struct gfx_surface g_screen;
static struct gfx_surface g_back;
static uint64_t g_fb_addr;
static uint32_t g_fb_pitch;

static struct UiWindow g_windows[WM_MAX_WINDOWS];
static struct UiWindow *g_zorder[WM_MAX_WINDOWS];
static uint32_t g_nwin;

static wm_bg_fn g_bg;
static wm_bar_fn g_bar;
static wm_barclick_fn g_barclick;
static wm_overlay_fn g_overlay;
static wm_overlay_click_fn g_overlay_click;
static int32_t g_bar_h;

static int32_t g_mx;
static int32_t g_my;
static uint8_t g_mbtn;
static struct UiWindow *g_drag;
static int32_t g_drag_dx;
static int32_t g_drag_dy;
static uint32_t g_frames;
static int32_t g_cur_x;
static int32_t g_cur_y;
static uint32_t g_cur_back[24 * 24];

static int32_t g_last_hw_x = -1;
static int32_t g_last_hw_y = -1;
static uint8_t g_last_hw_btn;
static int32_t g_last_hw_wheel = 0;

static int g_dirty = 1;

static int win_onscreen(struct UiWindow *win)
{
    return win->state == WIN_NORMAL || win->state == WIN_MAXIMIZED;
}

void wm_mark_dirty(void)
{
    g_dirty = 1;
}

int wm_init(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height)
{
    g_fb_addr = fb_addr;
    g_fb_pitch = pitch;
    g_screen.base = (uint8_t *)(unsigned long)fb_addr;
    g_screen.w = width;
    g_screen.h = height;
    g_screen.pitch = pitch;

    g_back.w = width;
    g_back.h = height;
    g_back.pitch = width * 4;
    g_back.base = kmalloc((uint64_t)width * height * 4);
    if (!g_back.base)
        return -1;
    memset(g_back.base, 0, (uint64_t)width * height * 4);

    g_mx = (int32_t)width / 2;
    g_my = (int32_t)height / 2;
    g_cur_x = g_mx;
    g_cur_y = g_my;
    g_nwin = 0;
    return 0;
}

struct UiWindow *wm_create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t accent, wm_paint_fn paint)
{
    if (g_nwin >= WM_MAX_WINDOWS)
        return 0;
    struct UiWindow *win = 0;
    for (uint32_t s = 0; s < WM_MAX_WINDOWS; s++)
    {
        if (g_windows[s].state == WIN_FREE)
        {
            win = &g_windows[s];
            break;
        }
    }
    if (!win)
        return 0;
    kprintf("wm_create: try '%s' nwin=%u slot=%u\n", title ? title : "", g_nwin, (uint32_t)(win - g_windows));
    memset(win, 0, sizeof(*win));
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->accent = accent;
    win->state = WIN_NORMAL;
    win->paint = paint;

    uint32_t i = 0;
    while (title[i] && i < WM_TITLE_MAX - 1)
    {
        win->title[i] = title[i];
        i++;
    }
    win->title[i] = 0;

    win->content.w = (uint32_t)(w - 2);
    win->content.h = (uint32_t)(h - WM_TITLE_H - 1);
    win->content.pitch = win->content.w * 4;
    win->content.base = kmalloc((uint64_t)win->content.pitch * win->content.h);
    if (!win->content.base)
    {
        uint64_t ppt = 0, ppu = 0, ppf = 0;
        pmm_stats(&ppt, &ppu, &ppf);
        kprintf("wm_create: '%s' FAILED kmalloc need=%u heap_used=%u free=%u physfree=%u\n",
                title ? title : "", (uint32_t)((uint64_t)win->content.pitch * win->content.h),
                kheap_used(), kheap_free(), (uint32_t)ppf);
        win->state = WIN_FREE;
        win->title[0] = 0;
        return 0;
    }
    memset(win->content.base, 0, (uint64_t)win->content.pitch * win->content.h);

    g_zorder[g_nwin] = win;
    g_nwin++;
    wm_focus(win);
    kprintf("wm_create: '%s' OK slot=%u\n", title ? title : "", (uint32_t)(win - g_windows));
    return win;
}

struct UiWindow *wm_create_dialog(const char *title, int32_t x, int32_t y, int32_t w, int32_t h, wm_paint_fn paint)
{
    struct UiWindow *win = wm_create_window(title, x, y, w, h, 0x00417AC0, paint);
    if (win) win->flags = WM_WIN_DIALOG;
    return win;
}

void wm_window_add_widget(struct UiWindow *win, struct UiWidget *widget)
{
    widget->next = 0;
    if (!win->widgets)
    {
        win->widgets = widget;
        return;
    }
    struct UiWidget *w = win->widgets;
    while (w->next)
        w = w->next;
    w->next = widget;
}

void wm_window_anchor(struct UiWindow *win, struct UiWidget *w, uint8_t flags)
{
    ui_widget_set_anchor(w, flags, (int32_t)win->content.w, (int32_t)win->content.h);
}

void wm_window_resize_content(struct UiWindow *win)
{
    if (!win) return;
    int32_t nw = win->w - 2;
    int32_t nh = win->h - WM_TITLE_H - 1;
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    uint32_t pitch = (uint32_t)nw * 4;
    uint8_t *buf = kmalloc((uint64_t)pitch * (uint64_t)nh);
    if (!buf) return;
    memset(buf, 0, (uint64_t)pitch * (uint64_t)nh);
    if (win->content.base) kfree(win->content.base);
    win->content.base = buf;
    win->content.w = (uint32_t)nw;
    win->content.h = (uint32_t)nh;
    win->content.pitch = pitch;
}

void wm_layout_widgets(struct UiWindow *win)
{
    if (!win) return;
    struct UiWidget *w = win->widgets;
    while (w)
    {
        ui_widget_layout(w, (int32_t)win->content.w, (int32_t)win->content.h);
        w = w->next;
    }
}

void wm_set_background(wm_bg_fn fn)
{
    g_bg = fn;
}

void wm_set_taskbar(int32_t height, wm_bar_fn paint, wm_barclick_fn click)
{
    g_bar_h = height;
    g_bar = paint;
    g_barclick = click;
}

void wm_set_overlay(wm_overlay_fn fn)
{
    g_overlay = fn;
}

void wm_set_overlay_click(wm_overlay_click_fn fn)
{
    g_overlay_click = fn;
}

void wm_focus(struct UiWindow *win)
{
    uint32_t idx = 0;
    int found = 0;
    for (uint32_t i = 0; i < g_nwin; i++)
    {
        if (g_zorder[i] == win)
        {
            idx = i;
            found = 1;
            break;
        }
    }
    if (!found)
        return;
    for (uint32_t i = idx; i + 1 < g_nwin; i++)
        g_zorder[i] = g_zorder[i + 1];
    g_zorder[g_nwin - 1] = win;
    for (uint32_t i = 0; i < g_nwin; i++)
        g_zorder[i]->focused = (g_zorder[i] == win) ? 1 : 0;
    g_dirty = 1;
}

void wm_show(struct UiWindow *win, int visible)
{
    if (!win || win->state == WIN_FREE)
        return;
    if (visible)
    {
        if (win->state == WIN_MINIMIZED || win->state == WIN_HIDDEN)
            win->state = (win->restore_state == WIN_MAXIMIZED) ? WIN_MAXIMIZED : WIN_NORMAL;
        wm_focus(win);
    }
    else
    {
        if (win_onscreen(win))
            win->restore_state = win->state;
        win->state = WIN_HIDDEN;
        win->focused = 0;
        if (g_drag == win)
            g_drag = 0;
    }
    g_dirty = 1;
}

void wm_minimize(struct UiWindow *win)
{
    if (!win || !win_onscreen(win))
        return;
    win->restore_state = win->state;
    win->state = WIN_MINIMIZED;
    win->focused = 0;
    if (g_drag == win)
        g_drag = 0;
    g_dirty = 1;
}

void wm_destroy_window(struct UiWindow *win)
{
    if (!win || win->state == WIN_FREE) return;
    win->state = WIN_FREE;
    win->has_saved = 0;
    win->title[0] = 0;
    if (win->content.base)
    {
        kfree(win->content.base);
        win->content.base = 0;
    }
    uint32_t i, j = 0;
    for (i = 0; i < g_nwin; i++)
    {
        if (g_zorder[i] == win) continue;
        g_zorder[j++] = g_zorder[i];
    }
    g_nwin = j;
    g_dirty = 1;
    g_drag = 0;
    for (int32_t i = (int32_t)g_nwin - 1; i >= 0; i--)
    {
        if (win_onscreen(g_zorder[i]))
        {
            wm_focus(g_zorder[i]);
            break;
        }
    }
}

void wm_window_set_close(struct UiWindow *win, wm_close_fn fn)
{
    win->on_close = fn;
}

void wm_close_window(struct UiWindow *win)
{
    if (!win || win->state == WIN_FREE) return;
    if (win->on_close && win->on_close(win))
        return;
    wm_destroy_window(win);
}

int wm_has_dialog(void)
{
    for (uint32_t i = 0; i < g_nwin; i++)
    {
        if (win_onscreen(g_zorder[i]) && (g_zorder[i]->flags & WM_WIN_DIALOG))
            return 1;
    }
    return 0;
}

void wm_window_set_icon(struct UiWindow *win, uint8_t *rgba, int32_t iw, int32_t ih)
{
    win->icon = rgba;
    win->icon_w = iw;
    win->icon_h = ih;
}

void wm_window_set_title(struct UiWindow *win, const char *title)
{
    uint32_t i = 0;
    while (title[i] && i < WM_TITLE_MAX - 1) { win->title[i] = title[i]; i++; }
    win->title[i] = 0;
}

void wm_window_set_click(struct UiWindow *win, wm_content_click_fn fn)
{
    win->on_click = fn;
}

struct UiWindow *wm_focused(void)
{
    for (int32_t i = (int32_t)g_nwin - 1; i >= 0; i--)
    {
        if (win_onscreen(g_zorder[i]))
            return g_zorder[i];
    }
    return 0;
}

void wm_for_each_taskbar(wm_window_iter_fn fn, void *ctx)
{
    for (uint32_t i = 0; i < WM_MAX_WINDOWS; i++)
    {
        struct UiWindow *win = &g_windows[i];
        if (win->state == WIN_FREE || win->state == WIN_HIDDEN)
            continue;
        if (!win->title[0])
            continue;
        fn(win, ctx);
    }
}

struct UiWindow *wm_window_at(int32_t x, int32_t y)
{
    for (int32_t i = (int32_t)g_nwin - 1; i >= 0; i--)
    {
        struct UiWindow *win = g_zorder[i];
        if (!win_onscreen(win))
            continue;
        if (x >= win->x && y >= win->y && x < win->x + win->w && y < win->y + win->h)
            return win;
    }
    return 0;
}

static int send_widget_event(struct UiWindow *win, uint8_t type, int32_t gx, int32_t gy, char key)
{
    struct UiEvent ev;
    ev.type = type;
    ev.x = gx - win->x - 1;
    ev.y = gy - win->y - WM_TITLE_H;
    ev.buttons = g_mbtn;
    ev.key = key;
    return ui_widget_dispatch(win->widgets, &ev);
}

static uint64_t g_dc_t;
static int32_t g_dc_x, g_dc_y;
static struct UiWindow *g_dc_win;

static void toggle_maximize(struct UiWindow *win)
{
    if (win->state == WIN_MAXIMIZED)
    {
        if (win->has_saved)
        {
            win->x = win->sx;
            win->y = win->sy;
            win->w = win->sw;
            win->h = win->sh;
            win->has_saved = 0;
        }
        win->state = WIN_NORMAL;
    }
    else
    {
        win->sx = win->x;
        win->sy = win->y;
        win->sw = win->w;
        win->sh = win->h;
        win->has_saved = 1;
        win->x = 0;
        win->y = 0;
        win->w = (int32_t)g_back.w;
        win->h = (int32_t)g_back.h - g_bar_h;
        win->state = WIN_MAXIMIZED;
    }
    wm_window_resize_content(win);
    wm_layout_widgets(win);
    g_dirty = 1;
}

int close_hit(int32_t px, int32_t py, int32_t cx, int32_t cy)
{
    int32_t dx = px - cx;
    int32_t dy = py - cy;
    return (dx * dx + dy * dy) <= 36;
}

void wm_inject_mouse(int32_t x, int32_t y, uint8_t buttons)
{
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= (int32_t)g_back.w)
        x = (int32_t)g_back.w - 1;
    if (y >= (int32_t)g_back.h)
        y = (int32_t)g_back.h - 1;

    uint8_t prev = g_mbtn;
    g_mx = x;
    g_my = y;
    g_mbtn = buttons;

    uint8_t was_down = prev & 1;
    uint8_t is_down = buttons & 1;

    if (!was_down && is_down)
    {
        if (wm_has_dialog())
        {
            struct UiWindow *hit = wm_window_at(x, y);
            if (!hit || !(hit->flags & WM_WIN_DIALOG))
                return;
        }
        if (g_overlay_click && g_overlay_click(x, y))
        {
            g_dirty = 1;
            return;
        }
        struct UiWindow *win = wm_window_at(x, y);
        if (win)
        {
            wm_focus(win);
            if (y < win->y + WM_TITLE_H)
            {
                int32_t relx = x - win->x;

                if (!(win->flags & WM_WIN_DIALOG))
                {
                    int32_t my = win->y + WM_TITLE_H / 2;
                    if (close_hit(x, y, win->x + win->w - 52, my)) { wm_minimize(win); return; }
                    if (close_hit(x, y, win->x + win->w - 34, my)) { toggle_maximize(win); return; }
                    if (close_hit(x, y, win->x + win->w - 16, my)) { wm_close_window(win); return; }
                }
                else
                {
                    int32_t dy = win->y + WM_TITLE_H / 2;
                    if (close_hit(x, y, win->x + win->w - 16, dy)) { wm_close_window(win); g_dirty = 1; return; }
                }
                {
                    uint64_t now = sched_ticks();
                    int dx = x - g_dc_x;
                    int dy = y - g_dc_y;
                    int dbl = (g_dc_win == win && (now - g_dc_t) < 30 &&
                               x >= win->x && x < win->x + win->w - 60 &&
                               dx > -10 && dx < 10 && dy > -10 && dy < 10);
                    g_dc_win = win; g_dc_t = now; g_dc_x = x; g_dc_y = y;
                    if (dbl)
                    {
                        toggle_maximize(win);
                        return;
                    }
                    if (win->state == WIN_MAXIMIZED)
                    {
                        toggle_maximize(win);
                        g_drag_dx = win->w / 2;
                        g_drag_dy = WM_TITLE_H / 2;
                        win->x = x - g_drag_dx;
                        win->y = y - g_drag_dy;
                        if (win->y < 0)
                            win->y = 0;
                    }
                    else
                    {
                        g_drag_dx = x - win->x;
                        g_drag_dy = y - win->y;
                    }
                    g_drag = win;
                }
            }
            else
            {
                send_widget_event(win, UI_EVENT_MOUSE_DOWN, x, y, 0);
                g_dirty = 1;
            }
        }
        else if (g_bar && y >= (int32_t)g_back.h - g_bar_h)
        {
            if (g_barclick)
                g_barclick(x);
        }
        else if (g_overlay_click)
        {
            if (g_overlay_click(x, y))
                g_dirty = 1;
        }
    }
    else if (was_down && !is_down)
    {
        g_drag = 0;
        struct UiWindow *win = wm_focused();
        if (win)
        {
            int handled = send_widget_event(win, UI_EVENT_MOUSE_UP, x, y, 0);
            if (!handled && win->on_click && y >= win->y + WM_TITLE_H && y < win->y + win->h)
                win->on_click(win, x - win->x - 1, y - win->y - WM_TITLE_H);
            g_dirty = 1;
        }
    }
    else if (is_down && g_drag)
    {
        g_drag->x = x - g_drag_dx;
        g_drag->y = y - g_drag_dy;
        if (g_drag->x < -g_drag->w + 60)
            g_drag->x = -g_drag->w + 60;
        if (g_drag->y < 0)
            g_drag->y = 0;
        if (g_drag->x > (int32_t)g_back.w - 60)
            g_drag->x = (int32_t)g_back.w - 60;
        if (g_drag->y > (int32_t)g_back.h - WM_TITLE_H)
            g_drag->y = (int32_t)g_back.h - WM_TITLE_H;
        g_dirty = 1;
    }
    else if (is_down)
    {
        struct UiWindow *win = wm_focused();
        if (win)
        {
            int h = send_widget_event(win, UI_EVENT_MOUSE_MOVE, x, y, 0);
            if (h || ui_hover_changed())
                g_dirty = 1;
        }
    }
    else
    {
        struct UiWindow *win = wm_focused();
        if (win)
        {
            int h = send_widget_event(win, UI_EVENT_MOUSE_MOVE, x, y, 0);
            if (h || ui_hover_changed())
                g_dirty = 1;
        }
    }
}

void wm_inject_wheel(int32_t delta)
{
    if (!delta)
        return;
    g_dirty = 1;
    struct UiWindow *win = wm_window_at(g_mx, g_my);
    if (!win)
        return;
    struct UiEvent ev;
    ev.type = UI_EVENT_WHEEL;
    ev.x = g_mx - win->x - 1;
    ev.y = g_my - win->y - WM_TITLE_H;
    ev.buttons = g_mbtn;
    ev.key = 0;
    ev.wheel = delta;
    ui_widget_dispatch(win->widgets, &ev);
}

void wm_inject_key(char c)
{
    g_dirty = 1;
    if (c == 23)
    {
        struct UiWindow *win = wm_focused();
        if (win && !(win->flags & WM_WIN_DIALOG))
            wm_close_window(win);
        return;
    }
    struct UiWindow *win = wm_focused();
    if (win)
        send_widget_event(win, UI_EVENT_KEY, g_mx, g_my, c);
}

static void draw_cursor(struct gfx_surface *s, int32_t x, int32_t y)
{
    for (int32_t row = 0; row < 18; row++)
    {
        int32_t span = row < 12 ? row + 1 : (18 - row) * 2;
        if (span < 1)
            span = 1;
        for (int32_t col = 0; col < span && col <= row; col++)
        {
            uint32_t c = (col == 0 || col == span - 1 || row == 17) ? 0x00101418 : 0x00F2F5FA;
            int32_t px = x + col;
            int32_t py = y + row;
            if (px >= 0 && py >= 0 && px < (int32_t)s->w && py < (int32_t)s->h)
                *(uint32_t *)(s->base + (uint64_t)py * s->pitch + (uint64_t)px * 4) = c;
        }
    }
}

static void draw_window(struct gfx_surface *s, struct UiWindow *win)
{
    gfx_fill(s, win->x, win->y, win->w, win->h, 0x00222222);

    gfx_fill(s, win->x, win->y, win->w, WM_TITLE_H, 0x00000000);
    uint32_t tcol = win->focused ? 0x00E6E6E6 : 0x00808080;

    int32_t tx = win->x + 10;
    if (win->icon && win->icon_w > 0 && win->icon_h > 0)
    {
        int32_t iy = win->y + (WM_TITLE_H - win->icon_h) / 2;
        int32_t ix = win->x + 6;
        for (int32_t py = 0; py < win->icon_h; py++)
        {
            for (int32_t px = 0; px < win->icon_w; px++)
            {
                uint32_t off = (uint32_t)(py * win->icon_w + px) * 4;
                uint8_t r = win->icon[off];
                uint8_t g = win->icon[off + 1];
                uint8_t b = win->icon[off + 2];
                uint8_t a = win->icon[off + 3];
                if (a > 0)
                {
                    uint32_t color = ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16) | ((uint32_t)a << 24);
                    gfx_fill_blend(s, ix + px, iy + py, 1, 1, color, a);
                }
            }
        }
        tx = ix + win->icon_w + 6;
    }

    gfx_text(s, tx, win->y + (WM_TITLE_H - 16) / 2, win->title, tcol);

    if (!(win->flags & WM_WIN_DIALOG))
    {
        int32_t by = win->y + WM_TITLE_H / 2;
        gfx_disc(s, win->x + win->w - 52, by, 6, 0x00FFBD2E);
        gfx_disc(s, win->x + win->w - 34, by, 6, 0x0028C840);
        gfx_disc(s, win->x + win->w - 16, by, 6, 0x00FF5F57);
    }
    else
    {
        int32_t by = win->y + WM_TITLE_H / 2;
        gfx_disc(s, win->x + win->w - 16, by, 6, 0x00FF5F57);
    }

    if (win->paint)
        win->paint(win, &win->content);
    ui_widget_draw_all(win->widgets, &win->content);

    gfx_blit(s, win->x + 1, win->y + WM_TITLE_H, &win->content, 0, 0,
             (int32_t)win->content.w, (int32_t)win->content.h);

    uint32_t border = win->focused ? 0x00555555 : 0x00333333;
    gfx_rect(s, win->x - 1, win->y - 1, win->w + 2, win->h + 2, border);
}

#define CUR_W 20
#define CUR_H 20

static void cur_clamp(int32_t *x1, int32_t *y1)
{
    int32_t x = *x1, y = *y1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > (int32_t)g_back.w - CUR_W) x = (int32_t)g_back.w - CUR_W;
    if (y > (int32_t)g_back.h - CUR_H) y = (int32_t)g_back.h - CUR_H;
    *x1 = x; *y1 = y;
}

static void cur_backup(int32_t x, int32_t y)
{
    cur_clamp(&x, &y);
    for (int32_t r = 0; r < CUR_H; r++)
    {
        int32_t yy = y + r;
        uint8_t *src = g_back.base + (uint64_t)yy * g_back.pitch + (uint64_t)x * 4;
        uint8_t *dst = (uint8_t *)g_cur_back + (uint64_t)r * CUR_W * 4;
        memcpy(dst, src, CUR_W * 4);
    }
}

static void cur_restore(int32_t x, int32_t y)
{
    cur_clamp(&x, &y);
    for (int32_t r = 0; r < CUR_H; r++)
    {
        int32_t yy = y + r;
        uint8_t *src = (uint8_t *)g_cur_back + (uint64_t)r * CUR_W * 4;
        uint8_t *dst = g_back.base + (uint64_t)yy * g_back.pitch + (uint64_t)x * 4;
        memcpy(dst, src, CUR_W * 4);
    }
}

static void cur_upload(int32_t x, int32_t y)
{
    cur_clamp(&x, &y);
    for (int32_t r = 0; r < CUR_H; r++)
    {
        int32_t yy = y + r;
        uint8_t *src = g_back.base + (uint64_t)yy * g_back.pitch + (uint64_t)x * 4;
        uint8_t *dst = (uint8_t *)(unsigned long)g_fb_addr + (uint64_t)yy * g_fb_pitch + (uint64_t)x * 4;
        memcpy(dst, src, CUR_W * 4);
    }
    svga_update((uint32_t)x, (uint32_t)y, CUR_W, CUR_H);
}

void wm_composite(void)
{
    if (g_dirty)
    {
        g_dirty = 0;

        if (g_bg)
            g_bg(&g_back);
        else
            gfx_fill(&g_back, 0, 0, (int32_t)g_back.w, (int32_t)g_back.h, 0x00101828);

        for (uint32_t i = 0; i < g_nwin; i++)
            if (win_onscreen(g_zorder[i]) && !(g_zorder[i]->flags & WM_WIN_DIALOG))
                draw_window(&g_back, g_zorder[i]);
        for (uint32_t i = 0; i < g_nwin; i++)
            if (win_onscreen(g_zorder[i]) && (g_zorder[i]->flags & WM_WIN_DIALOG))
                draw_window(&g_back, g_zorder[i]);

        if (g_bar)
            g_bar(&g_back);

        if (g_overlay)
            g_overlay(&g_back);

        tooltip_paint(&g_back);

        cur_backup(g_mx, g_my);
        draw_cursor(&g_back, g_mx, g_my);

        for (uint32_t y = 0; y < g_back.h; y++)
            memcpy((uint8_t *)(unsigned long)g_fb_addr + (uint64_t)y * g_fb_pitch,
                   g_back.base + (uint64_t)y * g_back.pitch,
                   (uint64_t)g_back.w * 4);

        svga_update(0, 0, g_back.w, g_back.h);
        g_frames++;
        g_cur_x = g_mx; g_cur_y = g_my;
    }
    else
    {
        if (g_mx != g_cur_x || g_my != g_cur_y)
        {
            cur_restore(g_cur_x, g_cur_y);
            cur_backup(g_mx, g_my);
            draw_cursor(&g_back, g_mx, g_my);
            cur_upload(g_cur_x, g_cur_y);
            cur_upload(g_mx, g_my);
            g_frames++;
            g_cur_x = g_mx; g_cur_y = g_my;
        }
    }
}

uint32_t wm_frame_count(void)
{
    return g_frames;
}

int32_t wm_cursor_x(void)
{
    return g_mx;
}

int32_t wm_cursor_y(void)
{
    return g_my;
}

uint32_t wm_window_count(void)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < g_nwin; i++)
        if (win_onscreen(g_zorder[i]))
            n++;
    return n;
}

uint32_t wm_total_windows(void)
{
    return g_nwin;
}

void wm_pump_hardware(void)
{
    extern int32_t mouse_x(void);
    extern int32_t mouse_y(void);
    extern uint8_t mouse_buttons(void);
    extern int32_t mouse_wheel(void);

    int32_t hx = mouse_x();
    int32_t hy = mouse_y();
    uint8_t hb = mouse_buttons();
    if (hx != g_last_hw_x || hy != g_last_hw_y || hb != g_last_hw_btn)
    {
        g_last_hw_x = hx;
        g_last_hw_y = hy;
        g_last_hw_btn = hb;
        wm_inject_mouse(hx, hy, hb);
    }

    int32_t hw = mouse_wheel();
    if (hw != g_last_hw_wheel)
    {
        g_last_hw_wheel = hw;
        wm_inject_wheel(hw);
    }
}
