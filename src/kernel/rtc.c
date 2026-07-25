#include "rtc.h"
#include "io.h"

static uint8_t rtc_read_reg(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd2bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

void rtc_read(rtc_time *t)
{
    uint8_t s = rtc_read_reg(0x00);
    uint8_t m = rtc_read_reg(0x02);
    uint8_t h = rtc_read_reg(0x04);
    uint8_t d = rtc_read_reg(0x07);
    uint8_t mo = rtc_read_reg(0x08);
    uint8_t y = rtc_read_reg(0x09);

    uint8_t regb = rtc_read_reg(0x0B);
    if (!(regb & 0x04))
    {
        s = bcd2bin(s);
        m = bcd2bin(m);
        h = bcd2bin(h);
        d = bcd2bin(d);
        mo = bcd2bin(mo);
        y = bcd2bin(y);
    }

    t->sec = s;
    t->min = m;
    t->hour = h;
    t->day = d;
    t->month = mo;
    t->year = (uint16_t)y + 2000;
}
