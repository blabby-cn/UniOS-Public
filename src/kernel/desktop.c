#include "desktop.h"
#include "wm.h"
#include "ui.h"
#include "gfx.h"
#include "svg.h"
#include "kheap.h"
#include "pmm.h"
#include "vfs.h"
#include "sched.h"
#include "kprintf.h"
#include "serial.h"
#include "util.h"
#include "locale.h"
#include "svga.h"
#include "rtc.h"
#include "keyboard.h"
#include "guisys.h"
#include "tooltip.h"
#include "console.h"
#include "udll.h"

void *memset(void *dst, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);

static const char *tooltip_resolve(int32_t mx, int32_t my);

#define COL_CONTENT 0x00222222
#define COL_TEXT 0x00E6E6E6
#define COL_MUTED 0x00888888
#define COL_DIVIDER 0x00333333
#define COL_ACCENT 0x00417AC0
#define COL_BAR 0x00000000
#define COL_BAR_EDGE 0x00333333

volatile uint64_t desktop_thread_runs[3];

static uint32_t g_sw;
static uint32_t g_sh;
static struct gfx_surface g_wall;

static struct UiWindow *g_win_sys;
static struct UiWindow *g_win_files;
static struct UiWindow *g_win_ctl;
static struct UiWindow *g_win_hello;
static struct UiWindow *g_run_dlg;
static struct UiWindow *g_win_appmgr;
static struct UiWindow *g_win_calc;
static struct UiWindow *g_win_note;
static struct UiWindow *g_win_term;
static struct UiWindow *g_win_clk;
static struct UiTextInput g_file_path_inp;
static struct UiButton g_file_go_btn;
static struct UiStartMenu g_start_menu;
static struct UiDropdown g_dd_lang;
static struct UiDropdown g_dd_res;
static struct UiTextInput g_run_input;
static struct UiButton g_run_btn;
static struct svg_ctx *g_svg;

static struct UiButton g_btn_mark;
static struct UiButton g_btn_rescan;
static struct UiButton g_btn_save;
static struct UiToggle g_tgl_grid;
static struct UiToggle g_tgl_accent;
static struct UiTextInput g_input_note;
static struct UiTextArea  g_note_area;
static struct UiScrollbar g_note_sb;
static struct UiMenuBar   g_note_menu;
static struct UiScrollbar g_term_sb;
static struct UiTextInput g_calc_disp;
static struct UiButton g_calc_btns[16];
static struct UiTextArea  g_term_out;
static struct UiTextInput g_term_in;
static struct UiButton    g_term_btn;
static int g_cur_res;

static uint32_t g_mark_count;
static uint32_t g_rescan_count;
static int g_note_saved;
static int g_note_verified;
static char g_note_status[64];
static char g_note_filename[64];
static char g_note_filepath[128];
static int g_note_dirty;
static struct UiWindow *g_note_conf_dlg;
static struct UiButton    g_conf_save;
static struct UiButton    g_conf_notsave;
static struct UiButton    g_conf_cancel;
static int g_note_force_close;

typedef void (*fdlg_cb_t)(int result, const char *path, int encoding, void *ctx);
typedef void (*fdlg_show_t)(int mode, const char *title, const char *initial_path,
                            const char *ok_text, const char *cancel_text,
                            fdlg_cb_t cb, void *ctx);
static struct UdllHandle *g_filedlg;
static fdlg_show_t g_filedlg_show;

struct file_row
{
    char name[48];
    uint32_t size;
    uint8_t type;
};

static struct file_row g_rows[32];
static uint32_t g_nrows;
static char g_filepath[128];
static int g_filesel;

static uint64_t g_last_hb;

static void note_update_title(void);
static void note_on_change(struct UiTextArea *ta);
static void note_open_file(const char *path, const char *name);
static void note_do_save(void);
static void note_show_saveas(void);
static void note_show_confirm(void);
static int note_on_close(struct UiWindow *win);
static int files_on_close(struct UiWindow *win);
static int term_on_close(struct UiWindow *win);
static int ctl_on_close(struct UiWindow *win);
static int sys_on_close(struct UiWindow *win);
static void note_menu_save(struct UiWidget *w, void *ctx);
static void note_menu_save_as(struct UiWidget *w, void *ctx);
static void note_menu_open(struct UiWidget *w, void *ctx);
static void note_menu_exit(struct UiWidget *w, void *ctx);
static void note_conf_save_click(struct UiWidget *w, void *ctx);
static void note_conf_notsave_click(struct UiWidget *w, void *ctx);
static void note_conf_cancel_click(struct UiWidget *w, void *ctx);
static void note_conf_paint(struct UiWindow *win, struct gfx_surface *s);
static void note_show_open(void);
static int note_conf_dlg_onclose(struct UiWindow *win);

static void u2s(char *dst, uint32_t *pos, uint64_t v)
{
    char tmp[24];
    int i = 0;
    if (v == 0)
        tmp[i++] = '0';
    while (v)
    {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i > 0)
        dst[(*pos)++] = tmp[--i];
    dst[*pos] = 0;
}

static void s2s(char *dst, uint32_t *pos, const char *s)
{
    while (*s)
        dst[(*pos)++] = *s++;
    dst[*pos] = 0;
}

static void wall_build(void)
{
    gfx_vgrad(&g_wall, 0, 0, (int32_t)g_sw, (int32_t)g_sh, 0x00141414, 0x001C1C1C);
    gfx_text(&g_wall, 24, (int32_t)g_sh - 60, "UniOS 0.1.0", 0x00505050);
    gfx_text(&g_wall, 24, (int32_t)g_sh - 42, "x86_64 - self hosted", 0x003A3A3A);
}

static void bg_paint(struct gfx_surface *screen)
{
    for (uint32_t y = 0; y < screen->h; y++)
        memcpy(screen->base + (uint64_t)y * screen->pitch,
               g_wall.base + (uint64_t)y * g_wall.pitch,
               (uint64_t)screen->w * 4);
    if (g_tgl_grid.on)
    {
        for (uint32_t gy = 0; gy < screen->h; gy += 40)
            gfx_fill_blend(screen, 0, (int32_t)gy, (int32_t)screen->w, 1, 0x00FFFFFF, 8);
        for (uint32_t gx = 0; gx < screen->w; gx += 40)
            gfx_fill_blend(screen, (int32_t)gx, 0, 1, (int32_t)screen->h, 0x00FFFFFF, 8);
    }
}

#define BAR_H 40
#define BAR_ITEMS_MAX 16

static struct UiWindow *g_bar_items[BAR_ITEMS_MAX];
static int g_bar_count;

static void bar_collect(struct UiWindow *win, void *ctx)
{
    (void)ctx;
    if (g_bar_count < BAR_ITEMS_MAX)
        g_bar_items[g_bar_count++] = win;
}

static void bar_paint(struct gfx_surface *screen)
{
    int32_t y0 = (int32_t)screen->h - BAR_H;
    gfx_fill(screen, 0, y0, (int32_t)screen->w, BAR_H, COL_BAR);
    gfx_fill(screen, 0, y0, (int32_t)screen->w, 1,
             g_tgl_accent.on ? COL_ACCENT : COL_BAR_EDGE);

    gfx_text(screen, 20, y0 + 12, "UniOS", COL_TEXT);

    g_bar_count = 0;
    wm_for_each_taskbar(bar_collect, 0);

    for (int i = 0; i < g_bar_count; i++)
    {
        int32_t bx = 110 + i * 104;
        struct UiWindow *w = g_bar_items[i];
        int focused = w->focused;
        if (focused)
            gfx_fill(screen, bx, y0 + BAR_H - 8, 96, 2, COL_ACCENT);
        int32_t tw = gfx_text_width(w->title);
        gfx_text(screen, bx + (96 - tw) / 2, y0 + 12, w->title,
                 focused ? COL_TEXT : COL_MUTED);
    }

    rtc_time rt;
    rtc_read(&rt);
    char buf[24];
    uint32_t p = 0;
    if (rt.hour < 10) buf[p++] = '0';
    u2s(buf, &p, rt.hour);
    buf[p++] = ':';
    if (rt.min < 10) buf[p++] = '0';
    u2s(buf, &p, rt.min);
    buf[p++] = ':';
    if (rt.sec < 10) buf[p++] = '0';
    u2s(buf, &p, rt.sec);
    buf[p] = 0;
    int32_t tw = gfx_text_width(buf);
    gfx_text(screen, (int32_t)screen->w - tw - 16, y0 + 12, buf, COL_TEXT);
}

static void bar_click(int32_t x)
{
    if (x < 110)
    {
        g_start_menu.visible = 1;
        wm_mark_dirty();
        return;
    }
    for (int i = 0; i < g_bar_count; i++)
    {
        int32_t bx = 110 + i * 104;
        if (x >= bx && x < bx + 96 && g_bar_items[i])
        {
            wm_show(g_bar_items[i], 1);
            return;
        }
    }
}

static void sys_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);

    char buf[96];
    uint32_t p;
    int32_t y = 16;

    uint64_t pt = 0, pu = 0, pf = 0;
    pmm_stats(&pt, &pu, &pf);

    p = 0;
    s2s(buf, &p, "uptime ticks : ");
    u2s(buf, &p, sched_ticks());
    gfx_text(s, 16, y, buf, COL_TEXT);
    y += 22;

    p = 0;
    s2s(buf, &p, "tasks        : ");
    u2s(buf, &p, sched_task_count());
    gfx_text(s, 16, y, buf, COL_TEXT);
    y += 22;

    p = 0;
    s2s(buf, &p, "heap used/KB : ");
    u2s(buf, &p, kheap_used() / 1024);
    s2s(buf, &p, " free ");
    u2s(buf, &p, kheap_free() / 1024);
    gfx_text(s, 16, y, buf, COL_TEXT);
    y += 22;

    p = 0;
    s2s(buf, &p, "phys free pg : ");
    u2s(buf, &p, pf);
    gfx_text(s, 16, y, buf, COL_TEXT);
    y += 30;

    gfx_fill(s, 16, y, (int32_t)s->w - 32, 1, COL_DIVIDER);
    y += 12;
    gfx_text(s, 16, y, "worker threads", COL_MUTED);
    y += 22;
    static const char *wnames[3] = {"worker-0", "worker-1", "worker-2"};
    for (int i = 0; i < 3; i++)
    {
        gfx_disc(s, 22, y + 7, 4, 0x004A9E6E);
        gfx_text(s, 36, y, wnames[i], COL_TEXT);
        p = 0;
        s2s(buf, &p, "runs ");
        u2s(buf, &p, (uint32_t)desktop_thread_runs[i]);
        int32_t tw = gfx_text_width(buf);
        gfx_text(s, (int32_t)s->w - tw - 16, y, buf, COL_MUTED);
        y += 22;
    }
}

static void file_dir_init(void)
{
    uint32_t i = 0;
    while (i < 127) { g_filepath[i] = 0; i++; }
    g_filepath[0] = '/';
    g_rows[0].size = 0;
    g_rows[0].type = VFS_DIR;
    g_rows[0].name[0] = 'v';
    g_rows[0].name[1] = 'a';
    g_rows[0].name[2] = 'r';
    g_rows[0].name[3] = 0;
    g_nrows = 1;
    g_filesel = -1;
}

static void fcb(const struct vfs_stat *st, void *ctx)
{
    (void)ctx;
    if (g_nrows >= 31) return;
    uint32_t i = 0;
    while (st->name[i] && i < 63) { g_rows[g_nrows].name[i] = st->name[i]; i++; }
    g_rows[g_nrows].name[i] = 0;
    g_rows[g_nrows].size = st->size;
    g_rows[g_nrows].type = st->type;
    g_nrows++;
}

static void file_sync_input(void)
{
    uint32_t i = 0;
    while (g_filepath[i] && i < UI_TEXT_MAX - 1) { g_file_path_inp.text[i] = g_filepath[i]; i++; }
    g_file_path_inp.text[i] = 0;
    g_file_path_inp.len = i;
}

static void file_nav(const char *path)
{
    uint32_t i = 0;
    while (path[i] && i < 127) { g_filepath[i] = path[i]; i++; }
    g_filepath[i] = 0;
    g_nrows = 0;
    g_filesel = -1;
    vfs_list(path, fcb, 0);
    file_sync_input();
}

static void file_go_up(void)
{
    uint32_t n = 0;
    while (g_filepath[n]) n++;
    if (n <= 1) return;
    uint32_t cut = n - 1;
    while (cut > 0 && g_filepath[cut] != '/') cut--;
    if (cut == 0) { g_filepath[1] = 0; }
    else { g_filepath[cut] = 0; }
    file_nav(g_filepath);
}

static void file_enter(void)
{
    if (g_filesel < 0 || (uint32_t)g_filesel >= g_nrows) return;
    if (g_filesel == 0 && g_filepath[1] == 0) return;
    if (g_rows[g_filesel].type != VFS_DIR) return;
    uint32_t n = 0;
    while (g_filepath[n]) n++;
    if (n > 0 && g_filepath[n-1] != '/') { g_filepath[n] = '/'; n++; }
    uint32_t i = 0;
    while (g_rows[g_filesel].name[i] && (n + i) < 126)
    {
        g_filepath[n + i] = g_rows[g_filesel].name[i];
        i++;
    }
    g_filepath[n + i] = 0;
    file_nav(g_filepath);
}

static void files_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    int32_t y0 = 36;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
    gfx_fill(s, 16, y0, (int32_t)s->w - 32, 1, COL_DIVIDER);
    y0 += 2;

    int32_t y = y0 + 6;
    int32_t bottom = (int32_t)s->h - 40;
    {
        int sel_up = (g_filesel == -2);
        if (sel_up)
            gfx_fill(s, 12, y, (int32_t)s->w - 24, 18, 0x003A3A3A);
        gfx_fill(s, 16, y + 3, 12, 10, sel_up ? 0x00FFD27F : 0x00555555);
        gfx_text(s, 36, y, "..", sel_up ? 0x00FFFFFF : COL_TEXT);
        y += 20;
    }
    for (uint32_t i = 0; i < g_nrows && y < bottom; i++)
    {
        int selected = (g_filesel >= 0 && (uint32_t)g_filesel == i);
        if (selected)
            gfx_fill(s, 12, y, (int32_t)s->w - 24, 18, 0x003A3A3A);
        if (g_rows[i].type == VFS_DIR)
        {
            gfx_fill(s, 16, y + 3, 12, 10, selected ? 0x00FFD27F : COL_ACCENT);
            gfx_text(s, 36, y, g_rows[i].name, selected ? 0x00FFFFFF : COL_TEXT);
        }
        else
        {
            gfx_rect(s, 17, y + 2, 10, 12, COL_MUTED);
            gfx_text(s, 36, y, g_rows[i].name, selected ? 0x00FFFFFF : COL_TEXT);
            char buf[24];
            uint32_t p = 0;
            u2s(buf, &p, g_rows[i].size);
            s2s(buf, &p, " B");
            int32_t tw = gfx_text_width(buf);
            gfx_text(s, (int32_t)s->w - tw - 16, y, buf, COL_MUTED);
        }
        y += 20;
    }
}

static void ctl_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
    gfx_text(s, 16, 14, locale_get("win.control_center"), COL_TEXT);
    gfx_fill(s, 16, 34, (int32_t)s->w - 32, 1, COL_DIVIDER);

    gfx_text(s, 16, 118, locale_get("label.type_note"), COL_MUTED);

    gfx_text(s, 16, 216, locale_get("label.resolution"), COL_MUTED);
    gfx_text(s, 16, 236, locale_get("label.language"), COL_MUTED);

    gfx_text(s, 16, 270, g_note_status, g_note_verified ? 0x004A9E6E : COL_MUTED);
}

static void act_mark(struct UiWidget *w, void *ctx)
{
    (void)w;
    (void)ctx;
    wm_show(g_run_dlg, 1);
    kprintf("gui: run dialog opened\n");
}

static void rows_cb(const struct vfs_stat *st, void *ctx)
{
    (void)ctx;
    if (g_nrows >= 12)
        return;
    uint32_t i = 0;
    while (st->name[i] && i < 47)
    {
        g_rows[g_nrows].name[i] = st->name[i];
        i++;
    }
    g_rows[g_nrows].name[i] = 0;
    g_rows[g_nrows].size = st->size;
    g_rows[g_nrows].type = st->type;
    g_nrows++;
}

static void do_rescan(void)
{
    file_nav("/var/documents");
}

static void act_rescan(struct UiWidget *w, void *ctx)
{
    (void)w;
    (void)ctx;
    do_rescan();
    g_rescan_count++;
    kprintf("gui: files rescanned, entries=%u\n", g_nrows);
}

static void act_toggle(struct UiWidget *w, void *ctx)
{
    (void)ctx;
    struct UiToggle *t = (struct UiToggle *)w;
    kprintf("gui: toggle '%s' -> %s\n", t->label, t->on ? "on" : "off");
}

static void act_save(struct UiWidget *w, void *ctx)
{
    (void)w;
    (void)ctx;
    if (!g_input_note.len)
    {
        uint32_t p = 0;
        s2s(g_note_status, &p, locale_get("msg.no_note"));
        return;
    }
    int rc = vfs_write("/var/documents/gui_note.txt", g_input_note.text, g_input_note.len);
    g_note_saved = (rc == 0);

    static char verify[UI_TEXT_MAX];
    int64_t n = vfs_read("/var/documents/gui_note.txt", verify, sizeof(verify));
    g_note_verified = 0;
    if (n == (int64_t)g_input_note.len)
    {
        g_note_verified = 1;
        for (uint32_t i = 0; i < g_input_note.len; i++)
            if (verify[i] != g_input_note.text[i])
            {
                g_note_verified = 0;
                break;
            }
    }
    uint32_t p = 0;
    s2s(g_note_status, &p, g_note_verified ? "saved + verified on disk" : "save FAILED");
    kprintf("gui: note save rc=%d verify=%s len=%u\n", rc,
            g_note_verified ? "OK" : "FAIL", g_input_note.len);
    do_rescan();
}

static uint16_t g_res_table[][2] = {
    {1024, 768},
    {1280, 720},
    {1920, 1080},
    {640, 480},
};

static const char *g_res_names[4] = {"1024x768", "1280x720", "1920x1080", "640x480"};

static void save_config(void)
{
    char cfg[128];
    uint32_t p = 0;
    const char *ln = (locale_get_lang() == LANG_EN) ? "en" : "zh";
    s2s(cfg, &p, "lang=");
    s2s(cfg, &p, ln);
    s2s(cfg, &p, "\nres=");
    s2s(cfg, &p, g_res_names[g_dd_res.selected]);
    s2s(cfg, &p, "\n");
    vfs_write("/etc/unios.ini", cfg, p);
    kprintf("config saved: %u bytes\n", p);
}

static void act_res_dd(struct UiWidget *w, void *ctx)
{
    (void)ctx;
    struct UiDropdown *dd = (struct UiDropdown *)w;
    uint16_t nw = g_res_table[dd->selected][0];
    uint16_t nh = g_res_table[dd->selected][1];
    struct svga_mode sm;
    int rc = svga_set_mode(nw, nh, 32, &sm);
    (void)sm;
    save_config();
    kprintf("gui: resolution %ux%u rc=%d (restart to apply)\n", (int)nw, (int)nh, rc);
}

static void act_lang_dd(struct UiWidget *w, void *ctx)
{
    (void)ctx;
    struct UiDropdown *dd = (struct UiDropdown *)w;
    if (dd->selected == 0) locale_set_lang(LANG_EN);
    else locale_set_lang(LANG_ZH);
    if (g_win_sys)  wm_window_set_title(g_win_sys, locale_get("win.sys_monitor"));
    if (g_win_files) wm_window_set_title(g_win_files, locale_get("win.files"));
    if (g_win_ctl)   wm_window_set_title(g_win_ctl, locale_get("win.control_center"));
    if (g_win_hello) wm_window_set_title(g_win_hello, locale_get("win.hello"));
    if (g_run_dlg)   wm_window_set_title(g_run_dlg, locale_get("dlg.run_title"));
    if (g_win_note && g_win_note->title[0]) note_update_title();
    if (g_win_term && g_win_term->title[0]) wm_window_set_title(g_win_term, locale_get("win.terminal"));
    if (g_win_calc && g_win_calc->title[0]) wm_window_set_title(g_win_calc, locale_get("win.calculator"));
    if (g_win_clk  && g_win_clk->title[0])  wm_window_set_title(g_win_clk,  locale_get("win.clock"));
    ui_toggle_set_label(&g_tgl_grid, locale_get("label.grid_overlay"));
    ui_toggle_set_label(&g_tgl_accent, locale_get("label.accent_line"));
    ui_startmenu_set_label(&g_start_menu, 5, locale_get("win.calculator"));
    ui_startmenu_set_label(&g_start_menu, 6, locale_get("win.notepad"));
    ui_startmenu_set_label(&g_start_menu, 7, locale_get("win.terminal"));
    ui_startmenu_set_label(&g_start_menu, 8, locale_get("win.clock"));
    ui_button_set_label(&g_btn_mark, locale_get("btn.mark"));
    ui_button_set_label(&g_btn_save, locale_get("label.save_note"));
    ui_button_set_label(&g_run_btn, locale_get("btn.run"));
    ui_button_set_label(&g_file_go_btn, locale_get("btn.go"));
    ui_textinput_set_placeholder(&g_input_note, locale_get("label.type_note"));
    ui_textinput_set_placeholder(&g_run_input, locale_get("dlg.run_placeholder"));
    save_config();
    wm_mark_dirty();
    kprintf("gui: lang -> %s\n", (locale_get_lang() == LANG_EN) ? "en" : "zh");
}

static uint8_t *load_icon(const char *name, int32_t *w, int32_t *h)
{
    if (!g_svg) return 0;
    int iw, ih;
    void *img = svg_load(g_svg, name, &iw, &ih);
    if (!img)
    {
        kprintf("icon %s load failed\n", name);
        return 0;
    }
    int32_t sz = 20;
    uint8_t *buf = kmalloc((uint32_t)(sz * sz * 4));
    if (!buf) { svg_unload(img); return 0; }
    memset(buf, 0, (unsigned long)(sz * sz * 4));
    svg_render(g_svg, img, buf, sz, sz, sz * 4);
    svg_unload(img);
    *w = sz;
    *h = sz;
    kprintf("icon %s ok\n", name);
    return buf;
}

static void run_dlg_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, s->w, s->h, COL_CONTENT);
    gfx_text(s, 16, 16, locale_get("dlg.run_prompt"), COL_TEXT);
}

static void info_dlg_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, s->w, s->h, COL_CONTENT);
    gfx_text(s, 16, 14, locale_get("menu.info_title"), 0x00417AC0);
    gfx_fill(s, 16, 32, s->w - 32, 1, COL_DIVIDER);

    uint64_t pt = 0, pu = 0, pf = 0;
    pmm_stats(&pt, &pu, &pf);
    (void)pu;
    char buf[64];
    int32_t y = 46;
    uint32_t bp;
    gfx_text(s, 16, y, locale_get("sys.uptime"), COL_MUTED); y += 20;
    bp = 0; s2s(buf, &bp, "  "); u2s(buf, &bp, (uint32_t)sched_ticks()); s2s(buf, &bp, " ticks"); gfx_text(s, 28, y, buf, COL_TEXT); y += 24;
    gfx_text(s, 16, y, locale_get("sys.tasks"), COL_MUTED); y += 20;
    bp = 0; s2s(buf, &bp, "  "); u2s(buf, &bp, sched_task_count()); gfx_text(s, 28, y, buf, COL_TEXT); y += 24;
    gfx_text(s, 16, y, locale_get("sys.heap"), COL_MUTED); y += 20;
    bp = 0; s2s(buf, &bp, "  used "); u2s(buf, &bp, kheap_used() / 1024); s2s(buf, &bp, " KB  free ");
    u2s(buf, &bp, kheap_free() / 1024); s2s(buf, &bp, " KB"); gfx_text(s, 28, y, buf, COL_TEXT); y += 24;
    gfx_text(s, 16, y, locale_get("sys.memory"), COL_MUTED); y += 20;
    bp = 0; s2s(buf, &bp, "  "); u2s(buf, &bp, (uint32_t)pf); s2s(buf, &bp, " free / ");
    u2s(buf, &bp, (uint32_t)pt); s2s(buf, &bp, " total"); gfx_text(s, 28, y, buf, COL_TEXT);
}

static void act_run_cmd(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    if (strncmp(g_run_input.text, "univer", 6) == 0)
    {
        wm_show(g_run_dlg, 0);
        const char *t = (locale_get_lang() == LANG_ZH) ? "\xe7\xb3\xbb\xe7\xbb\x9f\xe4\xbf\xa1\xe6\x81\xaf" : "System Information";
        wm_create_dialog(t, 200, 100, 420, 260, info_dlg_paint);
        kprintf("gui: run univer\n");
        memset(g_run_input.text, 0, UI_TEXT_MAX);
        g_run_input.len = 0;
        return;
    }
    if (g_run_input.len == 0) return;
    kprintf("gui: run '%s' (unknown)\n", g_run_input.text);
    memset(g_run_input.text, 0, UI_TEXT_MAX);
    g_run_input.len = 0;
}

static void term_append_text(struct UiTextArea *ta, const char *s);
static void term_emit(void *ctx, const char *s);

static int32_t term_hdr_len(void)
{
    const char *hdr = "Blabby Co. UniOS [ver 0.1.0]\n(c) Blabby Co. All rights reserved.\n\n";
    int32_t n = 0;
    while (hdr[n]) n++;
    return n;
}

static int xstrcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static char g_cwd[256] = "/";

static void push_seg(char segs[32][64], int *nseg, const char *seg)
{
    int si = 0;
    while (seg[si]) si++;
    if (si == 1 && seg[0] == '.') return;
    if (si == 2 && seg[0] == '.' && seg[1] == '.') { if (*nseg > 0) (*nseg)--; return; }
    if (*nseg < 32)
    {
        int k = 0;
        while (seg[k]) { segs[*nseg][k] = seg[k]; k++; }
        segs[*nseg][k] = 0;
        (*nseg)++;
    }
}

static void resolve_path(const char *in, char *out, int outn)
{
    char segs[32][64];
    int nseg = 0;
    if (in[0] != '/')
    {
        const char *p = g_cwd;
        while (*p)
        {
            while (*p == '/') p++;
            if (!*p) break;
            char seg[64]; int si = 0;
            while (*p && *p != '/') seg[si++] = *p++;
            seg[si] = 0;
            push_seg(segs, &nseg, seg);
        }
    }
    const char *q = in;
    while (*q)
    {
        while (*q == '/') q++;
        if (!*q) break;
        char seg[64]; int si = 0;
        while (*q && *q != '/') seg[si++] = *q++;
        seg[si] = 0;
        push_seg(segs, &nseg, seg);
    }
    int oi = 0;
    out[oi++] = '/';
    for (int i = 0; i < nseg; i++)
    {
        if (oi > 1) out[oi++] = '/';
        int k = 0;
        while (segs[i][k] && oi < outn - 1) out[oi++] = segs[i][k++];
    }
    out[oi] = 0;
}

struct shell_ls_ctx
{
    void (*out)(void *, const char *);
    void *ctx;
};

static void shell_ls_cb(const struct vfs_stat *st, void *ctx)
{
    struct shell_ls_ctx *lc = ctx;
    char line[96];
    int li = 0;
    line[li++] = (st->type == VFS_DIR) ? 'd' : '-';
    line[li++] = ' ';
    int ni = 0;
    while (st->name[ni] && li < 56) line[li++] = st->name[ni++];
    if (st->type != VFS_DIR)
    {
        line[li++] = ' '; line[li++] = ' ';
        char num[12]; int nli = 0; uint32_t v = st->size;
        if (v == 0) num[nli++] = '0';
        else { char t[12]; int tl = 0; while (v) { t[tl++] = '0' + (v % 10); v /= 10; } while (tl > 0) num[nli++] = t[--tl]; }
        num[nli] = 0;
        int k = 0;
        while (num[k]) line[li++] = num[k++];
    }
    line[li++] = '\n';
    line[li] = 0;
    lc->out(lc->ctx, line);
}

static void con_key(char c);
static void con_exec(const char *line);
static void shell_exec(const char *line, void (*out)(void *, const char *), void *ctx);
static void request_console(void);
static void request_gui(void);

static int is_known(const char *cmd)
{
    return xstrcmp(cmd, "help") == 0 || xstrcmp(cmd, "ver") == 0 || xstrcmp(cmd, "pwd") == 0 ||
           xstrcmp(cmd, "cd") == 0 || xstrcmp(cmd, "ls") == 0 || xstrcmp(cmd, "cat") == 0 ||
           xstrcmp(cmd, "mkdir") == 0 || xstrcmp(cmd, "tee") == 0 || xstrcmp(cmd, "echo") == 0 ||
           xstrcmp(cmd, "clear") == 0 || xstrcmp(cmd, "console") == 0 || xstrcmp(cmd, "gui") == 0;
}

static void help_usage(void (*out)(void *, const char *), void *ctx, const char *cmd)
{
    if (!is_known(cmd))
    {
        out(ctx, "help: no such command '");
        out(ctx, cmd);
        out(ctx, "'\n");
        return;
    }
    if (xstrcmp(cmd, "help") == 0) out(ctx, "usage: help [command]\n  List commands, or show usage of one.\n");
    else if (xstrcmp(cmd, "ver") == 0) out(ctx, "usage: ver\n  Print OS version.\n");
    else if (xstrcmp(cmd, "pwd") == 0) out(ctx, "usage: pwd\n  Print current directory.\n");
    else if (xstrcmp(cmd, "cd") == 0) out(ctx, "usage: cd <dir>\n  Change directory ('cd ..' goes up).\n");
    else if (xstrcmp(cmd, "ls") == 0) out(ctx, "usage: ls [path]\n  List a directory (default: current).\n");
    else if (xstrcmp(cmd, "cat") == 0) out(ctx, "usage: cat <file>\n  Print a file's contents.\n");
    else if (xstrcmp(cmd, "mkdir") == 0) out(ctx, "usage: mkdir <dir>\n  Create a directory.\n");
    else if (xstrcmp(cmd, "tee") == 0) out(ctx, "usage: tee <file> [text]\n  Write text to file (empty if omitted) and echo.\n");
    else if (xstrcmp(cmd, "echo") == 0) out(ctx, "usage: echo <text>\n  Print text.\n");
    else if (xstrcmp(cmd, "clear") == 0) out(ctx, "usage: clear\n  Clear the terminal.\n");
    else if (xstrcmp(cmd, "console") == 0) out(ctx, "usage: console\n  Switch to the text console (no graphics).\n");
    else if (xstrcmp(cmd, "gui") == 0) out(ctx, "usage: gui\n  Return from the text console to the desktop.\n");
}

static int g_console;
static char g_con_line[256];
static int g_con_len;

static void con_emit(void *ctx, const char *s)
{
    (void)ctx;
    if (s[0] == '\f' && s[1] == 0) { console_clear(); return; }
    console_write(s);
}

static void request_console(void)
{
    g_console = 1;
    keyboard_set_sink(con_key);
    console_set_suppress(0);
    console_set_color(0x00E6E6E6, 0x00000000);
    console_clear();
    console_write("UniOS text console\nType 'gui' to return to the desktop.\n\n> ");
    g_con_line[0] = 0;
    g_con_len = 0;
}

static void request_gui(void)
{
    g_console = 0;
    keyboard_set_sink(wm_inject_key);
    console_set_suppress(1);
}

static void con_exec(const char *line)
{
    shell_exec(line, con_emit, 0);
}

static void con_key(char c)
{
    if (c == '\n' || c == '\r')
    {
        console_write("\n");
        con_exec(g_con_line);
        g_con_line[0] = 0;
        g_con_len = 0;
        console_write("> ");
        return;
    }
    if (c == '\b' || c == 0x7F)
    {
        if (g_con_len > 0)
        {
            g_con_len--;
            g_con_line[g_con_len] = 0;
            console_backspace();
        }
        return;
    }
    if (c < 0x20)
        return;
    if (g_con_len < 255)
    {
        g_con_line[g_con_len++] = c;
        g_con_line[g_con_len] = 0;
        char ech[2] = { c, 0 };
        console_write(ech);
    }
}

static void shell_exec(const char *line, void (*out)(void *, const char *), void *ctx)
{
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 0) return;
    char cmd[32]; int ci = 0;
    while (*p && *p != ' ' && *p != '\t' && ci < 31) cmd[ci++] = *p++;
    cmd[ci] = 0;
    while (*p == ' ' || *p == '\t') p++;
    const char *args = p;

    if (xstrcmp(cmd, "console") == 0) { request_console(); return; }
    if (xstrcmp(cmd, "gui") == 0) { request_gui(); return; }

#define EMIT(s) out(ctx, s)
    if (xstrcmp(cmd, "help") == 0)
    {
        if (*args == 0)
            EMIT("Commands: help ver pwd cd ls cat mkdir tee echo clear console gui\nType 'help <command>' for usage.\n");
        else
            help_usage(out, ctx, args);
        return;
    }
    if (xstrcmp(cmd, "ver") == 0) { EMIT("Blabby Co. UniOS [ver 0.1.0]\n"); return; }
    if (xstrcmp(cmd, "pwd") == 0) { EMIT(g_cwd); EMIT("\n"); return; }
    if (xstrcmp(cmd, "clear") == 0) { EMIT("\f"); return; }
    if (xstrcmp(cmd, "echo") == 0) { EMIT(args); EMIT("\n"); return; }

    if (xstrcmp(cmd, "cd") == 0)
    {
        if (*args == 0) { EMIT("cd: missing operand\nusage: cd <dir>\n"); return; }
        char path[256]; resolve_path(args, path, 256);
        struct vfs_stat st;
        if (vfs_stat(path, &st) != 0) { EMIT("cd: "); EMIT(path); EMIT(": No such file or directory\n"); return; }
        if (st.type != VFS_DIR) { EMIT("cd: "); EMIT(path); EMIT(": Not a directory\n"); return; }
        int i = 0; while (path[i] && i < 255) { g_cwd[i] = path[i]; i++; } g_cwd[i] = 0;
        return;
    }
    if (xstrcmp(cmd, "ls") == 0)
    {
        char path[256];
        if (*args == 0) { int i = 0; while (g_cwd[i]) { path[i] = g_cwd[i]; i++; } path[i] = 0; }
        else resolve_path(args, path, 256);
        struct shell_ls_ctx lc; lc.out = out; lc.ctx = ctx;
        if (vfs_list(path, shell_ls_cb, &lc) != 0) { EMIT("ls: "); EMIT(path); EMIT(": No such file or directory\n"); return; }
        return;
    }
    if (xstrcmp(cmd, "cat") == 0)
    {
        if (*args == 0) { EMIT("cat: missing operand\nusage: cat <file>\n"); return; }
        char path[256]; resolve_path(args, path, 256);
        static char rbuf[2048];
        int64_t n = vfs_read(path, rbuf, 2047);
        if (n < 0) { EMIT("cat: "); EMIT(path); EMIT(": No such file or directory\n"); return; }
        rbuf[n] = 0;
        EMIT(rbuf);
        if (n > 0 && rbuf[n - 1] != '\n') EMIT("\n");
        return;
    }
    if (xstrcmp(cmd, "mkdir") == 0)
    {
        if (*args == 0) { EMIT("mkdir: missing operand\nusage: mkdir <dir>\n"); return; }
        char path[256]; resolve_path(args, path, 256);
        if (vfs_mkdir(path) != 0) { EMIT("mkdir: "); EMIT(path); EMIT(": cannot create (exists or error)\n"); return; }
        return;
    }
    if (xstrcmp(cmd, "tee") == 0)
    {
        if (*args == 0) { EMIT("tee: missing operand\nusage: tee <file> [text]\n"); return; }
        const char *a = args;
        while (*a == ' ' || *a == '\t') a++;
        char file[256]; int fi = 0;
        while (*a && *a != ' ' && *a != '\t' && fi < 255) file[fi++] = *a++;
        file[fi] = 0;
        while (*a == ' ' || *a == '\t') a++;
        char path[256]; resolve_path(file, path, 256);
        int textlen = 0; while (a[textlen]) textlen++;
        if (vfs_write(path, a, (uint32_t)textlen) < 0) { EMIT("tee: "); EMIT(path); EMIT(": write failed\n"); return; }
        EMIT(a);
        if (textlen > 0 && a[textlen - 1] != '\n') EMIT("\n");
        return;
    }
    EMIT("? unknown command '");
    EMIT(cmd);
    EMIT("'. type 'help' for usage.\n");
#undef EMIT
}

#define TERM_HIST_MAX 16

static char g_term_hist[TERM_HIST_MAX][256];
static int32_t g_term_hist_count = 0;
static int32_t g_term_hist_view = 0;

static void term_hist_push(const char *line)
{
    if (!line[0]) { g_term_hist_view = g_term_hist_count; return; }
    if (g_term_hist_count > 0 && xstrcmp(g_term_hist[g_term_hist_count - 1], line) == 0)
    {
        g_term_hist_view = g_term_hist_count;
        return;
    }
    if (g_term_hist_count == TERM_HIST_MAX)
    {
        for (int32_t i = 0; i < TERM_HIST_MAX - 1; i++)
        {
            int32_t k = 0;
            while (g_term_hist[i + 1][k]) { g_term_hist[i][k] = g_term_hist[i + 1][k]; k++; }
            g_term_hist[i][k] = 0;
        }
        g_term_hist_count--;
    }
    int32_t k = 0;
    while (line[k] && k < 255) { g_term_hist[g_term_hist_count][k] = line[k]; k++; }
    g_term_hist[g_term_hist_count][k] = 0;
    g_term_hist_count++;
    g_term_hist_view = g_term_hist_count;
}

static void term_set_cmdline(struct UiTextArea *ta, const char *s)
{
    if (ta->min_cursor < 0 || (uint32_t)ta->min_cursor > ta->len) return;
    ta->len = (uint32_t)ta->min_cursor;
    while (*s && ta->len < UI_AREA_MAX - 1) ta->text[ta->len++] = *s++;
    ta->text[ta->len] = 0;
    ta->cy = (int32_t)ta->len;
}

static void term_history_nav(struct UiTextArea *ta, int32_t dir)
{
    if (g_term_hist_count == 0) return;
    int32_t v = g_term_hist_view + dir;
    if (v < 0) v = 0;
    if (v > g_term_hist_count) v = g_term_hist_count;
    if (v == g_term_hist_view) return;
    g_term_hist_view = v;
    if (v == g_term_hist_count) term_set_cmdline(ta, "");
    else term_set_cmdline(ta, g_term_hist[v]);
}

static void term_enter(struct UiTextArea *ta)
{
    int32_t term_protected = term_hdr_len();
    int32_t last_prompt = term_protected;
    for (int32_t i = term_protected; i < (int32_t)ta->len; i++)
        if (ta->text[i] == '\n' && i + 2 < (int32_t)ta->len && ta->text[i+1] == '>' && ta->text[i+2] == ' ')
            last_prompt = i + 3;

    if (ta->cy < last_prompt) { ta->cy = (int32_t)ta->len; return; }
    int32_t start = last_prompt;

    int32_t cmd_len = ta->cy - start;
    if (cmd_len < 0) cmd_len = 0;
    if (cmd_len > 255) cmd_len = 255;
    char line[256];
    int32_t li = 0;
    for (int32_t i = 0; i < cmd_len && li < 255; i++)
    {
        char c = ta->text[start + i];
        if (c == '\n') break;
        line[li++] = c;
    }
    line[li] = 0;

    if (ta->len + 2 < UI_AREA_MAX - 1)
    {
        ta->text[ta->len++] = '\n';
        ta->text[ta->len] = 0;
        ta->cy = (int32_t)ta->len;
    }

    term_hist_push(line);

    shell_exec(line, term_emit, ta);
    term_append_text(ta, "> ");
    ta->min_cursor = (int32_t)ta->len;
}

static void term_append_text(struct UiTextArea *ta, const char *s)
{
    while (*s && ta->len < UI_AREA_MAX - 1) { ta->text[ta->len++] = *s++; }
    ta->text[ta->len] = 0;
    ta->cy = (int32_t)ta->len;
    if (ta->follow_bottom) ui_textarea_scroll_to_bottom(ta);
}

static void term_emit(void *ctx, const char *s)
{
    struct UiTextArea *ta = ctx;
    if (s[0] == '\f' && s[1] == 0)
    {
        int32_t h = term_hdr_len();
        ta->len = (uint32_t)h; ta->cy = h; ta->text[ta->len] = 0;
        return;
    }
    term_append_text(ta, s);
}

static void run_dlg_ensure(void);

static void act_start_run(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0;
    wm_mark_dirty();
    run_dlg_ensure();
    if (g_run_dlg)
    {
        wm_show(g_run_dlg, 1);
        wm_focus(g_run_dlg);
    }
}

static void calc_paint(struct UiWindow *win, struct gfx_surface *s);
static void note_paint(struct UiWindow *win, struct gfx_surface *s);
static void term_paint(struct UiWindow *win, struct gfx_surface *s);
static void clock_paint(struct UiWindow *win, struct gfx_surface *s);
static void hello_paint(struct UiWindow *win, struct gfx_surface *s);
static void calc_widgets(struct UiWindow *win);
static void note_widgets(struct UiWindow *win);
static void term_widgets(struct UiWindow *win);
static void sys_widgets(struct UiWindow *win);
static void files_widgets(struct UiWindow *win);
static void ctl_widgets(struct UiWindow *win);

static void act_menu_sys(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0;
    wm_mark_dirty();
    if (!g_win_sys || !g_win_sys->title[0])
    {
        g_win_sys = wm_create_window(locale_get("win.sys_monitor"), 40, 50, 420, 300, COL_ACCENT, sys_paint);
        if (g_win_sys) { sys_widgets(g_win_sys); wm_window_set_close(g_win_sys, sys_on_close); }
    }
    if (g_win_sys) { wm_show(g_win_sys, 1); wm_focus(g_win_sys); }
}

static void act_menu_files(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0;
    wm_mark_dirty();
    if (!g_win_files || !g_win_files->title[0])
    {
        g_win_files = wm_create_window(locale_get("win.files"), 490, 50, 360, 330, COL_ACCENT, files_paint);
        if (g_win_files) { files_widgets(g_win_files); wm_window_set_close(g_win_files, files_on_close); }
        file_nav("/var");
    }
    if (g_win_files) { wm_show(g_win_files, 1); wm_focus(g_win_files); }
}

static void act_menu_ctl(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0;
    wm_mark_dirty();
    if (!g_win_ctl || !g_win_ctl->title[0])
    {
        g_win_ctl = wm_create_window(locale_get("win.control_center"), 880, 60, 360, 380, COL_ACCENT, ctl_paint);
        if (g_win_ctl) { ctl_widgets(g_win_ctl); wm_window_set_close(g_win_ctl, ctl_on_close); }
    }
    if (g_win_ctl) { wm_show(g_win_ctl, 1); wm_focus(g_win_ctl); }
}

static void act_menu_hello(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0;
    wm_mark_dirty();
    if (!g_win_hello || !g_win_hello->title[0])
        g_win_hello = wm_create_window(locale_get("win.hello"), 40, 390, 380, 250, COL_ACCENT, hello_paint);
    if (g_win_hello) { wm_show(g_win_hello, 1); wm_focus(g_win_hello); }
}

static void calc_paint(struct UiWindow *win, struct gfx_surface *s);
static void note_paint(struct UiWindow *win, struct gfx_surface *s);
static void term_paint(struct UiWindow *win, struct gfx_surface *s);
static void clock_paint(struct UiWindow *win, struct gfx_surface *s);

static void act_menu_app(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0;
    wm_mark_dirty();
}

static void act_menu_calc(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0; wm_mark_dirty();
    guisys_focus_by_title("Calculator");
}
static void act_menu_note(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0; wm_mark_dirty();
    if (!g_win_note || !g_win_note->title[0])
    {
        g_win_note = wm_create_window("Untitled", 860, 20, 380, 320, COL_ACCENT, note_paint);
        if (g_win_note) note_widgets(g_win_note);
        g_note_filename[0] = 0;
        g_note_filepath[0] = 0;
        g_note_dirty = 0;
    }
    if (g_win_note) { wm_show(g_win_note, 1); wm_focus(g_win_note); }
}
static void act_menu_term(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0; wm_mark_dirty();
    if (!g_win_term || !g_win_term->title[0])
    {
        g_win_term = wm_create_window(locale_get("win.terminal"), 440, 380, 400, 340, COL_ACCENT, term_paint);
        if (g_win_term) { term_widgets(g_win_term); wm_window_set_close(g_win_term, term_on_close); }
    }
    if (g_win_term) { wm_show(g_win_term, 1); wm_focus(g_win_term); }
}
static void act_menu_clk(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0; wm_mark_dirty();
    guisys_focus_by_title("Clock");
}
static void appmgr_paint(struct UiWindow *win, struct gfx_surface *s);

static void act_menu_appmgr(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    g_start_menu.visible = 0;
    wm_mark_dirty();
    if (!g_win_appmgr || !g_win_appmgr->title[0])
        g_win_appmgr = wm_create_window(locale_get("menu.app_mgr"), 880, 390, 360, 300, COL_ACCENT, appmgr_paint);
    if (g_win_appmgr) { wm_show(g_win_appmgr, 1); wm_focus(g_win_appmgr); }
}

static void overlay_paint(struct gfx_surface *s)
{
    ui_startmenu_draw(&g_start_menu, s);
}

static int overlay_click(int32_t x, int32_t y)
{
    if (!g_start_menu.visible)
        return 0;
    return ui_startmenu_click(&g_start_menu, x, y);
}

static void overlay_move(int32_t x, int32_t y)
{
    ui_startmenu_move(&g_start_menu, x, y);
}

static void hello_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, s->w, s->h, 0x001A1A2E);
    gfx_vgrad(s, 14, 14, s->w - 28, 44, 0x00417AC0, 0x001A1A2E);
    gfx_text(s, 22, 24, "UniOS Chinese Test", 0x00FFFFFF);
    gfx_text(s, 30, 80, "Hello \xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c", 0x00FFD27F);
    gfx_text(s, 30, 120, "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c", 0x00FFFFFF);
    gfx_text(s, 30, 170, "ni hao shi jie", 0x00888888);
}

static const char *g_pkgs[5] = {
    "com.BlabbyCo.calculator",
    "com.BlabbyCo.notepad",
    "com.BlabbyCo.terminal",
    "com.BlabbyCo.clock",
    0
};

static int g_pkg_installed[4] = {1, 1, 1, 1};

static void appmgr_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
    gfx_text(s, 16, 14, locale_get("menu.app_mgr"), 0x00417AC0);
    gfx_fill(s, 16, 34, (int32_t)s->w - 32, 1, COL_DIVIDER);
    int32_t y = 50;
    for (int i = 0; i < 4; i++)
    {
        gfx_text(s, 16, y, g_pkgs[i], g_pkg_installed[i] ? 0x004A9E6E : COL_MUTED);
        char buf[20];
        uint32_t p = 0;
        s2s(buf, &p, g_pkg_installed[i] ? "  [installed]" : "  [not installed]");
        gfx_text(s, 16, y + 18, buf, COL_MUTED);
        y += 44;
    }
}

static int g_last_click_row = -1;
static uint64_t g_last_click_tick = 0;

static int files_on_click(struct UiWindow *win, int32_t cx, int32_t cy)
{
    (void)win;
    int row = (cy - 44) / 20;
    if (row < 0 || cy < 44) return 0;
    extern uint64_t sched_ticks(void);
    uint64_t now = sched_ticks();
    if (row == 0)
    {
        if (g_last_click_row == 0 && (now - g_last_click_tick) < 30)
        {
            file_go_up();
            g_last_click_row = -1;
            g_filesel = -1;
        }
        else
        {
            g_filesel = -2;
            g_last_click_row = 0;
            g_last_click_tick = now;
        }
        return 1;
    }
    int idx = row - 1;
    if ((uint32_t)idx >= g_nrows) { g_last_click_row = -1; return 0; }
    if (g_last_click_row == row && (now - g_last_click_tick) < 30)
    {
        g_filesel = idx;
        if (g_rows[idx].type == VFS_DIR)
        {
            file_enter();
        }
        else
        {
            char fpath[192];
            uint32_t fn = 0;
            while (g_filepath[fn] && fn < 127) { fpath[fn] = g_filepath[fn]; fn++; }
            if (fn > 0 && fpath[fn-1] != '/' && fn < 191) { fpath[fn] = '/'; fn++; }
            uint32_t fj = 0;
            while (g_rows[idx].name[fj] && (fn + fj) < 191)
            {
                fpath[fn + fj] = g_rows[idx].name[fj];
                fj++;
            }
            fpath[fn + fj] = 0;
            note_open_file(fpath, g_rows[idx].name);
        }
        g_last_click_row = -1;
        return 1;
    }
    g_filesel = idx;
    g_last_click_row = row;
    g_last_click_tick = now;
    return 1;
}

static void act_file_go(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    if (g_file_path_inp.len == 0) return;
    file_nav(g_file_path_inp.text);
    memset(g_file_path_inp.text, 0, UI_TEXT_MAX);
    g_file_path_inp.len = 0;
}

static void calc_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
    gfx_text(s, 16, 20, "UniOS Calculator", COL_MUTED);
    gfx_fill(s, 16, 38, (int32_t)s->w - 32, 1, COL_DIVIDER);
}

static void note_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
}

static void note_about_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
    gfx_text(s, 16, 16, "UniOS Notepad", 0x00417AC0);
    gfx_fill(s, 16, 34, (int32_t)s->w - 32, 1, COL_DIVIDER);
    gfx_text(s, 16, 48, "A simple kernel text editor.", COL_TEXT);
    gfx_text(s, 16, 72, "Blabby Co. UniOS", COL_MUTED);
}

static void note_update_title(void)
{
    char title[WM_TITLE_MAX];
    uint32_t p = 0;
    if (g_note_filename[0])
    {
        while (g_note_filename[p] && p < WM_TITLE_MAX - 2)
        { title[p] = g_note_filename[p]; p++; }
    }
    else
    {
        const char *u = "Untitled";
        while (*u && p < WM_TITLE_MAX - 2) title[p++] = *u++;
    }
    if (g_note_dirty && p < WM_TITLE_MAX - 1)
        title[p++] = '*';
    title[p] = 0;
    if (g_win_note)
        wm_window_set_title(g_win_note, title);
}

static void note_on_change(struct UiTextArea *ta)
{
    (void)ta;
    if (!g_note_dirty)
    {
        g_note_dirty = 1;
        note_update_title();
    }
}

static void note_open_file(const char *path, const char *name)
{
    if (!g_win_note || !g_win_note->title[0])
    {
        g_win_note = wm_create_window("Untitled", 860, 20, 380, 320, COL_ACCENT, note_paint);
        if (g_win_note) note_widgets(g_win_note);
    }
    if (!g_win_note) return;

    static char rbuf[UI_AREA_MAX];
    int64_t n = vfs_read(path, rbuf, UI_AREA_MAX - 1);
    if (n < 0) n = 0;
    rbuf[n] = 0;
    ui_textarea_set_text(&g_note_area, rbuf);
    g_note_area.cy = 0;
    g_note_area.scroll_y = 0;
    g_note_area.min_cursor = 0;

    uint32_t i = 0;
    while (name[i] && i < 63) { g_note_filename[i] = name[i]; i++; }
    g_note_filename[i] = 0;

    i = 0;
    while (path[i] && i < 127) { g_note_filepath[i] = path[i]; i++; }
    g_note_filepath[i] = 0;

    g_note_dirty = 0;
    note_update_title();

    wm_show(g_win_note, 1);
    wm_focus(g_win_note);
    wm_mark_dirty();
}

static void note_menu_new(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    ui_textarea_set_text(&g_note_area, "");
    g_note_area.cy = 0;
    g_note_area.scroll_y = 0;
    g_note_area.min_cursor = 0;
    g_note_filename[0] = 0;
    g_note_filepath[0] = 0;
    g_note_dirty = 0;
    note_update_title();
}

static void note_menu_about(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    wm_create_dialog("About Notepad", 300, 180, 340, 150, note_about_paint);
}

static void note_conf_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
    gfx_text(s, 16, 16, "Unsaved Changes", 0x00417AC0);
    gfx_fill(s, 16, 34, (int32_t)s->w - 32, 1, COL_DIVIDER);
    gfx_text(s, 16, 48, "Save before closing?", COL_TEXT);
}

static void ensure_filedlg(void)
{
    if (!g_filedlg)
    {
        g_filedlg = udll_load("/lib/filedlg.udll");
        if (g_filedlg)
            g_filedlg_show = (fdlg_show_t)udll_get_proc(g_filedlg, "file_dialog_show");
    }
}

static void note_file_open_cb(int result, const char *path, int encoding, void *ctx)
{
    (void)ctx;
    (void)encoding;
    if (result == 1 && path)
    {
        const char *name = path;
        const char *p = path;
        while (*p) { if (*p == '/') name = p + 1; p++; }
        note_open_file(path, name);
    }
    wm_mark_dirty();
}

static void note_file_save_cb(int result, const char *path, int encoding, void *ctx)
{
    (void)ctx;
    (void)encoding;
    if (result == 1 && path)
    {
        int rc = vfs_write(path, g_note_area.text, g_note_area.len);
        if (rc == 0)
        {
            const char *name = path;
            const char *p = path;
            while (*p) { if (*p == '/') name = p + 1; p++; }
            uint32_t i = 0;
            while (name[i] && i < 63) { g_note_filename[i] = name[i]; i++; }
            g_note_filename[i] = 0;
            i = 0;
            while (path[i] && i < 127) { g_note_filepath[i] = path[i]; i++; }
            g_note_filepath[i] = 0;
            g_note_dirty = 0;
            note_update_title();
            if (g_note_force_close)
            {
                g_note_force_close = 0;
                if (g_win_note) wm_close_window(g_win_note);
            }
        }
    }
    else
    {
        g_note_force_close = 0;
    }
    wm_mark_dirty();
}

static void note_show_saveas(void)
{
    ensure_filedlg();
    if (g_filedlg_show)
    {
        char init[192];
        uint32_t i = 0;
        if (g_note_filepath[0])
        {
            while (g_note_filepath[i] && i < 191) { init[i] = g_note_filepath[i]; i++; }
        }
        else
        {
            const char *d = "/var/documents/text.txt";
            while (d[i] && i < 191) { init[i] = d[i]; i++; }
        }
        init[i] = 0;
        g_filedlg_show(1, "Save File", init, "Save", "Cancel", note_file_save_cb, 0);
    }
    wm_mark_dirty();
}

static void note_show_open(void)
{
    ensure_filedlg();
    if (g_filedlg_show)
        g_filedlg_show(0, "Open File", "/", "Open", "Cancel", note_file_open_cb, 0);
    wm_mark_dirty();
}

static void note_conf_save_click(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    if (g_note_conf_dlg) { wm_destroy_window(g_note_conf_dlg); g_note_conf_dlg = 0; }
    g_note_force_close = 1;
    note_do_save();
    wm_mark_dirty();
}

static void note_conf_notsave_click(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    if (g_note_conf_dlg) { wm_destroy_window(g_note_conf_dlg); g_note_conf_dlg = 0; }
    g_note_force_close = 1;
    if (g_win_note) wm_close_window(g_win_note);
    wm_mark_dirty();
}

static void note_conf_cancel_click(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    if (g_note_conf_dlg) { wm_destroy_window(g_note_conf_dlg); g_note_conf_dlg = 0; }
    g_note_force_close = 0;
    wm_mark_dirty();
}

static void note_show_confirm(void)
{
    if (g_note_conf_dlg) { wm_show(g_note_conf_dlg, 1); wm_focus(g_note_conf_dlg); return; }
    g_note_conf_dlg = wm_create_dialog("Confirm Close", 420, 220, 340, 150, note_conf_paint);
    if (!g_note_conf_dlg) return;
    wm_window_set_close(g_note_conf_dlg, note_conf_dlg_onclose);
    ui_button_init(&g_conf_save, 16, 80, 90, 28, "Save", COL_ACCENT);
    g_conf_save.base.action = note_conf_save_click;
    wm_window_add_widget(g_note_conf_dlg, &g_conf_save.base);
    wm_window_anchor(g_note_conf_dlg, &g_conf_save.base, UI_ANCHOR_BOTTOM_LEFT);
    ui_button_init(&g_conf_notsave, 120, 80, 110, 28, "Don't Save", COL_DIVIDER);
    g_conf_notsave.base.action = note_conf_notsave_click;
    wm_window_add_widget(g_note_conf_dlg, &g_conf_notsave.base);
    wm_window_anchor(g_note_conf_dlg, &g_conf_notsave.base, UI_ANCHOR_BOTTOM_LEFT);
    ui_button_init(&g_conf_cancel, 250, 80, 70, 28, "Cancel", COL_DIVIDER);
    g_conf_cancel.base.action = note_conf_cancel_click;
    wm_window_add_widget(g_note_conf_dlg, &g_conf_cancel.base);
    wm_window_anchor(g_note_conf_dlg, &g_conf_cancel.base, UI_ANCHOR_BOTTOM_RIGHT);
    wm_show(g_note_conf_dlg, 1);
    wm_focus(g_note_conf_dlg);
}

static int note_conf_dlg_onclose(struct UiWindow *win)
{
    (void)win;
    g_note_conf_dlg = 0;
    g_note_force_close = 0;
    return 0;
}

static int note_on_close(struct UiWindow *win)
{
    (void)win;
    if (g_note_force_close) { g_note_force_close = 0; g_win_note = 0; return 0; }
    if (g_note_dirty) { note_show_confirm(); return 1; }
    g_win_note = 0;
    return 0;
}

static int files_on_close(struct UiWindow *win) { (void)win; g_win_files = 0; return 0; }
static int term_on_close(struct UiWindow *win) { (void)win; g_win_term = 0; return 0; }
static int ctl_on_close(struct UiWindow *win) { (void)win; g_win_ctl = 0; return 0; }
static int sys_on_close(struct UiWindow *win) { (void)win; g_win_sys = 0; return 0; }

static void note_do_save(void)
{
    if (!g_win_note) return;
    if (g_note_filepath[0])
    {
        int rc = vfs_write(g_note_filepath, g_note_area.text, g_note_area.len);
        if (rc == 0)
        {
            g_note_dirty = 0;
            note_update_title();
        }
        if (g_note_force_close)
        {
            g_note_force_close = 0;
            wm_close_window(g_win_note);
        }
    }
    else
    {
        note_show_saveas();
    }
}

static void note_menu_save(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    note_do_save();
    wm_mark_dirty();
}

static void note_menu_save_as(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    note_show_saveas();
    wm_mark_dirty();
}

static void note_menu_open(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    note_show_open();
    wm_mark_dirty();
}

static void note_menu_exit(struct UiWidget *w, void *ctx)
{
    (void)w; (void)ctx;
    if (g_win_note) wm_close_window(g_win_note);
    wm_mark_dirty();
}

static void term_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, 0x00000000);
}

static void clock_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    rtc_time rt;
    rtc_read(&rt);
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, COL_CONTENT);
    char buf[20];
    uint32_t p = 0;
    if (rt.hour < 10) buf[p++] = '0';
    u2s(buf, &p, rt.hour);
    buf[p++] = ':';
    if (rt.min < 10) buf[p++] = '0';
    u2s(buf, &p, rt.min);
    buf[p++] = ':';
    if (rt.sec < 10) buf[p++] = '0';
    u2s(buf, &p, rt.sec);
    buf[p] = 0;
    int32_t tw = gfx_text_width(buf);
    gfx_text(s, ((int32_t)s->w - tw) / 2, ((int32_t)s->h - 20) / 2, buf, 0x00FFD27F);
    char dbuf[20];
    p = 0;
    u2s(dbuf, &p, rt.year);
    dbuf[p++] = '-';
    if (rt.month < 10) dbuf[p++] = '0';
    u2s(dbuf, &p, rt.month);
    dbuf[p++] = '-';
    if (rt.day < 10) dbuf[p++] = '0';
    u2s(dbuf, &p, rt.day);
    dbuf[p] = 0;
    tw = gfx_text_width(dbuf);
    gfx_text(s, ((int32_t)s->w - tw) / 2, ((int32_t)s->h - 20) / 2 + 30, dbuf, COL_MUTED);
}

static void sys_widgets(struct UiWindow *win)
{
    ui_button_init(&g_btn_mark, 16, 220, 110, 32, locale_get("btn.mark"), COL_ACCENT);
    g_btn_mark.base.action = act_mark;
    ui_widget_set_tip(&g_btn_mark.base, "tip.mark");
    wm_window_add_widget(win, &g_btn_mark.base);
}

static void files_widgets(struct UiWindow *win)
{
    ui_textinput_init(&g_file_path_inp, 4, 4, (int32_t)win->content.w - 50, "/");
    g_file_path_inp.base.action = act_file_go;
    wm_window_add_widget(win, &g_file_path_inp.base);
    wm_window_anchor(win, &g_file_path_inp.base, UI_ANCHOR_TOP_FILL);
    ui_button_init(&g_file_go_btn, (int32_t)win->content.w - 44, 4, 40, 28, "Go", COL_ACCENT);
    g_file_go_btn.base.action = act_file_go;
    ui_widget_set_tip(&g_file_go_btn.base, "tip.go");
    wm_window_add_widget(win, &g_file_go_btn.base);
    wm_window_anchor(win, &g_file_go_btn.base, UI_ANCHOR_TOP_RIGHT);
    wm_window_set_click(win, files_on_click);
}

static void ctl_widgets(struct UiWindow *win)
{
    ui_toggle_init(&g_tgl_grid, 16, 44, locale_get("label.grid_overlay"), 0);
    g_tgl_grid.base.action = act_toggle;
    wm_window_add_widget(win, &g_tgl_grid.base);
    ui_toggle_init(&g_tgl_accent, 16, 78, locale_get("label.accent_line"), 1);
    g_tgl_accent.base.action = act_toggle;
    wm_window_add_widget(win, &g_tgl_accent.base);
    ui_widget_set_tip(&g_tgl_grid.base, "tip.grid_overlay");
    ui_widget_set_tip(&g_tgl_accent.base, "tip.accent_line");
    ui_textinput_init(&g_input_note, 16, 138, 260, locale_get("label.type_note"));
    wm_window_add_widget(win, &g_input_note.base);
    ui_button_init(&g_btn_save, 16, 176, 130, 32, locale_get("label.save_note"), COL_ACCENT);
    g_btn_save.base.action = act_save;
    ui_widget_set_tip(&g_btn_save.base, "tip.save");
    wm_window_add_widget(win, &g_btn_save.base);
    ui_dropdown_init(&g_dd_res, 16, 236, 150);
    ui_dropdown_add(&g_dd_res, "1024x768");
    ui_dropdown_add(&g_dd_res, "1280x720");
    ui_dropdown_add(&g_dd_res, "1920x1080");
    ui_dropdown_add(&g_dd_res, "640x480");
    g_dd_res.selected = g_cur_res;
    g_dd_res.base.action = act_res_dd;
    ui_widget_set_tip(&g_dd_res.base, "tip.resolution");
    wm_window_add_widget(win, &g_dd_res.base);
    ui_dropdown_init(&g_dd_lang, 16, 268, 150);
    ui_dropdown_add(&g_dd_lang, "English  [en]");
    ui_dropdown_add(&g_dd_lang, "\xe4\xb8\xad\xe6\x96\x87  [zh]");
    g_dd_lang.selected = (locale_get_lang() == LANG_ZH) ? 1 : 0;
    g_dd_lang.base.action = act_lang_dd;
    ui_widget_set_tip(&g_dd_lang.base, "tip.language");
    wm_window_add_widget(win, &g_dd_lang.base);
}

static void calc_widgets(struct UiWindow *win)
{
    ui_textinput_init(&g_calc_disp, 8, 48, (int32_t)win->content.w - 16, "");
    wm_window_add_widget(win, &g_calc_disp.base);
    static const char *clbls[16] = {"7","8","9","/","4","5","6","*","1","2","3","-","0","C","=","+"};
    for (int i = 0; i < 16; i++)
    {
        ui_button_init(&g_calc_btns[i], 10 + (i % 4) * 66, 88 + (i / 4) * 40, 60, 32, clbls[i], COL_ACCENT);
        wm_window_add_widget(win, &g_calc_btns[i].base);
    }
}

static void note_widgets(struct UiWindow *win)
{
    ui_menubar_init(&g_note_menu, 0, 0, (int32_t)win->content.w);
    int mf = ui_menubar_add_menu(&g_note_menu, "File");
    ui_menubar_add_item(&g_note_menu, mf, "New", note_menu_new);
    ui_menubar_add_item(&g_note_menu, mf, "Open", note_menu_open);
    ui_menubar_add_item(&g_note_menu, mf, "Save", note_menu_save);
    ui_menubar_add_item(&g_note_menu, mf, "Save As", note_menu_save_as);
    ui_menubar_add_item(&g_note_menu, mf, "Exit", note_menu_exit);
    int mt = ui_menubar_add_menu(&g_note_menu, "Tools");
    ui_menubar_add_item(&g_note_menu, mt, "About", note_menu_about);
    wm_window_add_widget(win, &g_note_menu.base);
    wm_window_anchor(win, &g_note_menu.base, UI_ANCHOR_TOP_FILL);

    ui_textarea_init(&g_note_area, 8, 32, (int32_t)win->content.w - 24, (int32_t)win->content.h - 40);
    g_note_area.on_change = note_on_change;
    wm_window_add_widget(win, &g_note_area.base);
    wm_window_anchor(win, &g_note_area.base, UI_ANCHOR_FILL);
    ui_scrollbar_init(&g_note_sb, (int32_t)win->content.w - 18, 32, (int32_t)win->content.h - 40, 100);
    g_note_sb.target = &g_note_area;
    wm_window_add_widget(win, &g_note_sb.base);
    wm_window_anchor(win, &g_note_sb.base, UI_ANCHOR_RIGHT_FILL);

    wm_window_set_close(win, note_on_close);
}

static void term_widgets(struct UiWindow *win)
{
    ui_textarea_init(&g_term_out, 8, 32, (int32_t)win->content.w - 24, (int32_t)win->content.h - 40);
    ui_textarea_set_text(&g_term_out, "Blabby Co. UniOS [ver 0.1.0]\n(c) Blabby Co. All rights reserved.\n\n> ");
    g_term_out.on_enter = term_enter;
    g_term_out.on_history = term_history_nav;
    g_term_out.min_cursor = (int32_t)g_term_out.len;
    g_term_out.follow_bottom = 1;
    g_term_hist_view = g_term_hist_count;
    wm_window_add_widget(win, &g_term_out.base);
    wm_window_anchor(win, &g_term_out.base, UI_ANCHOR_FILL);
    ui_scrollbar_init(&g_term_sb, (int32_t)win->content.w - 18, 32, (int32_t)win->content.h - 40, 100);
    g_term_sb.target = &g_term_out;
    wm_window_add_widget(win, &g_term_sb.base);
    wm_window_anchor(win, &g_term_sb.base, UI_ANCHOR_RIGHT_FILL);
}

int desktop_init(uint32_t screen_w, uint32_t screen_h)
{
    g_sw = screen_w;
    g_sh = screen_h;

    g_wall.w = screen_w;
    g_wall.h = screen_h;
    g_wall.pitch = screen_w * 4;
    g_wall.base = kmalloc((uint64_t)screen_w * screen_h * 4);
    if (!g_wall.base)
        return -1;
    wall_build();

    locale_init();

    char cfg[128];
    int64_t n = vfs_read("/etc/unios.ini", cfg, 127);
    if (n > 0)
    {
        cfg[n] = 0;
        if (strstr(cfg, "lang=zh")) locale_set_lang(LANG_ZH);
        char *rp = strstr(cfg, "res=");
        if (rp)
        {
            rp += 4;
            for (int i = 0; i < 4; i++)
            {
                if (strncmp(rp, g_res_names[i], strlen(g_res_names[i])) == 0)
                {
                    g_cur_res = i;
                    break;
                }
            }
        }
        kprintf("config loaded: %d bytes\n", (int)n);
    }

    wm_set_background(bg_paint);
    wm_set_taskbar(BAR_H, bar_paint, bar_click);

    g_svg = svg_init();

    g_win_hello = wm_create_window("Hello UniOS", 40, 390, 380, 250, COL_ACCENT, hello_paint);
    if (!g_win_hello)
        return -2;

    if (g_svg)
    {
        (void)g_svg;
        kprintf("SVG engine online, icons pending\n");
    }

    ui_startmenu_init(&g_start_menu, 4, (int32_t)g_sh - BAR_H - 356, 220);
    ui_startmenu_add(&g_start_menu, locale_get("dlg.run_title"), act_start_run);
    ui_startmenu_add(&g_start_menu, locale_get("win.sys_monitor"), act_menu_sys);
    ui_startmenu_add(&g_start_menu, locale_get("win.files"), act_menu_files);
    ui_startmenu_add(&g_start_menu, locale_get("win.control_center"), act_menu_ctl);
    ui_startmenu_add(&g_start_menu, locale_get("win.hello"), act_menu_hello);
    ui_startmenu_add(&g_start_menu, locale_get("win.calculator"), act_menu_calc);
    ui_startmenu_add(&g_start_menu, locale_get("win.notepad"), act_menu_note);
    ui_startmenu_add(&g_start_menu, locale_get("win.terminal"), act_menu_term);
    ui_startmenu_add(&g_start_menu, locale_get("win.clock"), act_menu_clk);
    ui_startmenu_add(&g_start_menu, locale_get("menu.app_mgr"), act_menu_appmgr);
    wm_set_overlay(overlay_paint);
    wm_set_overlay_click(overlay_click);

    uint32_t p = 0;
    s2s(g_note_status, &p, "no note saved yet");

    wm_focus(g_win_hello);
    tooltip_init();

    return 0;
}

static void run_dlg_ensure(void)
{
    if (g_run_dlg && g_run_dlg->title[0])
        return;
    g_run_dlg = wm_create_dialog(locale_get("dlg.run_title"), 20, (int32_t)g_sh - BAR_H - 140, 360, 120, run_dlg_paint);
    if (g_run_dlg)
    {
        ui_textinput_init(&g_run_input, 16, 36, 240, locale_get("dlg.run_placeholder"));
        g_run_input.base.action = act_run_cmd;
        wm_window_add_widget(g_run_dlg, &g_run_input.base);
        ui_button_init(&g_run_btn, 264, 36, 70, 28, locale_get("btn.run"), COL_ACCENT);
        g_run_btn.base.action = act_run_cmd;
        ui_widget_set_tip(&g_run_btn.base, "tip.run");
        wm_window_add_widget(g_run_dlg, &g_run_btn.base);
        wm_show(g_run_dlg, 0);
        kprintf("gui: run dialog created (lazy)\n");
    }
}

static const char *tooltip_resolve(int32_t mx, int32_t my)
{
    if (g_start_menu.visible)
    {
        int idx = ui_startmenu_item_at(&g_start_menu, mx, my);
        if (idx >= 0 && g_start_menu.items[idx].label[0])
            return g_start_menu.items[idx].label;
    }

    int32_t y0 = (int32_t)g_sh - BAR_H;
    if (my >= y0)
    {
        if (mx < 110)
            return locale_get("tip.start");
        for (int i = 0; i < g_bar_count; i++)
        {
            int32_t bx = 110 + i * 104;
            if (mx >= bx && mx < bx + 96 && g_bar_items[i])
                return g_bar_items[i]->title;
        }
        return 0;
    }

    struct UiWindow *win = wm_window_at(mx, my);
    if (win)
    {
        if (my < win->y + WM_TITLE_H)
        {
            int32_t my2 = win->y + WM_TITLE_H / 2;
            if (close_hit(mx, my, win->x + win->w - 52, my2)) return locale_get("tip.btn_min");
            if (close_hit(mx, my, win->x + win->w - 34, my2)) return locale_get("tip.btn_max");
            if (close_hit(mx, my, win->x + win->w - 16, my2)) return locale_get("tip.btn_close");
            return win->title;
        }
        int32_t lx = mx - win->x - 1;
        int32_t ly = my - win->y - WM_TITLE_H;
        for (struct UiWidget *w = win->widgets; w; w = w->next)
        {
            if (!ui_widget_hit(w, lx, ly))
                continue;
            if (w->tip && w->tip[0])
                return locale_get(w->tip);
            const char *def = ui_widget_default_tip(w);
            if (def && def[0])
                return def;
        }
    }
    return 0;
}

int desktop_in_console(void)
{
    return g_console;
}

void desktop_tick(uint64_t tick)
{
    if (g_console) return;
    wm_pump_hardware();

    extern int32_t mouse_x(void);
    extern int32_t mouse_y(void);
    if (g_start_menu.visible)
    {
        overlay_move(mouse_x(), mouse_y());
        wm_mark_dirty();
    }

    if (keyboard_hotkey_ctrl_r())
    {
        run_dlg_ensure();
        if (g_run_dlg)
        {
            wm_show(g_run_dlg, 1);
            wm_focus(g_run_dlg);
        }
    }

    if (keyboard_hotkey_ctrl_esc())
    {
        g_start_menu.visible = !g_start_menu.visible;
        wm_mark_dirty();
    }

    if (keyboard_hotkey_ctrl_s())
    {
        if (g_win_note && wm_focused() == g_win_note)
            note_do_save();
    }

    if (tick - g_last_hb >= 100)
    {
        g_last_hb = tick;
        wm_mark_dirty();
    }

    ui_scrollbar_sync(&g_term_sb, &g_term_out);
    ui_scrollbar_sync(&g_note_sb, &g_note_area);

    int32_t tx = mouse_x();
    int32_t ty = mouse_y();
    tooltip_set(tooltip_resolve(tx, ty), tx, ty, tick);
}

int desktop_autotest_done(void)
{
    return 1;
}
