#ifndef UNI_CONSOLE_H
#define UNI_CONSOLE_H

#include <stdint.h>

void console_init(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp);
void console_clear(void);
void console_set_color(uint32_t fg, uint32_t bg);
void console_write(const char *utf8);
void console_set_suppress(int on);
void console_backspace(void);

#endif
