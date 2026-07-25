#include "Uni.h"

static int g_win;
static int g_input;
static int g_status;

static int slen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void scpy(char *d, const char *s, int max)
{
    int i = 0;
    while (i < max - 1 && s[i]) { d[i] = s[i]; i++; }
    d[i] = 0;
}

static void show_status(const char *msg)
{
    Uni_Label(g_win, g_status, msg);
}

__attribute__((section(".entry"))) __attribute__((noreturn))
void _start(void)
{
    g_win = Uni_Window("Terminal", 440, 380, 400, 340);
    if (g_win < 0) sys_exit(1);

    struct UniWidgetReq req;
    for (int i = 0; i < 64; i++) req.label[i] = 0;

    Uni_Text(g_win, 16, 20, "UniOS Terminal v1.0");
    Uni_Text(g_win, 16, 42, "ring3 user shell");

    req.type = UNI_WIDGET_TEXTINPUT;
    req.x = 16; req.y = 70; req.w = 280; req.h = 24;
    scpy(req.label, "type command", 64);
    g_input = Uni_Widget(g_win, &req);

    req.type = UNI_WIDGET_BUTTON;
    req.x = 304; req.y = 70; req.w = 80; req.h = 24;
    scpy(req.label, "Run", 64);
    int run_btn = Uni_Widget(g_win, &req);

    g_status = Uni_Text(g_win, 16, 110, "ready");

    Uni_Show(g_win, 1);

    for (;;)
    {
        struct UniEvent ev;
        while (Uni_Poll(g_win, &ev))
        {
            if (ev.type == UNI_EV_CLICK && ev.widget_idx == run_btn)
            {
                char cmd[128];
                int n = Uni_GetText(g_win, g_input, cmd, sizeof(cmd));
                if (n > 0)
                {
                    cmd[n] = 0;
                    show_status(cmd);
                }
            }
            if (ev.type == UNI_EV_SUBMIT && ev.widget_idx == g_input)
            {
                char cmd[128];
                int n = Uni_GetText(g_win, g_input, cmd, sizeof(cmd));
                if (n > 0)
                {
                    cmd[n] = 0;
                    show_status(cmd);
                }
            }
        }
        sys_yield();
    }
}
