#include "Uni.h"

#define MAX_BODY 200

static int g_win;
static int g_path;
static int g_body;
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

static int smatch(const char *a, const char *b)
{
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

static void do_open(void) __attribute__((noinline));
static void do_save(void) __attribute__((noinline));

static void show_status(const char *msg)
{
    Uni_Label(g_win, g_status, msg);
}

static void do_open(void)
{
    char path[128];
    if (Uni_GetText(g_win, g_path, path, sizeof(path)) <= 0)
    {
        show_status("no path");
        return;
    }
    if (Uni_FOpen(0, path) < 0)
    {
        show_status("open failed");
        return;
    }
    char buf[MAX_BODY];
    int n = Uni_FRead(0, buf, sizeof(buf) - 1);
    Uni_FClose(0);
    if (n < 0)
    {
        show_status("read failed");
        return;
    }
    buf[n] = 0;
    Uni_SetText(g_win, g_body, buf);
    show_status("loaded");
}

static void do_save(void)
{
    char path[128];
    char buf[MAX_BODY];
    if (Uni_GetText(g_win, g_path, path, sizeof(path)) <= 0)
    {
        show_status("no path");
        return;
    }
    int n = Uni_GetText(g_win, g_body, buf, sizeof(buf));
    if (n < 0)
    {
        show_status("body read failed");
        return;
    }
    if (Uni_FOpen(0, path) < 0)
    {
        show_status("open failed");
        return;
    }
    int rc = Uni_FWrite(0, buf, (unsigned)n);
    Uni_FClose(0);
    if (rc)
        show_status("write failed");
    else
        show_status("saved");
}

__attribute__((section(".entry"))) __attribute__((noreturn))
void _start(void)
{
    g_win = Uni_Window("Notepad", 860, 20, 400, 320);
    if (g_win < 0) sys_exit(1);

    struct UniWidgetReq req;
    for (int i = 0; i < 64; i++) req.label[i] = 0;

    Uni_Text(g_win, 16, 20, "UniOS Notepad");
    Uni_Text(g_win, 16, 42, "Path:");

    req.type = UNI_WIDGET_TEXTINPUT;
    req.x = 16; req.y = 60; req.w = 368; req.h = 24;
    scpy(req.label, "/var/documents/note.txt", 64);
    g_path = Uni_Widget(g_win, &req);

    Uni_Text(g_win, 16, 92, "Body:");
    req.x = 16; req.y = 110; req.w = 368; req.h = 24;
    req.label[0] = 0;
    g_body = Uni_Widget(g_win, &req);

    req.type = UNI_WIDGET_BUTTON;
    req.x = 16; req.y = 148; req.w = 80; req.h = 28;
    scpy(req.label, "Open", 64);
    int open_btn = Uni_Widget(g_win, &req);

    req.x = 112; req.y = 148; req.w = 80; req.h = 28;
    scpy(req.label, "Save", 64);
    int save_btn = Uni_Widget(g_win, &req);

    g_status = Uni_Text(g_win, 16, 188, "ready");

    char init_path[128];
    scpy(init_path, "/etc/motd", sizeof(init_path));
    Uni_SetText(g_win, g_path, init_path);
    do_open();

    Uni_Show(g_win, 1);

    for (;;)
    {
        struct UniEvent ev;
        while (Uni_Poll(g_win, &ev))
        {
            if (ev.type == UNI_EV_CLICK)
            {
                if (ev.widget_idx == open_btn) do_open();
                else if (ev.widget_idx == save_btn) do_save();
            }
        }
        sys_yield();
    }
}
