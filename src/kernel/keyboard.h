#ifndef UNI_KEYBOARD_H
#define UNI_KEYBOARD_H

#include <stdint.h>

#define KEY_UP    128
#define KEY_DOWN  129
#define KEY_LEFT  130
#define KEY_RIGHT 131
#define KEY_HOME  132
#define KEY_END   133

void keyboard_init(void);
uint64_t keyboard_event_count(void);
void keyboard_set_sink(void (*fn)(char c));
int keyboard_poll_read(char *c);
int keyboard_ctrl_pressed(void);
int keyboard_hotkey_ctrl_r(void);
int keyboard_hotkey_ctrl_esc(void);
int keyboard_hotkey_ctrl_s(void);

#endif
