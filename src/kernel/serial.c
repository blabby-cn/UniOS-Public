#include "serial.h"
#include "io.h"

#define COM1 0x3F8
#define COM2 0x2F8

static void port_init(uint16_t p)
{
    outb(p + 1, 0x00);
    outb(p + 3, 0x80);
    outb(p + 0, 0x01);
    outb(p + 1, 0x00);
    outb(p + 3, 0x03);
    outb(p + 2, 0xC7);
    outb(p + 4, 0x0B);
}

void serial_init(void)
{
    port_init(COM1);
}

void serial_putc(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
    {
    }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
        {
            serial_putc('\r');
        }
        serial_putc(*s++);
    }
}

void serial2_init(void)
{
    port_init(COM2);
}

void serial2_putb(uint8_t b)
{
    while ((inb(COM2 + 5) & 0x20) == 0)
    {
    }
    outb(COM2, b);
}
