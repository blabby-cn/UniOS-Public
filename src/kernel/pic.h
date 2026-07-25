#ifndef UNI_PIC_H
#define UNI_PIC_H

#include <stdint.h>

void pic_init(void);
void pic_eoi(uint8_t irq);
void pic_unmask(uint8_t irq);
void pic_mask(uint8_t irq);

#endif
