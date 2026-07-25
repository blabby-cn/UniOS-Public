#ifndef UNI_UI_H
#define UNI_UI_H

#include <stdint.h>
#include "gfx.h"

#define UI_EVENT_MOUSE_DOWN 1
#define UI_EVENT_MOUSE_UP 2
#define UI_EVENT_MOUSE_MOVE 3
#define UI_EVENT_KEY 4
#define UI_EVENT_WHEEL 5

#define UI_TEXT_MAX 64
#define UI_AREA_MAX 2048

#define UI_TYPE_WIDGET   0
#define UI_TYPE_BUTTON   1
#define UI_TYPE_TOGGLE   2
#define UI_TYPE_TEXTINPUT 3
#define UI_TYPE_TEXTAREA 4
#define UI_TYPE_SCROLLBAR 5
#define UI_TYPE_CHECKBOX 6
#define UI_TYPE_RADIO     7
#define UI_TYPE_DROPDOWN 8
#define UI_TYPE_IMAGE    9
#define UI_TYPE_MENUBAR  10

#define UI_ANCHOR_NONE         0
#define UI_ANCHOR_TOP          1
#define UI_ANCHOR_BOTTOM       2
#define UI_ANCHOR_LEFT         3
#define UI_ANCHOR_RIGHT        4
#define UI_ANCHOR_TOP_LEFT     5
#define UI_ANCHOR_TOP_RIGHT    6
#define UI_ANCHOR_BOTTOM_LEFT  7
#define UI_ANCHOR_BOTTOM_RIGHT 8
#define UI_ANCHOR_TOP_FILL     9
#define UI_ANCHOR_BOTTOM_FILL  10
#define UI_ANCHOR_LEFT_FILL    11
#define UI_ANCHOR_RIGHT_FILL   12
#define UI_ANCHOR_FILL         13
#define UI_ANCHOR_CENTER       14

struct UiEvent
{
    uint8_t type;
    int32_t x;
    int32_t y;
    uint8_t buttons;
    char key;
    int32_t wheel;
};

struct UiWidget;

typedef void (*ui_draw_fn)(struct UiWidget *w, struct gfx_surface *s);
typedef int (*ui_event_fn)(struct UiWidget *w, const struct UiEvent *ev);
typedef void (*ui_action_fn)(struct UiWidget *w, void *ctx);

struct UiWidget
{
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    uint8_t hover;
    uint8_t pressed;
    uint8_t focused;
    uint8_t visible;
    uint8_t type;
    const char *tip;
    ui_draw_fn draw;
    ui_event_fn on_event;
    ui_action_fn action;
    void *action_ctx;
    uint8_t anchor;
    int32_t margin_l;
    int32_t margin_r;
    int32_t margin_t;
    int32_t margin_b;
    int32_t base_w;
    int32_t base_h;
    struct UiWidget *next;
};

struct UiButton
{
    struct UiWidget base;
    char label[UI_TEXT_MAX];
    uint32_t accent;
};

struct UiToggle
{
    struct UiWidget base;
    char label[UI_TEXT_MAX];
    uint8_t on;
};

struct UiTextInput
{
    struct UiWidget base;
    char text[UI_TEXT_MAX];
    uint32_t len;
    uint32_t cursor_pos;
    char placeholder[UI_TEXT_MAX];
};

struct UiTextArea
{
    struct UiWidget base;
    char text[UI_AREA_MAX];
    uint32_t len;
    int32_t cx;
    int32_t cy;
    int32_t sel_start;
    int32_t sel_end;
    int32_t scroll_y;
    int32_t min_cursor;
    int32_t lines;
    int32_t scrolled;
    uint8_t follow_bottom;
    void (*on_enter)(struct UiTextArea *ta);
    void (*on_history)(struct UiTextArea *ta, int32_t dir);
    void (*on_change)(struct UiTextArea *ta);
};

struct UiScrollbar
{
    struct UiWidget base;
    int32_t max_val;
    int32_t cur_val;
    int32_t thumb_size;
    uint8_t dragging;
    struct UiTextArea *target;
    uint32_t track_color;
    uint32_t thumb_color;
};

void ui_textarea_init(struct UiTextArea *ta, int32_t x, int32_t y, int32_t w, int32_t h);
void ui_textarea_set_text(struct UiTextArea *ta, const char *text);
void ui_textarea_scroll_to_bottom(struct UiTextArea *ta);
void ui_scrollbar_init(struct UiScrollbar *sb, int32_t x, int32_t y, int32_t h, int32_t max_val);
void ui_scrollbar_set_value(struct UiScrollbar *sb, int32_t val);
void ui_scrollbar_sync(struct UiScrollbar *sb, struct UiTextArea *ta);

struct UiCheckbox
{
    struct UiWidget base;
    char label[UI_TEXT_MAX];
    uint8_t checked;
};

struct UiRadio
{
    struct UiWidget base;
    char label[UI_TEXT_MAX];
    uint8_t selected;
    int group;
};

struct UiDropdown
{
    struct UiWidget base;
    char options[8][UI_TEXT_MAX];
    int n_options;
    int selected;
    int expanded;
};

struct UiImage
{
    struct UiWidget base;
    uint8_t *pixels;
    int32_t img_w;
    int32_t img_h;
};

struct UiMenuItem
{
    char label[UI_TEXT_MAX];
    ui_action_fn action;
};

struct UiStartMenu
{
    int32_t x;
    int32_t y;
    int32_t w;
    uint8_t visible;
    struct UiMenuItem items[12];
    int n_items;
    int hover_idx;
};

#define UI_MENUBAR_MENUS 4
#define UI_MENUBAR_ITEMS 8

struct UiMenuBar
{
    struct UiWidget base;
    char titles[UI_MENUBAR_MENUS][UI_TEXT_MAX];
    int32_t title_x[UI_MENUBAR_MENUS];
    int32_t title_w[UI_MENUBAR_MENUS];
    struct UiMenuItem items[UI_MENUBAR_MENUS][UI_MENUBAR_ITEMS];
    int n_items[UI_MENUBAR_MENUS];
    int n_menus;
    int open;
    int hover_menu;
    int hover_item;
};

void ui_button_init(struct UiButton *b, int32_t x, int32_t y, int32_t w, int32_t h, const char *label, uint32_t accent);
void ui_button_set_label(struct UiButton *b, const char *label);
void ui_toggle_init(struct UiToggle *t, int32_t x, int32_t y, const char *label, uint8_t on);
void ui_toggle_set_label(struct UiToggle *t, const char *label);
void ui_textinput_init(struct UiTextInput *t, int32_t x, int32_t y, int32_t w, const char *placeholder);
void ui_textinput_set_placeholder(struct UiTextInput *t, const char *placeholder);
void ui_checkbox_init(struct UiCheckbox *c, int32_t x, int32_t y, const char *label, uint8_t checked);
void ui_radio_init(struct UiRadio *r, int32_t x, int32_t y, const char *label, int group, uint8_t selected);
void ui_radio_set_group(struct UiWidget *list, int group, struct UiRadio *sel);
void ui_dropdown_init(struct UiDropdown *d, int32_t x, int32_t y, int32_t w);
void ui_dropdown_add(struct UiDropdown *d, const char *option);
void ui_image_init(struct UiImage *img, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *pixels, int32_t iw, int32_t ih);
void ui_menubar_init(struct UiMenuBar *mb, int32_t x, int32_t y, int32_t w);
int ui_menubar_add_menu(struct UiMenuBar *mb, const char *title);
void ui_menubar_add_item(struct UiMenuBar *mb, int menu, const char *label, ui_action_fn action);
void ui_startmenu_init(struct UiStartMenu *m, int32_t x, int32_t y, int32_t w);
void ui_startmenu_add(struct UiStartMenu *m, const char *label, ui_action_fn action);
void ui_startmenu_set_label(struct UiStartMenu *m, int idx, const char *label);
void ui_startmenu_draw(struct UiStartMenu *m, struct gfx_surface *s);
int ui_startmenu_click(struct UiStartMenu *m, int32_t mx, int32_t my);
void ui_startmenu_move(struct UiStartMenu *m, int32_t mx, int32_t my);
int ui_widget_hit(const struct UiWidget *w, int32_t x, int32_t y);
void ui_widget_set_anchor(struct UiWidget *w, uint8_t flags, int32_t cw, int32_t ch);
void ui_widget_layout(struct UiWidget *w, int32_t cw, int32_t ch);
int ui_widget_dispatch(struct UiWidget *list, const struct UiEvent *ev);
void ui_widget_draw_all(struct UiWidget *list, struct gfx_surface *s);
void ui_widget_set_tip(struct UiWidget *w, const char *key);
const char *ui_widget_default_tip(const struct UiWidget *w);
int ui_startmenu_item_at(const struct UiStartMenu *m, int32_t mx, int32_t my);

#endif
