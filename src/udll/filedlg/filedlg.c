#include <stdint.h>
#include "wm.h"
#include "ui.h"
#include "gfx.h"
#include "vfs.h"
#include "kheap.h"
#include "sched.h"
#include "util.h"

#define FD_OPEN 0
#define FD_SAVE 1

#define FD_MAX_ROWS 32
#define FD_PATH_MAX 128

#define FD_COL_BG 0x00222222
#define FD_COL_TEXT 0x00E6E6E6
#define FD_COL_MUTED 0x00888888
#define FD_COL_DIV 0x00333333
#define FD_COL_ACC 0x00417AC0
#define FD_COL_SEL 0x003A3A3A

#define FD_ENC_UTF8  0
#define FD_ENC_ASCII 1
#define FD_ENC_UTF16 2

typedef void (*FileDialogCallback)(int result, const char *path, int encoding, void *ctx);

struct fd_row
{
    char name[48];
    uint32_t size;
    uint8_t type;
};

static struct UiWindow *fd_win;
static struct UiTextInput fd_name;
static struct UiDropdown fd_enc;
static struct UiButton fd_ok;
static struct UiButton fd_cancel;
static int fd_mode;
static int fd_sel;
static char fd_path[FD_PATH_MAX];
static char fd_result[192];
static struct fd_row fd_rows[FD_MAX_ROWS];
static uint32_t fd_nrows;
static int fd_last_row;
static uint64_t fd_last_tick;
static FileDialogCallback fd_cb;
static void *fd_ctx;
static int fd_active;
static int fd_has_enc;

static void fd_paint(struct UiWindow *win, struct gfx_surface *s);
static int fd_on_click(struct UiWindow *win, int32_t cx, int32_t cy);
static int fd_on_close(struct UiWindow *win);
static void fd_ok_click(struct UiWidget *w, void *ctx);
static void fd_cancel_click(struct UiWidget *w, void *ctx);
static void fd_list_cb(const struct vfs_stat *st, void *ctx);
static void fd_nav(const char *path);
static void fd_go_up(void);
static void fd_enter(void);
static void fd_build_result(int idx);
static void fd_finish(int result);

static void fd_list_cb(const struct vfs_stat *st, void *ctx)
{
    (void)ctx;
    if (fd_nrows >= FD_MAX_ROWS - 1)
        return;
    uint32_t i = 0;
    while (st->name[i] && i < 47)
    {
        fd_rows[fd_nrows].name[i] = st->name[i];
        i++;
    }
    fd_rows[fd_nrows].name[i] = 0;
    fd_rows[fd_nrows].size = st->size;
    fd_rows[fd_nrows].type = st->type;
    fd_nrows++;
}

static void fd_nav(const char *path)
{
    uint32_t i = 0;
    while (path[i] && i < FD_PATH_MAX - 1)
    {
        fd_path[i] = path[i];
        i++;
    }
    fd_path[i] = 0;
    fd_nrows = 0;
    fd_sel = -1;
    vfs_list(path, fd_list_cb, 0);
}

static void fd_go_up(void)
{
    uint32_t n = 0;
    while (fd_path[n])
        n++;
    if (n <= 1)
        return;
    uint32_t cut = n - 1;
    while (cut > 0 && fd_path[cut] != '/')
        cut--;
    if (cut == 0)
        fd_path[1] = 0;
    else
        fd_path[cut] = 0;
    fd_nav(fd_path);
}

static void fd_enter(void)
{
    if (fd_sel < 0 || (uint32_t)fd_sel >= fd_nrows)
        return;
    if (fd_rows[fd_sel].type != VFS_DIR)
        return;
    uint32_t n = 0;
    while (fd_path[n])
        n++;
    if (n > 0 && fd_path[n - 1] != '/')
    {
        fd_path[n] = '/';
        n++;
    }
    uint32_t i = 0;
    while (fd_rows[fd_sel].name[i] && (n + i) < FD_PATH_MAX - 1)
    {
        fd_path[n + i] = fd_rows[fd_sel].name[i];
        i++;
    }
    fd_path[n + i] = 0;
    fd_nav(fd_path);
}

static void fd_build_result(int idx)
{
    uint32_t n = 0;
    while (fd_path[n] && n < 191)
    {
        fd_result[n] = fd_path[n];
        n++;
    }
    if (n > 0 && fd_result[n - 1] != '/' && n < 191)
    {
        fd_result[n] = '/';
        n++;
    }
    if (fd_mode == FD_SAVE)
    {
        uint32_t j = 0;
        while (fd_name.text[j] && (n + j) < 191)
        {
            fd_result[n + j] = fd_name.text[j];
            j++;
        }
        fd_result[n + j] = 0;
    }
    else
    {
        if (idx >= 0 && (uint32_t)idx < fd_nrows && fd_rows[idx].type != VFS_DIR)
        {
            uint32_t j = 0;
            while (fd_rows[idx].name[j] && (n + j) < 191)
            {
                fd_result[n + j] = fd_rows[idx].name[j];
                j++;
            }
            fd_result[n + j] = 0;
        }
        else
        {
            fd_result[0] = 0;
        }
    }
}

static void fd_finish(int result)
{
    struct UiWindow *w = fd_win;
    FileDialogCallback cb = fd_cb;
    void *ctx = fd_ctx;
    char path_copy[192];
    int enc_val = -1;
    uint32_t i = 0;

    if (result == 1)
    {
        while (fd_result[i] && i < 191)
        {
            path_copy[i] = fd_result[i];
            i++;
        }
        if (fd_has_enc)
            enc_val = fd_enc.selected;
    }
    path_copy[i] = 0;

    fd_win = 0;
    fd_active = 0;

    if (w)
        wm_destroy_window(w);

    if (cb)
        cb(result, result ? path_copy : 0, enc_val, ctx);
}

static void fd_ok_click(struct UiWidget *w, void *ctx)
{
    (void)w;
    (void)ctx;
    if (fd_mode == FD_SAVE)
    {
        if (!fd_name.text[0])
            return;
        fd_build_result(-1);
        fd_finish(1);
    }
    else
    {
        if (fd_sel < 0 || (uint32_t)fd_sel >= fd_nrows)
            return;
        if (fd_rows[fd_sel].type == VFS_DIR)
        {
            fd_enter();
            return;
        }
        fd_build_result(fd_sel);
        fd_finish(1);
    }
}

static void fd_cancel_click(struct UiWidget *w, void *ctx)
{
    (void)w;
    (void)ctx;
    fd_finish(0);
}

static int fd_on_close(struct UiWindow *win)
{
    (void)win;
    fd_finish(0);
    return 1;
}

static void fd_paint(struct UiWindow *win, struct gfx_surface *s)
{
    (void)win;
    int32_t cw = (int32_t)s->w;
    int32_t ch = (int32_t)s->h;

    gfx_fill(s, 0, 0, cw, ch, FD_COL_BG);

    gfx_text(s, 16, 12, fd_mode == FD_SAVE ? "Save File" : "Open File", FD_COL_TEXT);
    gfx_text(s, 16, 32, fd_path, FD_COL_MUTED);
    gfx_fill(s, 16, 50, cw - 32, 1, FD_COL_DIV);

    int32_t bottom_h = fd_has_enc ? 120 : (fd_mode == FD_SAVE ? 90 : 50);
    int32_t y = 58;
    int32_t bottom = ch - bottom_h;

    int sel_up = (fd_sel == -2);
    if (sel_up)
        gfx_fill(s, 12, y, cw - 24, 18, FD_COL_SEL);
    gfx_fill(s, 16, y + 3, 12, 10, sel_up ? 0x00FFD27F : 0x00555555);
    gfx_text(s, 36, y, "..", sel_up ? 0x00FFFFFF : FD_COL_TEXT);
    y += 20;

    uint32_t i;
    for (i = 0; i < fd_nrows && y < bottom; i++)
    {
        int selected = (fd_sel >= 0 && (uint32_t)fd_sel == i);
        if (selected)
            gfx_fill(s, 12, y, cw - 24, 18, FD_COL_SEL);
        if (fd_rows[i].type == VFS_DIR)
        {
            gfx_fill(s, 16, y + 3, 12, 10, selected ? 0x00FFD27F : FD_COL_ACC);
            gfx_text(s, 36, y, fd_rows[i].name, selected ? 0x00FFFFFF : FD_COL_TEXT);
        }
        else
        {
            gfx_rect(s, 17, y + 2, 10, 12, FD_COL_MUTED);
            gfx_text(s, 36, y, fd_rows[i].name, selected ? 0x00FFFFFF : FD_COL_TEXT);
        }
        y += 20;
    }

    if (fd_has_enc)
    {
        int32_t ly = ch - 90;
        gfx_text(s, 16, ly + 6, "Encoding:", FD_COL_MUTED);
    }
}

static int fd_on_click(struct UiWindow *win, int32_t cx, int32_t cy)
{
    (void)cx;
    int32_t ch = (int32_t)win->content.h;
    int32_t bottom_h = fd_has_enc ? 120 : (fd_mode == FD_SAVE ? 90 : 50);
    int32_t bottom = ch - bottom_h;

    int row = (cy - 58) / 20;
    if (row < 0 || cy < 58)
        return 0;

    if (cy >= bottom)
        return 0;

    uint64_t now = sched_ticks();

    if (row == 0)
    {
        if (fd_last_row == 0 && (now - fd_last_tick) < 30)
        {
            fd_go_up();
            fd_last_row = -1;
            fd_sel = -1;
        }
        else
        {
            fd_sel = -2;
            fd_last_row = 0;
            fd_last_tick = now;
        }
        return 1;
    }

    int idx = row - 1;
    if ((uint32_t)idx >= fd_nrows)
    {
        fd_last_row = -1;
        return 0;
    }

    if (fd_last_row == row && (now - fd_last_tick) < 30)
    {
        fd_sel = idx;
        if (fd_rows[idx].type == VFS_DIR)
        {
            fd_enter();
        }
        else
        {
            fd_build_result(idx);
            fd_finish(1);
        }
        fd_last_row = -1;
        return 1;
    }

    fd_sel = idx;

    if (fd_mode == FD_SAVE && fd_rows[idx].type != VFS_DIR)
    {
        uint32_t i = 0;
        while (fd_rows[idx].name[i] && i < UI_TEXT_MAX - 1)
        {
            fd_name.text[i] = fd_rows[idx].name[i];
            i++;
        }
        fd_name.text[i] = 0;
        fd_name.len = i;
        fd_name.cursor_pos = i;
    }

    fd_last_row = row;
    fd_last_tick = now;
    return 1;
}

void file_dialog_show(int mode, const char *title, const char *initial_path,
                      const char *ok_text, const char *cancel_text,
                      FileDialogCallback cb, void *ctx)
{
    if (fd_active)
        return;

    fd_mode = mode;
    fd_cb = cb;
    fd_ctx = ctx;
    fd_active = 1;
    fd_sel = -1;
    fd_last_row = -1;
    fd_nrows = 0;
    fd_path[0] = '/';
    fd_path[1] = 0;
    fd_has_enc = (mode == FD_SAVE) ? 1 : 0;

    char init_name[64];
    init_name[0] = 0;

    if (initial_path && initial_path[0])
    {
        uint32_t last_slash = 0;
        uint32_t j = 0;
        while (initial_path[j] && j < FD_PATH_MAX - 1)
        {
            if (initial_path[j] == '/')
                last_slash = j;
            j++;
        }

        if (last_slash == 0)
        {
            fd_path[0] = '/';
            fd_path[1] = 0;
        }
        else
        {
            uint32_t k = 0;
            while (k < last_slash && k < FD_PATH_MAX - 1)
            {
                fd_path[k] = initial_path[k];
                k++;
            }
            fd_path[k] = 0;
        }

        if (mode == FD_SAVE)
        {
            uint32_t start = last_slash + 1;
            uint32_t k = 0;
            while (initial_path[start + k] && k < 63)
            {
                init_name[k] = initial_path[start + k];
                k++;
            }
            init_name[k] = 0;
        }
    }

    fd_nav(fd_path);

    int32_t dw = 520;
    int32_t dh = fd_has_enc ? 420 : (mode == FD_SAVE ? 400 : 360);
    int32_t dx = 380;
    int32_t dy = 120;
    int32_t cw = dw - 2;
    int32_t ch = dh - WM_TITLE_H - 1;

    fd_win = wm_create_dialog(title, dx, dy, dw, dh, fd_paint);
    if (!fd_win)
    {
        fd_active = 0;
        if (cb)
            cb(0, 0, -1, ctx);
        return;
    }

    wm_window_set_close(fd_win, fd_on_close);
    wm_window_set_click(fd_win, fd_on_click);

    if (fd_has_enc)
    {
        ui_textinput_init(&fd_name, 16, ch - 108, cw - 180, "text.txt");
        if (init_name[0])
        {
            uint32_t j = 0;
            while (init_name[j] && j < UI_TEXT_MAX - 1)
            {
                fd_name.text[j] = init_name[j];
                j++;
            }
            fd_name.text[j] = 0;
            fd_name.len = j;
            fd_name.cursor_pos = j;
        }
        wm_window_add_widget(fd_win, &fd_name.base);
        wm_window_anchor(fd_win, &fd_name.base, UI_ANCHOR_BOTTOM_FILL);

        ui_dropdown_init(&fd_enc, cw - 156, ch - 108, 140);
        ui_dropdown_add(&fd_enc, "UTF-8");
        ui_dropdown_add(&fd_enc, "ASCII");
        ui_dropdown_add(&fd_enc, "UTF-16");
        fd_enc.selected = 0;
        wm_window_add_widget(fd_win, &fd_enc.base);
        wm_window_anchor(fd_win, &fd_enc.base, UI_ANCHOR_BOTTOM_RIGHT);
    }

    const char *ok_lbl = ok_text;
    if (!ok_lbl)
        ok_lbl = mode == FD_SAVE ? "Save" : "Open";
    const char *cancel_lbl = cancel_text;
    if (!cancel_lbl)
        cancel_lbl = "Cancel";

    ui_button_init(&fd_ok, cw - 144, ch - 42, 60, 28, ok_lbl, FD_COL_ACC);
    fd_ok.base.action = fd_ok_click;
    wm_window_add_widget(fd_win, &fd_ok.base);
    wm_window_anchor(fd_win, &fd_ok.base, UI_ANCHOR_BOTTOM_RIGHT);

    ui_button_init(&fd_cancel, cw - 76, ch - 42, 60, 28, cancel_lbl, FD_COL_DIV);
    fd_cancel.base.action = fd_cancel_click;
    wm_window_add_widget(fd_win, &fd_cancel.base);
    wm_window_anchor(fd_win, &fd_cancel.base, UI_ANCHOR_BOTTOM_RIGHT);

    wm_show(fd_win, 1);
    wm_focus(fd_win);
    wm_mark_dirty();
}
