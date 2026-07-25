#ifndef UNI_H
#define UNI_H

#include "syscall.h"

#define UNI_WIDGET_BUTTON   1
#define UNI_WIDGET_TEXTINPUT 3
#define UNI_WIDGET_TEXT     4

#define UNI_EV_NONE   0
#define UNI_EV_CLICK  1
#define UNI_EV_SUBMIT 2

struct UniWidgetReq
{
    int type;
    int x;
    int y;
    int w;
    int h;
    char label[64];
};

struct UniEvent
{
    unsigned int win_id;
    unsigned int widget_idx;
    unsigned char type;
};

static inline int Uni_Window(const char *title, int x, int y, int w, int h)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(6), "D"((long)(title)), "S"((long)(x)), "d"((long)(y)), "c"((long)(w)), "b"((long)(h)) : "memory", "r10", "r8");
    return (int)ret;
}

static inline int Uni_Destroy(int win_id)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(7), "D"((long)(win_id)) : "memory");
    return (int)ret;
}

static inline int Uni_Widget(int win_id, struct UniWidgetReq *req)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(8), "D"((long)(win_id)), "S"((long)(req)) : "memory");
    return (int)ret;
}

static inline int Uni_Poll(int win_id, void *ev)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(9), "D"((long)(win_id)), "S"((long)(ev)) : "memory");
    return (int)ret;
}

static inline int Uni_Show(int win_id, int visible)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(10), "D"((long)(win_id)), "S"((long)(visible)) : "memory");
    return (int)ret;
}

static inline int Uni_Text(int win_id, int x, int y, const char *text)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(11), "D"((long)(win_id)), "S"((long)(x)), "d"((long)(y)), "c"((long)(text)) : "memory", "r10");
    return (int)ret;
}

static inline int Uni_Label(int win_id, int text_idx, const char *str)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(12), "D"((long)(win_id)), "S"((long)(text_idx)), "d"((long)(str)) : "memory");
    return (int)ret;
}

static inline int Uni_GetText(int win_id, int widget_idx, char *buf, int cap)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(13), "D"((long)(win_id)), "S"((long)(widget_idx)), "d"((long)(buf)), "c"((long)(cap)) : "memory", "r10");
    return (int)ret;
}

#define UNI_FLIST_MAX 32

struct UniStat
{
    char name[256];
    unsigned int size;
    unsigned char type;
};

static inline int Uni_FOpen(int fd, const char *path)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "D"((long)(fd)), "S"((long)(path)) : "memory");
    return (int)ret;
}

static inline int Uni_FClose(int fd)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(15), "D"((long)(fd)) : "memory");
    return (int)ret;
}

static inline int Uni_FRead(int fd, char *buf, int max)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(16), "D"((long)(fd)), "S"((long)(buf)), "d"((long)(max)) : "memory");
    return (int)ret;
}

static inline int Uni_FWrite(int fd, const char *buf, int len)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(17), "D"((long)(fd)), "S"((long)(buf)), "d"((long)(len)) : "memory");
    return (int)ret;
}

static inline int Uni_FList(const char *path, struct UniStat *out, int max)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(18), "D"((long)(path)), "S"((long)(out)), "d"((long)(max)) : "memory");
    return (int)ret;
}

static inline int Uni_SetText(int win_id, int widget_idx, const char *str)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(19), "D"((long)(win_id)), "S"((long)(widget_idx)), "d"((long)(str)) : "memory");
    return (int)ret;
}

#endif
