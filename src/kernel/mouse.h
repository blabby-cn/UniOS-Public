#ifndef UNI_MOUSE_H
#define UNI_MOUSE_H

#include <stdint.h>

void mouse_init(uint32_t screen_w, uint32_t screen_h);
void mouse_resync(void);
int32_t mouse_x(void);
int32_t mouse_y(void);
uint8_t mouse_buttons(void);
uint64_t mouse_event_count(void);
int32_t mouse_wheel(void);

#endif
