#ifndef UNI_FBDUMP_H
#define UNI_FBDUMP_H

#include <stdint.h>

void fb_dump(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height);
void fb_set_current(uint64_t fb_addr, uint32_t pitch, uint32_t width, uint32_t height);
void fb_dump_current(void);
uint64_t fb_get_addr(void);
uint32_t fb_get_pitch(void);
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);

#endif
