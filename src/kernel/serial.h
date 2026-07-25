#ifndef UNI_SERIAL_H
#define UNI_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);

void serial2_init(void);
void serial2_putb(uint8_t b);

#endif
