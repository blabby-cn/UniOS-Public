#ifndef UNI_TOOLTIP_H
#define UNI_TOOLTIP_H

#include "gfx.h"

void tooltip_init(void);
void tooltip_set(const char *text, int32_t mx, int32_t my, uint64_t tick);
void tooltip_paint(struct gfx_surface *s);

#endif
