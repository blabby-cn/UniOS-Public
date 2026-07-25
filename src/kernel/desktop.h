#ifndef UNI_DESKTOP_H
#define UNI_DESKTOP_H

#include <stdint.h>

int desktop_init(uint32_t screen_w, uint32_t screen_h);
void desktop_tick(uint64_t tick);
int desktop_autotest_done(void);
int desktop_in_console(void);

extern volatile uint64_t desktop_thread_runs[3];

#endif
