#include "Uni.h"

__attribute__((section(".entry"))) __attribute__((noreturn))
void _start(void)
{
    int win = Uni_Window("Clock", 900, 500, 240, 160);
    if (win < 0) sys_exit(1);

    Uni_Text(win, 16, 20, "UniOS Clock");
    Uni_Text(win, 16, 56, "12:00");
    Uni_Show(win, 1);

    for (;;)
    {
        struct UniEvent ev;
        while (Uni_Poll(win, &ev)) {}
        sys_yield();
    }
}
