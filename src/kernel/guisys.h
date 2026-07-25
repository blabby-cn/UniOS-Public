#ifndef UNI_GUISYS_H
#define UNI_GUISYS_H

#include <stdint.h>

#define GUI_WIDGET_BUTTON   1
#define GUI_WIDGET_TOGGLE   2
#define GUI_WIDGET_TEXTINPUT 3
#define GUI_WIDGET_TEXT     4

#define GUI_EV_NONE   0
#define GUI_EV_CLICK  1
#define GUI_EV_SUBMIT 2

struct gui_widget_req
{
    uint32_t type;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    char label[64];
};

struct gui_event
{
    uint32_t win_id;
    uint32_t widget_idx;
    uint8_t type;
};

int guisys_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h);
int guisys_destroy(uint32_t win_id);
int guisys_widget(uint32_t win_id, const struct gui_widget_req *req);
int guisys_set_visible(uint32_t win_id, int visible);
int guisys_text(uint32_t win_id, int32_t x, int32_t y, const char *text);
int guisys_label(uint32_t win_id, uint32_t text_idx, const char *str);
int guisys_get_text(uint32_t win_id, uint32_t widget_idx, char *buf, uint32_t cap);
int guisys_set_text(uint32_t win_id, uint32_t widget_idx, const char *str);
int guisys_poll(uint32_t win_id, struct gui_event *ev);
int guisys_focus_by_title(const char *title);

#endif
