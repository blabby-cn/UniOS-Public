#include "Uni.h"

static const char g_lbl[16] = {
    '7', '8', '9', '/',
    '4', '5', '6', '*',
    '1', '2', '3', '-',
    'C', '0', '=', '+'
};

static int g_win;
static int g_disp;
static char g_expr[64];
static int g_len;
static int g_shown;

static int ltoa(long v, char *buf)
{
    char tmp[24];
    int n = 0;
    int neg = 0;
    unsigned long u;
    if (v < 0) { neg = 1; u = (unsigned long)(-v); }
    else u = (unsigned long)v;
    if (u == 0) tmp[n++] = '0';
    while (u) { tmp[n++] = (char)('0' + (int)(u % 10)); u /= 10; }
    int m = 0;
    if (neg) buf[m++] = '-';
    while (n) buf[m++] = tmp[--n];
    buf[m] = 0;
    return m;
}

static int is_op(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/';
}

static long apply(char op, long a, long b)
{
    if (op == '+') return a + b;
    if (op == '-') return a - b;
    if (op == '*') return a * b;
    if (op == '/') return b ? a / b : 0;
    return b;
}

static long eval_expr(const char *s)
{
    long acc = 0;
    long cur = 0;
    char op = '+';
    for (int i = 0; s[i]; i++)
    {
        char c = s[i];
        if (c >= '0' && c <= '9')
            cur = cur * 10 + (long)(c - '0');
        else if (is_op(c))
        {
            acc = apply(op, acc, cur);
            op = c;
            cur = 0;
        }
    }
    return apply(op, acc, cur);
}

static void show_expr(void)
{
    if (g_len == 0) { Uni_Label(g_win, g_disp, "0"); return; }
    const char *p = g_expr;
    if (g_len > 30) p = g_expr + (g_len - 30);
    Uni_Label(g_win, g_disp, p);
}

static void press(char c)
{
    if (c >= '0' && c <= '9')
    {
        if (g_shown) { g_len = 0; g_expr[0] = 0; g_shown = 0; }
        if (g_len < 62) { g_expr[g_len++] = c; g_expr[g_len] = 0; }
        show_expr();
        return;
    }
    if (c == 'C')
    {
        g_len = 0; g_expr[0] = 0; g_shown = 0;
        show_expr();
        return;
    }
    if (c == '=')
    {
        if (g_len == 0) return;
        if (is_op(g_expr[g_len - 1])) { g_len--; g_expr[g_len] = 0; }
        if (g_len == 0) return;
        long r = eval_expr(g_expr);
        g_len = ltoa(r, g_expr);
        g_shown = 1;
        show_expr();
        return;
    }
    if (g_shown) g_shown = 0;
    if (g_len == 0)
    {
        if (c == '-') { g_expr[g_len++] = c; g_expr[g_len] = 0; show_expr(); }
        return;
    }
    if (is_op(g_expr[g_len - 1]))
        g_expr[g_len - 1] = c;
    else if (g_len < 62)
    {
        g_expr[g_len++] = c; g_expr[g_len] = 0;
    }
    show_expr();
}

__attribute__((section(".entry")))
void _start(void)
{
    g_len = 0;
    g_expr[0] = 0;
    g_shown = 0;

    g_win = Uni_Window("Calculator", 440, 20, 280, 380);
    if (g_win < 0) sys_exit(1);

    Uni_Text(g_win, 16, 20, "UniOS Calculator");
    g_disp = Uni_Text(g_win, 16, 56, "0");

    struct UniWidgetReq req;
    for (int i = 0; i < 64; i++) req.label[i] = 0;
    req.type = UNI_WIDGET_BUTTON;
    req.w = 56;
    req.h = 44;
    for (int i = 0; i < 16; i++)
    {
        req.x = 16 + (i % 4) * 64;
        req.y = 92 + (i / 4) * 52;
        req.label[0] = g_lbl[i];
        req.label[1] = 0;
        Uni_Widget(g_win, &req);
    }

    Uni_Show(g_win, 1);

    for (;;)
    {
        struct UniEvent ev;
        while (Uni_Poll(g_win, &ev))
        {
            if (ev.type == UNI_EV_CLICK && ev.widget_idx < 16)
                press(g_lbl[ev.widget_idx]);
        }
        sys_yield();
    }
}
