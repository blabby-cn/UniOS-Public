#include "font.h"

const uint8_t *font_glyph(uint32_t cp, int *width)
{
    if (cp > 0xFFFF)
    {
        cp = '?';
    }
    int w = unifont_widths[cp];
    if (w != 8 && w != 16)
    {
        w = 8;
    }
    *width = w;
    return unifont_glyphs + (unsigned long)cp * 32;
}
