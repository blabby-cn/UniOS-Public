#include "guisys.h"
#include "wm.h"
#include "ui.h"
#include "gfx.h"
#include "kheap.h"
#include "kprintf.h"
#include "sched.h"

#define MAX_USER_WINS 8
#define MAX_WIDGETS_PER_WIN 16
#define GUI_Q_CAP 32
#define USER_TEXT_MAX 64
#define USER_TEXT_SLOTS 16

struct user_text
{
    int32_t x;
    int32_t y;
    char str[USER_TEXT_MAX];
    uint8_t used;
};

struct wcbctx
{
    struct user_win *uw;
    uint32_t idx;
};

struct user_win
{
    uint32_t id;
    uint32_t owner_pid;
    struct UiWindow *win;
    struct UiWidget *widgets[MAX_WIDGETS_PER_WIN];
    struct wcbctx cbctx[MAX_WIDGETS_PER_WIN];
    struct user_text texts[USER_TEXT_SLOTS];
    uint32_t n_texts;
    uint32_t n_widgets;
    struct gui_event q[GUI_Q_CAP];
    uint32_t q_head;
    uint32_t q_tail;
    int used;
};

static struct user_win g_uwins[MAX_USER_WINS];
static uint32_t g_next_id = 1;

static struct user_win *find(uint32_t id)
{
    for (int i = 0; i < MAX_USER_WINS; i++)
        if (g_uwins[i].used && g_uwins[i].id == id)
            return &g_uwins[i];
    return 0;
}

static void queue_push(struct user_win *uw, uint32_t widget_idx, uint8_t type)
{
    uint32_t next = (uw->q_head + 1) % GUI_Q_CAP;
    if (next == uw->q_tail)
        return;
    uw->q[uw->q_head].win_id = uw->id;
    uw->q[uw->q_head].widget_idx = widget_idx;
    uw->q[uw->q_head].type = type;
    uw->q_head = next;
}

static void widget_action(struct UiWidget *w, void *ctx)
{
    struct wcbctx *c = (struct wcbctx *)ctx;
    if (!c || !c->uw)
        return;
    uint8_t type = (w->type == UI_TYPE_TEXTINPUT) ? GUI_EV_SUBMIT : GUI_EV_CLICK;
    queue_push(c->uw, c->idx, type);
}

static void paint_user(struct UiWindow *win, struct gfx_surface *s)
{
    gfx_fill(s, 0, 0, (int32_t)s->w, (int32_t)s->h, 0x00222222);
    for (int i = 0; i < MAX_USER_WINS; i++)
    {
        if (g_uwins[i].used && g_uwins[i].win == win)
        {
            struct user_win *uw = &g_uwins[i];
            for (uint32_t j = 0; j < USER_TEXT_SLOTS; j++)
                if (uw->texts[j].used)
                    gfx_text(s, uw->texts[j].x, uw->texts[j].y, uw->texts[j].str, 0x00E6E6E6);
            break;
        }
    }
}

int guisys_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h)
{
    for (int i = 0; i < MAX_USER_WINS; i++)
    {
        if (!g_uwins[i].used)
        {
            struct UiWindow *win = wm_create_window(title, x, y, w, h, 0x00417AC0, paint_user);
            if (!win) return -1;
            g_uwins[i].used = 1;
            g_uwins[i].id = g_next_id++;
            g_uwins[i].owner_pid = (uint32_t)sched_current_id();
            g_uwins[i].win = win;
            g_uwins[i].n_widgets = 0;
            g_uwins[i].n_texts = 0;
            g_uwins[i].q_head = 0;
            g_uwins[i].q_tail = 0;
            for (uint32_t j = 0; j < USER_TEXT_SLOTS; j++)
                g_uwins[i].texts[j].used = 0;
            wm_window_set_click(win, 0);
            kprintf("guisys: win %u '%s' pid=%u\n", g_uwins[i].id, title, g_uwins[i].owner_pid);
            return (int)g_uwins[i].id;
        }
    }
    return -1;
}

int guisys_destroy(uint32_t win_id)
{
    struct user_win *uw = find(win_id);
    if (!uw) return -1;
    wm_destroy_window(uw->win);
    uw->used = 0;
    return 0;
}

int guisys_widget(uint32_t win_id, const struct gui_widget_req *req)
{
    struct user_win *uw = find(win_id);
    if (!uw || uw->n_widgets >= MAX_WIDGETS_PER_WIN) return -1;

    uint32_t idx = uw->n_widgets;
    struct UiWidget *w = 0;

    if (req->type == GUI_WIDGET_BUTTON)
    {
        struct UiButton *b = kmalloc(sizeof(struct UiButton));
        if (!b) return -1;
        ui_button_init(b, req->x, req->y, req->w, req->h, req->label, 0x00417AC0);
        w = &b->base;
    }
    else if (req->type == GUI_WIDGET_TEXTINPUT)
    {
        struct UiTextInput *t = kmalloc(sizeof(struct UiTextInput));
        if (!t) return -1;
        ui_textinput_init(t, req->x, req->y, req->w, req->label);
        w = &t->base;
    }
    else if (req->type == GUI_WIDGET_TEXT)
    {
        struct UiWidget *tw = kmalloc(sizeof(struct UiWidget));
        if (!tw) return -1;
        tw->x = req->x; tw->y = req->y; tw->w = req->w; tw->h = 16;
        tw->visible = 1;
        tw->type = UI_TYPE_WIDGET;
        tw->tip = 0;
        tw->draw = 0;
        tw->on_event = 0;
        tw->action = 0;
        tw->action_ctx = 0;
        tw->next = 0;
        w = tw;
        wm_window_add_widget(uw->win, w);
        uw->widgets[idx] = w;
        uw->n_widgets++;
        return (int)idx;
    }
    else
    {
        return -1;
    }

    if (!w) return -1;
    uw->cbctx[idx].uw = uw;
    uw->cbctx[idx].idx = idx;
    w->action = widget_action;
    w->action_ctx = &uw->cbctx[idx];
    wm_window_add_widget(uw->win, w);
    uw->widgets[idx] = w;
    uw->n_widgets++;
    return (int)idx;
}

int guisys_set_visible(uint32_t win_id, int visible)
{
    struct user_win *uw = find(win_id);
    if (!uw) return -1;
    wm_show(uw->win, visible);
    return 0;
}

int guisys_text(uint32_t win_id, int32_t x, int32_t y, const char *text)
{
    struct user_win *uw = find(win_id);
    if (!uw) return -1;
    uint32_t slot = USER_TEXT_SLOTS;
    for (uint32_t i = 0; i < USER_TEXT_SLOTS; i++)
        if (!uw->texts[i].used) { slot = i; break; }
    if (slot == USER_TEXT_SLOTS) return -1;
    uint32_t n = 0;
    while (text[n] && n < USER_TEXT_MAX - 1) { uw->texts[slot].str[n] = text[n]; n++; }
    uw->texts[slot].str[n] = 0;
    uw->texts[slot].x = x;
    uw->texts[slot].y = y;
    uw->texts[slot].used = 1;
    if (slot + 1 > uw->n_texts) uw->n_texts = slot + 1;
    return (int)slot;
}

int guisys_label(uint32_t win_id, uint32_t text_idx, const char *str)
{
    struct user_win *uw = find(win_id);
    if (!uw || text_idx >= USER_TEXT_SLOTS || !uw->texts[text_idx].used) return -1;
    uint32_t n = 0;
    while (str[n] && n < USER_TEXT_MAX - 1) { uw->texts[text_idx].str[n] = str[n]; n++; }
    uw->texts[text_idx].str[n] = 0;
    wm_mark_dirty();
    return 0;
}

int guisys_set_text(uint32_t win_id, uint32_t widget_idx, const char *str)
{
    struct user_win *uw = find(win_id);
    if (!uw || widget_idx >= uw->n_widgets || !str) return -1;
    struct UiWidget *w = uw->widgets[widget_idx];
    if (!w || w->type != UI_TYPE_TEXTINPUT) return -1;
    struct UiTextInput *t = (struct UiTextInput *)w;
    uint32_t n = 0;
    while (str[n] && n < UI_TEXT_MAX - 1) { t->text[n] = str[n]; n++; }
    t->text[n] = 0;
    t->len = n;
    t->cursor_pos = n;
    wm_mark_dirty();
    return 0;
}

int guisys_get_text(uint32_t win_id, uint32_t widget_idx, char *buf, uint32_t cap)
{
    struct user_win *uw = find(win_id);
    if (!uw || widget_idx >= uw->n_widgets || !buf || cap == 0) return -1;
    struct UiWidget *w = uw->widgets[widget_idx];
    if (!w || w->type != UI_TYPE_TEXTINPUT) return -1;
    struct UiTextInput *t = (struct UiTextInput *)w;
    uint32_t n = 0;
    while (n < t->len && n < cap - 1) { buf[n] = t->text[n]; n++; }
    buf[n] = 0;
    return (int)n;
}

int guisys_poll(uint32_t win_id, struct gui_event *ev)
{
    struct user_win *uw = find(win_id);
    if (!uw || !ev) return 0;
    if (uw->q_tail == uw->q_head) return 0;
    *ev = uw->q[uw->q_tail];
    uw->q_tail = (uw->q_tail + 1) % GUI_Q_CAP;
    return 1;
}

int guisys_focus_by_title(const char *title)
{
    for (int i = 0; i < MAX_USER_WINS; i++)
    {
        if (!g_uwins[i].used) continue;
        uint32_t j = 0;
        int match = 1;
        while (title[j] && j < WM_TITLE_MAX - 1)
        {
            if (g_uwins[i].win->title[j] != title[j]) { match = 0; break; }
            j++;
        }
        if (match && !title[j])
        {
            wm_show(g_uwins[i].win, 1);
            wm_focus(g_uwins[i].win);
            return (int)g_uwins[i].id;
        }
    }
    return -1;
}
