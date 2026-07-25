#ifndef UNI_FONT_H
#define UNI_FONT_H

#include <stdint.h>

extern const uint8_t unifont_glyphs[];
extern const uint8_t unifont_widths[];

const uint8_t *font_glyph(uint32_t cp, int *width);

#endif
