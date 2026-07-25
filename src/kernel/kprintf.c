#include "kprintf.h"
#include "console.h"
#include "serial.h"

#include <stdint.h>
#include <stdarg.h>

static void emit(const char *s)
{
    unsigned long flags;
    __asm__ volatile("pushfq; cli; pop %0" : "=r"(flags));
    console_write(s);
    serial_write(s);
    __asm__ volatile("push %0; popfq" :: "r"(flags));
}

void kputs(const char *utf8)
{
    emit(utf8);
}

static void emit_uint(uint64_t v, unsigned base, int upper, int width, char pad)
{
    const char *digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[32];
    int i = 0;
    if (v == 0)
    {
        buf[i++] = '0';
    }
    while (v)
    {
        buf[i++] = digs[v % base];
        v /= base;
    }
    while (i < width)
    {
        buf[i++] = pad;
    }
    char out[33];
    int j = 0;
    while (i > 0)
    {
        out[j++] = buf[--i];
    }
    out[j] = 0;
    emit(out);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const char *f = fmt;
    char tmp[2];
    tmp[1] = 0;
    while (*f)
    {
        if (*f != '%')
        {
            tmp[0] = *f++;
            emit(tmp);
            continue;
        }
        f++;
        char pad = ' ';
        int width = 0;
        if (*f == '0')
        {
            pad = '0';
            f++;
        }
        while (*f >= '0' && *f <= '9')
        {
            width = width * 10 + (*f - '0');
            f++;
        }
        int lng = 0;
        while (*f == 'l')
        {
            lng++;
            f++;
        }
        char c = *f++;
        if (c == '%')
        {
            emit("%");
        }
        else if (c == 'c')
        {
            int ch = va_arg(ap, int);
            tmp[0] = (char)ch;
            emit(tmp);
        }
        else if (c == 's')
        {
            const char *s = va_arg(ap, const char *);
            emit(s ? s : "(null)");
        }
        else if (c == 'd' || c == 'i')
        {
            long long v = lng ? va_arg(ap, long long) : (long long)va_arg(ap, int);
            if (v < 0)
            {
                emit("-");
                emit_uint((uint64_t)(-v), 10, 0, width, pad);
            }
            else
            {
                emit_uint((uint64_t)v, 10, 0, width, pad);
            }
        }
        else if (c == 'u')
        {
            uint64_t v = lng ? va_arg(ap, unsigned long long) : (uint64_t)va_arg(ap, unsigned int);
            emit_uint(v, 10, 0, width, pad);
        }
        else if (c == 'x')
        {
            uint64_t v = lng ? va_arg(ap, unsigned long long) : (uint64_t)va_arg(ap, unsigned int);
            emit_uint(v, 16, 0, width, pad);
        }
        else if (c == 'X')
        {
            uint64_t v = lng ? va_arg(ap, unsigned long long) : (uint64_t)va_arg(ap, unsigned int);
            emit_uint(v, 16, 1, width, pad);
        }
        else if (c == 'p')
        {
            uint64_t v = (uint64_t)(unsigned long)va_arg(ap, void *);
            emit("0x");
            emit_uint(v, 16, 0, 0, ' ');
        }
        else
        {
            emit("%");
            tmp[0] = c;
            emit(tmp);
        }
    }
    va_end(ap);
}
