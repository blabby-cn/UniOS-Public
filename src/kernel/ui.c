#include "ui.h"
#include "keyboard.h"
#include "locale.h"

void *memset(void *dst, int c, unsigned long n);

static void str_copy(char *dst, const char *src, uint32_t max)
{
    uint32_t i = 0;
    while (src[i] && i < max - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

int ui_widget_hit(const struct UiWidget *w, int32_t x, int32_t y)
{
    return x >= w->x && y >= w->y && x < w->x + w->w && y < w->y + w->h;
}

void ui_widget_set_anchor(struct UiWidget *w, uint8_t preset, int32_t cw, int32_t ch)
{
    w->anchor = preset;
    if (!preset) return;
    w->base_w = w->w;
    w->base_h = w->h;
    w->margin_l = w->x;
    w->margin_t = w->y;
    w->margin_r = cw - (w->x + w->w);
    w->margin_b = ch - (w->y + w->h);
    ui_widget_layout(w, cw, ch);
}

void ui_widget_layout(struct UiWidget *w, int32_t cw, int32_t ch)
{
    uint8_t p = w->anchor;
    if (!p) return;
    int32_t bw = w->base_w;
    int32_t bh = w->base_h;
    switch (p)
    {
        case UI_ANCHOR_TOP:
            w->w = bw; w->h = bh;
            w->x = (cw - bw) / 2;
            w->y = w->margin_t;
            break;
        case UI_ANCHOR_BOTTOM:
            w->w = bw; w->h = bh;
            w->x = (cw - bw) / 2;
            w->y = ch - w->margin_b - bh;
            break;
        case UI_ANCHOR_LEFT:
            w->w = bw; w->h = bh;
            w->x = w->margin_l;
            w->y = (ch - bh) / 2;
            break;
        case UI_ANCHOR_RIGHT:
            w->w = bw; w->h = bh;
            w->x = cw - w->margin_r - bw;
            w->y = (ch - bh) / 2;
            break;
        case UI_ANCHOR_TOP_LEFT:
            w->w = bw; w->h = bh;
            w->x = w->margin_l;
            w->y = w->margin_t;
            break;
        case UI_ANCHOR_TOP_RIGHT:
            w->w = bw; w->h = bh;
            w->x = cw - w->margin_r - bw;
            w->y = w->margin_t;
            break;
        case UI_ANCHOR_BOTTOM_LEFT:
            w->w = bw; w->h = bh;
            w->x = w->margin_l;
            w->y = ch - w->margin_b - bh;
            break;
        case UI_ANCHOR_BOTTOM_RIGHT:
            w->w = bw; w->h = bh;
            w->x = cw - w->margin_r - bw;
            w->y = ch - w->margin_b - bh;
            break;
        case UI_ANCHOR_TOP_FILL:
            w->h = bh;
            w->w = cw - w->margin_l - w->margin_r;
            if (w->w < 1) w->w = 1;
            w->x = w->margin_l;
            w->y = w->margin_t;
            break;
        case UI_ANCHOR_BOTTOM_FILL:
            w->h = bh;
            w->w = cw - w->margin_l - w->margin_r;
            if (w->w < 1) w->w = 1;
            w->x = w->margin_l;
            w->y = ch - w->margin_b - bh;
            break;
        case UI_ANCHOR_LEFT_FILL:
            w->w = bw;
            w->h = ch - w->margin_t - w->margin_b;
            if (w->h < 1) w->h = 1;
            w->x = w->margin_l;
            w->y = w->margin_t;
            break;
        case UI_ANCHOR_RIGHT_FILL:
            w->w = bw;
            w->h = ch - w->margin_t - w->margin_b;
            if (w->h < 1) w->h = 1;
            w->x = cw - w->margin_r - bw;
            w->y = w->margin_t;
            break;
        case UI_ANCHOR_FILL:
            w->w = cw - w->margin_l - w->margin_r;
            if (w->w < 1) w->w = 1;
            w->h = ch - w->margin_t - w->margin_b;
            if (w->h < 1) w->h = 1;
            w->x = w->margin_l;
            w->y = w->margin_t;
            break;
        case UI_ANCHOR_CENTER:
            w->w = bw; w->h = bh;
            w->x = (cw - bw) / 2;
            w->y = (ch - bh) / 2;
            break;
    }
}

static void button_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiButton *b = (struct UiButton *)w;
    uint32_t fill = 0x00333333;
    if (w->pressed)
        fill = 0x00262626;
    else if (w->hover)
        fill = 0x003E3E3E;
    gfx_round_fill(s, w->x, w->y, w->w, w->h, 4, fill);
    int32_t tw = gfx_text_width(b->label);
    gfx_text(s, w->x + (w->w - tw) / 2, w->y + (w->h - 16) / 2, b->label, 0x00E6E6E6);
}

static int button_event(struct UiWidget *w, const struct UiEvent *ev)
{
    if (ev->type == UI_EVENT_MOUSE_MOVE)
    {
        w->hover = ui_widget_hit(w, ev->x, ev->y) ? 1 : 0;
        if (!w->hover)
            w->pressed = 0;
        return 0;
    }
    if (!ui_widget_hit(w, ev->x, ev->y))
        return 0;
    if (ev->type == UI_EVENT_MOUSE_DOWN)
    {
        w->pressed = 1;
        return 1;
    }
    if (ev->type == UI_EVENT_MOUSE_UP && w->pressed)
    {
        w->pressed = 0;
        if (w->action)
            w->action(w, w->action_ctx);
        return 1;
    }
    return 0;
}

void ui_button_init(struct UiButton *b, int32_t x, int32_t y, int32_t w, int32_t h, const char *label, uint32_t accent)
{
    memset(b, 0, sizeof(*b));
    b->base.x = x;
    b->base.y = y;
    b->base.w = w;
    b->base.h = h;
    b->base.visible = 1;
    b->base.type = UI_TYPE_BUTTON;
    b->base.draw = button_draw;
    b->base.on_event = button_event;
    b->accent = accent;
    str_copy(b->label, label, UI_TEXT_MAX);
}

void ui_button_set_label(struct UiButton *b, const char *label)
{
    str_copy(b->label, label, UI_TEXT_MAX);
}

static void toggle_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiToggle *t = (struct UiToggle *)w;
    uint32_t track = t->on ? 0x00417AC0 : 0x00444444;
    if (w->hover) track = t->on ? 0x005A90D4 : 0x00555555;
    gfx_round_fill(s, w->x, w->y + 2, 40, 20, 10, track);
    int32_t kx = t->on ? w->x + 40 - 18 : w->x + 2;
    gfx_disc(s, kx + 8, w->y + 12, 8, 0x00E6E6E6);
    gfx_text(s, w->x + 48, w->y + 4, t->label, w->hover ? 0x00FFFFFF : 0x00E6E6E6);
}

static int toggle_event(struct UiWidget *w, const struct UiEvent *ev)
{
    struct UiToggle *t = (struct UiToggle *)w;
    if (ev->type == UI_EVENT_MOUSE_MOVE)
    {
        w->hover = ui_widget_hit(w, ev->x, ev->y) ? 1 : 0;
        return 0;
    }
    if (!ui_widget_hit(w, ev->x, ev->y))
        return 0;
    if (ev->type == UI_EVENT_MOUSE_DOWN)
    {
        t->on = t->on ? 0 : 1;
        if (w->action)
            w->action(w, w->action_ctx);
        return 1;
    }
    return 0;
}

void ui_toggle_init(struct UiToggle *t, int32_t x, int32_t y, const char *label, uint8_t on)
{
    memset(t, 0, sizeof(*t));
    t->base.x = x;
    t->base.y = y;
    t->base.w = 48 + gfx_text_width(label);
    t->base.h = 24;
    t->base.visible = 1;
    t->base.type = UI_TYPE_TOGGLE;
    t->base.draw = toggle_draw;
    t->base.on_event = toggle_event;
    t->on = on;
    str_copy(t->label, label, UI_TEXT_MAX);
}

void ui_toggle_set_label(struct UiToggle *t, const char *label)
{
    str_copy(t->label, label, UI_TEXT_MAX);
    t->base.w = 48 + gfx_text_width(label);
}

static void textinput_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiTextInput *t = (struct UiTextInput *)w;
    gfx_round_fill(s, w->x, w->y, w->w, w->h, 4, 0x001A1A1A);
    gfx_rect(s, w->x, w->y, w->w, w->h, w->focused ? 0x00417AC0 : 0x00404040);
    if (t->len)
    {
        gfx_text(s, w->x + 8, w->y + (w->h - 16) / 2, t->text, 0x00E6E6E6);
        if (w->focused)
        {
            int32_t cx = w->x + 8 + (int32_t)t->cursor_pos * 8;
            gfx_fill(s, cx, w->y + 4, 2, w->h - 8, 0x00FFD27F);
        }
    }
    else
    {
        gfx_text(s, w->x + 8, w->y + (w->h - 16) / 2, t->placeholder, 0x00707070);
        if (w->focused)
            gfx_fill(s, w->x + 8, w->y + 4, 2, w->h - 8, 0x00FFD27F);
    }
}

static int textinput_event(struct UiWidget *w, const struct UiEvent *ev)
{
    struct UiTextInput *t = (struct UiTextInput *)w;
    if (ev->type == UI_EVENT_MOUSE_DOWN)
    {
        w->focused = ui_widget_hit(w, ev->x, ev->y) ? 1 : 0;
        if (w->focused)
        {
            int32_t rel = ev->x - w->x - 8;
            int32_t idx = rel / 8;
            if (idx < 0) idx = 0;
            if ((uint32_t)idx > t->len) idx = (int32_t)t->len;
            t->cursor_pos = (uint32_t)idx;
        }
        return w->focused;
    }
    if (ev->type == UI_EVENT_KEY && w->focused)
    {
        unsigned char k = (unsigned char)ev->key;
        if (ev->key == '\b' && t->cursor_pos > 0 && t->len > 0)
        {
            for (uint32_t i = t->cursor_pos - 1; i < t->len; i++)
                t->text[i] = t->text[i + 1];
            t->len--;
            t->cursor_pos--;
            return 1;
        }
        if (k == KEY_LEFT  && t->cursor_pos > 0)      { t->cursor_pos--; return 1; }
        if (k == KEY_RIGHT && t->cursor_pos < t->len) { t->cursor_pos++; return 1; }
        if (k == KEY_HOME) { t->cursor_pos = 0; return 1; }
        if (k == KEY_END)  { t->cursor_pos = t->len; return 1; }
        if (k == KEY_UP || k == KEY_DOWN) return 1;
        if (ev->key >= 32 && ev->key < 127 && t->len < UI_TEXT_MAX - 1)
        {
            for (uint32_t i = t->len; i > t->cursor_pos; i--)
                t->text[i] = t->text[i - 1];
            t->text[t->cursor_pos] = ev->key;
            t->len++;
            t->cursor_pos++;
            t->text[t->len] = 0;
            return 1;
        }
        if (ev->key == '\r' || ev->key == '\n')
        {
            if (w->action) w->action(w, w->action_ctx);
            return 1;
        }
    }
    return 0;
}

void ui_textinput_init(struct UiTextInput *t, int32_t x, int32_t y, int32_t w, const char *placeholder)
{
    memset(t, 0, sizeof(*t));
    t->base.x = x;
    t->base.y = y;
    t->base.w = w;
    t->base.h = 28;
    t->base.visible = 1;
    t->base.type = UI_TYPE_TEXTINPUT;
    t->base.draw = textinput_draw;
    t->base.on_event = textinput_event;
    str_copy(t->placeholder, placeholder, UI_TEXT_MAX);
}

void ui_textinput_set_placeholder(struct UiTextInput *t, const char *placeholder)
{
    str_copy(t->placeholder, placeholder, UI_TEXT_MAX);
}

static void checkbox_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiCheckbox *c = (struct UiCheckbox *)w;
    uint32_t bc = w->hover ? 0x00777777 : 0x00555555;
    gfx_rect(s, w->x, w->y, 16, 16, bc);
    gfx_fill(s, w->x + 2, w->y + 2, 12, 12, w->hover ? 0x002A2A2A : 0x001A1A1A);
    if (c->checked)
    {
        gfx_fill(s, w->x + 3, w->y + 3, 10, 10, 0x00417AC0);
        gfx_text(s, w->x + 3, w->y, "x", 0x00FFFFFF);
    }
    gfx_text(s, w->x + 22, w->y, c->label, w->hover ? 0x00FFFFFF : 0x00E6E6E6);
}

static int checkbox_event(struct UiWidget *w, const struct UiEvent *ev)
{
    struct UiCheckbox *c = (struct UiCheckbox *)w;
    if (ev->type == UI_EVENT_MOUSE_DOWN && ui_widget_hit(w, ev->x, ev->y))
    {
        c->checked = c->checked ? 0 : 1;
        if (w->action) w->action(w, w->action_ctx);
        return 1;
    }
    return 0;
}

void ui_checkbox_init(struct UiCheckbox *c, int32_t x, int32_t y, const char *label, uint8_t checked)
{
    memset(c, 0, sizeof(*c));
    c->base.x = x; c->base.y = y;
    c->base.w = 22 + gfx_text_width(label); c->base.h = 18;
    c->base.visible = 1;
    c->base.type = UI_TYPE_CHECKBOX;
    c->base.draw = checkbox_draw;
    c->base.on_event = checkbox_event;
    c->checked = checked;
    str_copy(c->label, label, UI_TEXT_MAX);
}

static void radio_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiRadio *r = (struct UiRadio *)w;
    gfx_disc(s, w->x + 7, w->y + 8, 7, 0x00555555);
    gfx_disc(s, w->x + 7, w->y + 8, 5, 0x001A1A1A);
    if (r->selected)
        gfx_disc(s, w->x + 7, w->y + 8, 3, 0x00417AC0);
    gfx_text(s, w->x + 18, w->y + 1, r->label, 0x00E6E6E6);
}

static int radio_event(struct UiWidget *w, const struct UiEvent *ev)
{
    if (ev->type == UI_EVENT_MOUSE_DOWN && ui_widget_hit(w, ev->x, ev->y))
    {
        struct UiRadio *r = (struct UiRadio *)w;
        r->selected = 1;
        if (w->action) w->action(w, w->action_ctx);
        return 1;
    }
    return 0;
}

void ui_radio_init(struct UiRadio *r, int32_t x, int32_t y, const char *label, int group, uint8_t selected)
{
    memset(r, 0, sizeof(*r));
    r->base.x = x; r->base.y = y;
    r->base.w = 18 + gfx_text_width(label); r->base.h = 18;
    r->base.visible = 1;
    r->base.type = UI_TYPE_RADIO;
    r->base.draw = radio_draw;
    r->base.on_event = radio_event;
    r->group = group;
    r->selected = selected;
    str_copy(r->label, label, UI_TEXT_MAX);
}

void ui_radio_set_group(struct UiWidget *list, int group, struct UiRadio *sel)
{
    for (struct UiWidget *w = list; w; w = w->next)
    {
        struct UiRadio *r = (struct UiRadio *)w;
        if ((void *)w->draw == (void *)radio_draw && r->group == group)
            r->selected = (r == sel) ? 1 : 0;
    }
}

static void dropdown_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiDropdown *d = (struct UiDropdown *)w;
    uint32_t bg = w->hover || d->expanded ? 0x003E3E3E : 0x00333333;
    gfx_fill(s, w->x, w->y, w->w, 24, bg);
    gfx_rect(s, w->x, w->y, w->w, 24, w->hover ? 0x00777777 : 0x00555555);
    if (d->selected >= 0 && d->selected < d->n_options)
        gfx_text(s, w->x + 8, w->y + 4, d->options[d->selected], 0x00E6E6E6);
    gfx_text(s, w->x + w->w - 18, w->y + 4, "v", 0x00888888);
    if (d->expanded)
    {
        for (int i = 0; i < d->n_options; i++)
        {
            int32_t oy = w->y + 24 + i * 22;
            gfx_fill(s, w->x, oy, w->w, 22, i == d->selected ? 0x00224466 : 0x002A2A2A);
            gfx_text(s, w->x + 8, oy + 3, d->options[i], i == d->selected ? 0x00FFFFFF : 0x00CCCCCC);
        }
        d->base.h = 24 + d->n_options * 22;
    }
    else
    {
        d->base.h = 24;
    }
}

static int dropdown_event(struct UiWidget *w, const struct UiEvent *ev)
{
    struct UiDropdown *d = (struct UiDropdown *)w;
    if (ev->type == UI_EVENT_MOUSE_MOVE)
    {
        w->hover = ui_widget_hit(w, ev->x, ev->y) ? 1 : 0;
        if (d->expanded)
        {
            int old_h = d->base.h;
            d->base.h = 24 + d->n_options * 22;
            return (old_h != d->base.h);
        }
        return 0;
    }
    if (ev->type == UI_EVENT_MOUSE_DOWN)
    {
        if (d->expanded)
        {
            for (int i = 0; i < d->n_options; i++)
            {
                int32_t oy = w->y + 24 + i * 22;
                if (ev->x >= w->x && ev->x < w->x + w->w && ev->y >= oy && ev->y < oy + 22)
                {
                    d->selected = i;
                    d->expanded = 0;
                    d->base.h = 24;
                    if (w->action) w->action(w, w->action_ctx);
                    return 1;
                }
            }
        }
        d->expanded = ui_widget_hit(w, ev->x, ev->y) ? (d->expanded ? 0 : 1) : 0;
        if (!d->expanded) d->base.h = 24;
        return ui_widget_hit(w, ev->x, ev->y);
    }
    return 0;
}

void ui_dropdown_init(struct UiDropdown *d, int32_t x, int32_t y, int32_t w)
{
    memset(d, 0, sizeof(*d));
    d->base.x = x; d->base.y = y;
    d->base.w = w; d->base.h = 24;
    d->base.visible = 1;
    d->base.type = UI_TYPE_DROPDOWN;
    d->base.draw = dropdown_draw;
    d->base.on_event = dropdown_event;
    d->selected = -1;
}

void ui_dropdown_add(struct UiDropdown *d, const char *option)
{
    if (d->n_options < 8)
    {
        str_copy(d->options[d->n_options], option, UI_TEXT_MAX);
        if (d->selected < 0) d->selected = 0;
        d->n_options++;
    }
}

static void image_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiImage *img = (struct UiImage *)w;
    if (!img->pixels) return;
    for (int32_t py = 0; py < w->h && py < img->img_h; py++)
    {
        for (int32_t px = 0; px < w->w && px < img->img_w; px++)
        {
            uint32_t off = (uint32_t)(py * img->img_w + px) * 4;
            uint8_t r = img->pixels[off];
            uint8_t g = img->pixels[off + 1];
            uint8_t b = img->pixels[off + 2];
            uint8_t a = img->pixels[off + 3];
            if (a > 0)
            {
                uint32_t color = ((uint32_t)b) | ((uint32_t)g << 8) | ((uint32_t)r << 16) | ((uint32_t)a << 24);
                gfx_fill_blend(s, w->x + px, w->y + py, 1, 1, color, a);
            }
        }
    }
}

static int image_event(struct UiWidget *w, const struct UiEvent *ev)
{
    (void)w; (void)ev;
    return 0;
}

void ui_image_init(struct UiImage *img, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *pixels, int32_t iw, int32_t ih)
{
    memset(img, 0, sizeof(*img));
    img->base.x = x; img->base.y = y;
    img->base.w = w; img->base.h = h;
    img->base.visible = 1;
    img->base.type = UI_TYPE_IMAGE;
    img->base.draw = image_draw;
    img->base.on_event = image_event;
    img->pixels = pixels;
    img->img_w = iw;
    img->img_h = ih;
}

void ui_startmenu_init(struct UiStartMenu *m, int32_t x, int32_t y, int32_t w)
{
    memset(m, 0, sizeof(*m));
    m->x = x;
    m->y = y;
    m->w = w;
    m->hover_idx = -1;
}

void ui_startmenu_add(struct UiStartMenu *m, const char *label, ui_action_fn action)
{
    if (m->n_items >= 12) return;
    uint32_t i = 0;
    while (label[i] && i < UI_TEXT_MAX - 1) { m->items[m->n_items].label[i] = label[i]; i++; }
    m->items[m->n_items].label[i] = 0;
    m->items[m->n_items].action = action;
    m->n_items++;
}

void ui_startmenu_set_label(struct UiStartMenu *m, int idx, const char *label)
{
    if (idx < 0 || idx >= m->n_items) return;
    uint32_t i = 0;
    while (label[i] && i < UI_TEXT_MAX - 1) { m->items[idx].label[i] = label[i]; i++; }
    m->items[idx].label[i] = 0;
}

void ui_startmenu_draw(struct UiStartMenu *m, struct gfx_surface *s)
{
    if (!m->visible) return;
    int32_t h = 8 + m->n_items * 34 + 8;
    gfx_fill(s, m->x, m->y, m->w, h, 0x00222222);
    gfx_rect(s, m->x, m->y, m->w, h, 0x00555555);
    for (int i = 0; i < m->n_items; i++)
    {
        int32_t iy = m->y + 8 + i * 34;
        if (i == m->hover_idx)
            gfx_fill(s, m->x + 4, iy, m->w - 8, 28, 0x003E3E3E);
        gfx_text(s, m->x + 12, iy + 6, m->items[i].label,
                 i == m->hover_idx ? 0x00FFFFFF : 0x00E6E6E6);
    }
}

int ui_startmenu_click(struct UiStartMenu *m, int32_t mx, int32_t my)
{
    if (!m->visible) return 0;
    int32_t h = 8 + m->n_items * 34 + 8;
    if (mx < m->x || my < m->y || mx >= m->x + m->w || my >= m->y + h)
    {
        m->visible = 0;
        m->hover_idx = -1;
        return 1;
    }
    for (int i = 0; i < m->n_items; i++)
    {
        int32_t iy = m->y + 8 + i * 34;
        if (my >= iy && my < iy + 28 && mx >= m->x + 4 && mx < m->x + m->w - 4)
        {
            if (m->items[i].action)
                m->items[i].action(0, 0);
            m->visible = 0;
            m->hover_idx = -1;
            return 1;
        }
    }
    return 0;
}

void ui_startmenu_move(struct UiStartMenu *m, int32_t mx, int32_t my)
{
    if (!m->visible) return;
    m->hover_idx = -1;
    for (int i = 0; i < m->n_items; i++)
    {
        int32_t iy = m->y + 8 + i * 34;
        if (mx >= m->x + 4 && mx < m->x + m->w - 4 && my >= iy && my < iy + 28)
        {
            m->hover_idx = i;
            return;
        }
    }
}

void ui_widget_set_tip(struct UiWidget *w, const char *key)
{
    w->tip = key;
}

const char *ui_widget_default_tip(const struct UiWidget *w)
{
    switch (w->type)
    {
    case UI_TYPE_BUTTON:
        return ((const struct UiButton *)w)->label;
    case UI_TYPE_TOGGLE:
        return ((const struct UiToggle *)w)->label;
    case UI_TYPE_CHECKBOX:
        return ((const struct UiCheckbox *)w)->label;
    case UI_TYPE_RADIO:
        return ((const struct UiRadio *)w)->label;
    case UI_TYPE_DROPDOWN:
        return locale_get("tip.dropdown");
    case UI_TYPE_TEXTINPUT:
        return locale_get("tip.input");
    case UI_TYPE_TEXTAREA:
        return locale_get("tip.textarea");
    default:
        return 0;
    }
}

int ui_startmenu_item_at(const struct UiStartMenu *m, int32_t mx, int32_t my)
{
    if (!m->visible) return -1;
    int32_t h = 8 + m->n_items * 34 + 8;
    if (mx < m->x || my < m->y || mx >= m->x + m->w || my >= m->y + h) return -1;
    for (int i = 0; i < m->n_items; i++)
    {
        int32_t iy = m->y + 8 + i * 34;
        if (my >= iy && my < iy + 28 && mx >= m->x + 4 && mx < m->x + m->w - 4)
            return i;
    }
    return -1;
}

static int32_t menubar_panel_w(struct UiMenuBar *mb, int m)
{
    int32_t maxw = 120;
    for (int i = 0; i < mb->n_items[m]; i++)
    {
        int32_t tw = gfx_text_width(mb->items[m][i].label) + 28;
        if (tw > maxw) maxw = tw;
    }
    return maxw;
}

static void menubar_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiMenuBar *mb = (struct UiMenuBar *)w;
    gfx_fill(s, w->x, w->y, w->w, 24, 0x00181818);
    gfx_fill(s, w->x, w->y + 24, w->w, 1, 0x00333333);
    for (int i = 0; i < mb->n_menus; i++)
    {
        int32_t tx = w->x + mb->title_x[i];
        int32_t tw = mb->title_w[i];
        int active = (mb->open == i);
        int hov = (mb->hover_menu == i);
        if (active) gfx_fill(s, tx, w->y, tw, 24, 0x003E3E3E);
        else if (hov) gfx_fill(s, tx, w->y, tw, 24, 0x002A2A2A);
        gfx_text(s, tx + 12, w->y + 4, mb->titles[i], (active || hov) ? 0x00FFFFFF : 0x00E6E6E6);
    }
    if (mb->open >= 0 && mb->open < mb->n_menus)
    {
        int m = mb->open;
        int32_t px = w->x + mb->title_x[m];
        int32_t py = w->y + 24;
        int32_t pw = menubar_panel_w(mb, m);
        int32_t ph = mb->n_items[m] * 24 + 4;
        gfx_fill(s, px, py, pw, ph, 0x00222222);
        gfx_rect(s, px, py, pw, ph, 0x00555555);
        for (int i = 0; i < mb->n_items[m]; i++)
        {
            int32_t iy = py + 2 + i * 24;
            if (i == mb->hover_item)
                gfx_fill(s, px + 2, iy, pw - 4, 24, 0x003E3E3E);
            gfx_text(s, px + 12, iy + 4, mb->items[m][i].label,
                     i == mb->hover_item ? 0x00FFFFFF : 0x00E6E6E6);
        }
    }
}

static int menubar_event(struct UiWidget *w, const struct UiEvent *ev)
{
    struct UiMenuBar *mb = (struct UiMenuBar *)w;
    if (ev->type == UI_EVENT_MOUSE_MOVE)
    {
        int old_hm = mb->hover_menu;
        int old_hi = mb->hover_item;
        mb->hover_menu = -1;
        if (ev->y >= w->y && ev->y < w->y + 24)
        {
            for (int i = 0; i < mb->n_menus; i++)
            {
                int32_t tx = w->x + mb->title_x[i];
                if (ev->x >= tx && ev->x < tx + mb->title_w[i]) { mb->hover_menu = i; break; }
            }
        }
        mb->hover_item = -1;
        if (mb->open >= 0)
        {
            int m = mb->open;
            int32_t px = w->x + mb->title_x[m];
            int32_t py = w->y + 24;
            int32_t pw = menubar_panel_w(mb, m);
            if (ev->x >= px && ev->x < px + pw)
            {
                for (int i = 0; i < mb->n_items[m]; i++)
                {
                    int32_t iy = py + 2 + i * 24;
                    if (ev->y >= iy && ev->y < iy + 24) { mb->hover_item = i; break; }
                }
            }
            if (mb->hover_menu >= 0 && mb->hover_menu != mb->open)
                mb->open = mb->hover_menu;
        }
        return (old_hm != mb->hover_menu || old_hi != mb->hover_item);
    }
    if (ev->type == UI_EVENT_MOUSE_DOWN)
    {
        if (ev->y >= w->y && ev->y < w->y + 24)
        {
            for (int i = 0; i < mb->n_menus; i++)
            {
                int32_t tx = w->x + mb->title_x[i];
                if (ev->x >= tx && ev->x < tx + mb->title_w[i])
                {
                    mb->open = (mb->open == i) ? -1 : i;
                    return 1;
                }
            }
            mb->open = -1;
            return 0;
        }
        if (mb->open >= 0)
        {
            int m = mb->open;
            int32_t px = w->x + mb->title_x[m];
            int32_t py = w->y + 24;
            int32_t pw = menubar_panel_w(mb, m);
            int32_t ph = mb->n_items[m] * 24 + 4;
            if (ev->x >= px && ev->x < px + pw && ev->y >= py && ev->y < py + ph)
            {
                for (int i = 0; i < mb->n_items[m]; i++)
                {
                    int32_t iy = py + 2 + i * 24;
                    if (ev->y >= iy && ev->y < iy + 24)
                    {
                        ui_action_fn a = mb->items[m][i].action;
                        mb->open = -1;
                        mb->hover_item = -1;
                        if (a) a(&mb->base, mb->base.action_ctx);
                        return 1;
                    }
                }
                mb->open = -1;
                return 1;
            }
            mb->open = -1;
            return 0;
        }
    }
    return 0;
}

void ui_menubar_init(struct UiMenuBar *mb, int32_t x, int32_t y, int32_t w)
{
    memset(mb, 0, sizeof(*mb));
    mb->base.x = x;
    mb->base.y = y;
    mb->base.w = w;
    mb->base.h = 24;
    mb->base.visible = 1;
    mb->base.type = UI_TYPE_MENUBAR;
    mb->base.draw = menubar_draw;
    mb->base.on_event = menubar_event;
    mb->open = -1;
    mb->hover_menu = -1;
    mb->hover_item = -1;
}

int ui_menubar_add_menu(struct UiMenuBar *mb, const char *title)
{
    if (mb->n_menus >= UI_MENUBAR_MENUS) return -1;
    int idx = mb->n_menus;
    str_copy(mb->titles[idx], title, UI_TEXT_MAX);
    int32_t prev_x = 0, prev_w = 0;
    if (idx > 0) { prev_x = mb->title_x[idx - 1]; prev_w = mb->title_w[idx - 1]; }
    mb->title_x[idx] = prev_x + prev_w;
    mb->title_w[idx] = gfx_text_width(title) + 24;
    mb->n_menus++;
    return idx;
}

void ui_menubar_add_item(struct UiMenuBar *mb, int menu, const char *label, ui_action_fn action)
{
    if (menu < 0 || menu >= mb->n_menus) return;
    int n = mb->n_items[menu];
    if (n >= UI_MENUBAR_ITEMS) return;
    str_copy(mb->items[menu][n].label, label, UI_TEXT_MAX);
    mb->items[menu][n].action = action;
    mb->n_items[menu]++;
}

static int g_hover_changed;

int ui_hover_changed(void)
{
    int v = g_hover_changed;
    g_hover_changed = 0;
    return v;
}

int ui_widget_dispatch(struct UiWidget *list, const struct UiEvent *ev)
{
    int handled = 0;
    for (struct UiWidget *w = list; w; w = w->next)
    {
        if (!w->visible)
            continue;
        uint8_t oh = w->hover;
        if (w->on_event && w->on_event(w, ev))
            handled = 1;
        if (w->hover != oh)
            g_hover_changed = 1;
    }
    return handled;
}

void ui_widget_draw_all(struct UiWidget *list, struct gfx_surface *s)
{
    struct UiWidget *dd = 0;
    struct UiWidget *mb = 0;
    for (struct UiWidget *w = list; w; w = w->next)
    {
        if (!w->visible || !w->draw) continue;
        if ((void *)w->draw == (void *)dropdown_draw && ((struct UiDropdown *)w)->expanded)
        {
            dd = w;
            continue;
        }
        if ((void *)w->draw == (void *)menubar_draw && ((struct UiMenuBar *)w)->open >= 0)
        {
            mb = w;
            continue;
        }
        w->draw(w, s);
    }
    if (dd) dd->draw(dd, s);
    if (mb) mb->draw(mb, s);
}

static void textarea_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiTextArea *ta = (struct UiTextArea *)w;
    uint32_t bg = w->focused ? 0x002A2A2A : 0x001A1A1A;
    gfx_fill(s, w->x, w->y, w->w, w->h, bg);
    gfx_rect(s, w->x, w->y, w->w, w->h, w->focused ? 0x00417AC0 : 0x00333333);

    int32_t lx = w->x + 4;
    int32_t cx = lx;
    int32_t cy = w->y + 4 + ta->scroll_y;
    extern uint64_t sched_ticks(void);
    int blink = (sched_ticks() / 15) & 1;
    for (uint32_t i = 0; i < ta->len && cy < w->y + w->h; i++)
    {
        char ch = ta->text[i];
        if (ch == '\n') { cx = lx; cy += 16; continue; }
        if (ch < 32) continue;
        if (cx + 8 > w->x + w->w - 4) { cx = lx; cy += 16; }
        if (cy >= w->y && cy + 16 <= w->y + w->h)
        {
            char tmp[2] = {ch, 0};
            gfx_text(s, cx, cy, tmp, 0x00E6E6E6);
        }
        if (i == (uint32_t)ta->cy && w->focused && blink)
            gfx_fill(s, cx, cy, 2, 14, 0x00FFD27F);
        cx += 8;
    }
    if (w->focused && ta->cy == (int32_t)ta->len && blink)
        gfx_fill(s, cx, cy, 2, 14, 0x00FFD27F);
}

static int32_t textarea_cols(struct UiTextArea *ta)
{
    int32_t cpl = (ta->base.w - 8) / 8;
    if (cpl < 1) cpl = 1;
    return cpl;
}

static int32_t textarea_total_lines(struct UiTextArea *ta)
{
    int32_t cpl = textarea_cols(ta);
    int32_t lines = 1;
    int32_t c = 0;
    for (uint32_t i = 0; i < ta->len; i++)
    {
        if (ta->text[i] == '\n') { lines++; c = 0; }
        else { if (c >= cpl) { lines++; c = 0; } c++; }
    }
    return lines;
}

static int32_t textarea_max_scroll(struct UiTextArea *ta)
{
    int32_t m = textarea_total_lines(ta) * 16 - ta->base.h + 4;
    if (m < 0) m = 0;
    return m;
}

static void textarea_scroll_bottom(struct UiTextArea *ta)
{
    ta->scroll_y = -textarea_max_scroll(ta);
}

void ui_textarea_scroll_to_bottom(struct UiTextArea *ta)
{
    textarea_scroll_bottom(ta);
}

static int textarea_event(struct UiWidget *w, const struct UiEvent *ev)
{
    struct UiTextArea *ta = (struct UiTextArea *)w;
    if (ev->type == UI_EVENT_WHEEL)
    {
        if (!ui_widget_hit(w, ev->x, ev->y))
            return 0;
        int32_t max_scroll = textarea_max_scroll(ta);
        ta->scroll_y -= (int32_t)ev->wheel * 3 * 16;
        if (ta->scroll_y > 0) ta->scroll_y = 0;
        if (ta->scroll_y < -max_scroll) ta->scroll_y = -max_scroll;
        ta->scrolled = 1;
        return 1;
    }
    if (ev->type == UI_EVENT_MOUSE_DOWN)
    {
        w->focused = ui_widget_hit(w, ev->x, ev->y) ? 1 : 0;
        if (w->focused)
        {
            int32_t col = (ev->x - w->x - 4) / 8;
            int32_t row = (ev->y - w->y - 4 - ta->scroll_y) / 16;
            if (col < 0) col = 0;
            if (row < 0) row = 0;
            int32_t pos = 0, r = 0, c = 0;
            while (pos < (int32_t)ta->len && r < row)
            {
                if (ta->text[pos] == '\n') { r++; c = 0; pos++; }
                else { c++; pos++; if (c > 60) { r++; c = 0; } }
            }
            while (c < col && pos < (int32_t)ta->len && ta->text[pos] != '\n')
            { c++; pos++; }
            ta->cy = pos;
            ta->scrolled = 0;
        }
        return w->focused;
    }
    if (!w->focused) return 0;
    if (ev->type == UI_EVENT_KEY && ev->key)
    {
        unsigned char k = (unsigned char)ev->key;
        if (ev->key == '\b' || ev->key == 127)
        {
            if (ta->len > 0 && ta->cy > ta->min_cursor)
            {
                for (int32_t i = (int32_t)(ta->cy) - 1; (uint32_t)i < ta->len; i++)
                    ta->text[i] = ta->text[i + 1];
                ta->len--;
                ta->cy--;
                if (ta->follow_bottom) textarea_scroll_bottom(ta);
                if (ta->on_change) ta->on_change(ta);
            }
            return 1;
        }
        if (k == KEY_LEFT  && ta->cy > ta->min_cursor) { ta->cy--; return 1; }
        if (k == KEY_RIGHT && (uint32_t)ta->cy < ta->len) { ta->cy++; return 1; }
        if ((k == KEY_UP || k == KEY_DOWN) && ta->on_history)
        {
            ta->on_history(ta, k == KEY_UP ? -1 : 1);
            if (ta->follow_bottom) textarea_scroll_bottom(ta);
            return 1;
        }
        if (k == KEY_UP)
        {
            int32_t p = ta->cy;
            while (p > 0 && ta->text[p-1] != '\n') p--;
            if (p > 0) p--;
            while (p > 0 && ta->text[p-1] != '\n') p--;
            if (p >= 0) ta->cy = p;
            return 1;
        }
        if (k == KEY_DOWN)
        {
            int32_t p = (int32_t)ta->cy;
            while ((uint32_t)p < ta->len && ta->text[p] != '\n') p++;
            if ((uint32_t)p < ta->len) p++;
            if ((uint32_t)p >= ta->len) return 1;
            int32_t col = (int32_t)ta->cy;
            while (col > 0 && ta->text[col-1] != '\n') col--;
            col = (int32_t)ta->cy - col;
            int32_t dc = 0;
            while ((uint32_t)p < ta->len && ta->text[p] != '\n' && dc < col) { p++; dc++; }
            ta->cy = p;
            return 1;
        }
        if (ev->key == '\r') { k = '\n'; }
        if (ev->key == '\n')
        {
            if (ta->on_enter) { ta->on_enter(ta); return 1; }
            if (ta->len < UI_AREA_MAX - 1)
            {
                for (int32_t i = (int32_t)ta->len; i > ta->cy; i--)
                    ta->text[i] = ta->text[i - 1];
                ta->text[ta->cy] = '\n';
                ta->len++;
                ta->cy++;
                ta->text[ta->len] = 0;
                if (ta->follow_bottom) textarea_scroll_bottom(ta);
                if (ta->on_change) ta->on_change(ta);
                return 1;
            }
        }
        if (k >= 32 && ta->len < UI_AREA_MAX - 1)
        {
            for (int32_t i = (int32_t)ta->len; i > ta->cy; i--)
                ta->text[i] = ta->text[i - 1];
            ta->text[ta->cy] = (char)k;
            ta->len++;
            ta->cy++;
            ta->text[ta->len] = 0;
            if (ta->follow_bottom) textarea_scroll_bottom(ta);
            if (ta->on_change) ta->on_change(ta);
            return 1;
        }
        return 1;
    }
    return 0;
}

void ui_textarea_init(struct UiTextArea *ta, int32_t x, int32_t y, int32_t w, int32_t h)
{
    memset(ta, 0, sizeof(*ta));
    ta->base.x = x;
    ta->base.y = y;
    ta->base.w = w;
    ta->base.h = h;
    ta->base.visible = 1;
    ta->base.type = UI_TYPE_TEXTAREA;
    ta->base.draw = textarea_draw;
    ta->base.on_event = textarea_event;
}

void ui_textarea_set_text(struct UiTextArea *ta, const char *text)
{
    ta->len = 0;
    while (text[ta->len] && ta->len < UI_AREA_MAX - 1) { ta->text[ta->len] = text[ta->len]; ta->len++; }
    ta->text[ta->len] = 0;
}

static void scrollbar_draw(struct UiWidget *w, struct gfx_surface *s)
{
    struct UiScrollbar *sb = (struct UiScrollbar *)w;
    gfx_fill(s, w->x, w->y, w->w, w->h, 0x001A1A1A);
    if (sb->max_val <= 0)
        return;
    int32_t range = w->h - 4;
    int32_t thumb_h = (sb->thumb_size > 0) ? sb->thumb_size : 16;
    if (thumb_h > range) thumb_h = range;
    int32_t thumb_y = w->y + 2;
    if (sb->max_val > 0)
        thumb_y += (int32_t)((int64_t)sb->cur_val * (range - thumb_h) / sb->max_val);
    gfx_fill(s, w->x + 1, thumb_y, w->w - 2, thumb_h, 0x00417AC0);
}

static int scrollbar_event(struct UiWidget *w, const struct UiEvent *ev)
{
    struct UiScrollbar *sb = (struct UiScrollbar *)w;
    if (ev->type == UI_EVENT_WHEEL && ui_widget_hit(w, ev->x, ev->y))
    {
        if (sb->max_val <= 0) return 1;
        sb->cur_val += (int32_t)ev->wheel * 3 * 16;
        if (sb->cur_val < 0) sb->cur_val = 0;
        if (sb->cur_val > sb->max_val) sb->cur_val = sb->max_val;
        if (sb->target)
        {
            sb->target->scroll_y = -sb->cur_val;
            sb->target->scrolled = 1;
        }
        return 1;
    }
    if (ev->type == UI_EVENT_MOUSE_DOWN && ui_widget_hit(w, ev->x, ev->y))
    {
        sb->dragging = 1;
        w->pressed = 1;
        goto update;
    }
    if (ev->type == UI_EVENT_MOUSE_UP)
    {
        sb->dragging = 0;
        w->pressed = 0;
        return 1;
    }
    if (ev->type == UI_EVENT_MOUSE_MOVE && sb->dragging)
        goto update;
    return 0;
update:
    {
        int32_t rel = ev->y - w->y - 2;
        int32_t range = w->h - 4;
        int32_t thumb_h = (sb->thumb_size > 0) ? sb->thumb_size : 16;
        if (thumb_h > range) thumb_h = range;
        if (range > thumb_h)
            sb->cur_val = (int32_t)((int64_t)rel * sb->max_val / (range - thumb_h));
        if (sb->cur_val < 0) sb->cur_val = 0;
        if (sb->cur_val > sb->max_val) sb->cur_val = sb->max_val;
        if (sb->target)
        {
            sb->target->scroll_y = -sb->cur_val;
            sb->target->scrolled = 1;
        }
        return 1;
    }
}

void ui_scrollbar_init(struct UiScrollbar *sb, int32_t x, int32_t y, int32_t h, int32_t max_val)
{
    memset(sb, 0, sizeof(*sb));
    sb->base.x = x;
    sb->base.y = y;
    sb->base.w = 14;
    sb->base.h = h;
    sb->base.visible = 1;
    sb->base.type = UI_TYPE_SCROLLBAR;
    sb->base.draw = scrollbar_draw;
    sb->base.on_event = scrollbar_event;
    sb->max_val = max_val;
    sb->cur_val = 0;
    sb->thumb_size = 20;
}

void ui_scrollbar_set_value(struct UiScrollbar *sb, int32_t val)
{
    sb->cur_val = val;
    if (sb->cur_val < 0) sb->cur_val = 0;
    if (sb->cur_val > sb->max_val) sb->cur_val = sb->max_val;
}

void ui_scrollbar_sync(struct UiScrollbar *sb, struct UiTextArea *ta)
{
    int32_t total_lines = textarea_total_lines(ta);
    int32_t content_px = total_lines * 16 + 4;
    int32_t view_px = ta->base.h;
    int32_t maxv = textarea_max_scroll(ta);
    sb->max_val = maxv;
    int32_t range = sb->base.h - 4;
    if (range < 1) range = 1;
    int32_t thumb;
    if (content_px <= view_px) thumb = range;
    else thumb = (int32_t)((int64_t)view_px * range / content_px);
    if (thumb < 16) thumb = 16;
    if (thumb > range) thumb = range;
    sb->thumb_size = thumb;
    int32_t cv = -ta->scroll_y;
    if (cv < 0) cv = 0;
    if (cv > maxv) cv = maxv;
    sb->cur_val = cv;
}
