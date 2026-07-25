#ifndef UNI_USER_SYSCALL_H
#define UNI_USER_SYSCALL_H

static inline long sys_write(const char *buf, unsigned long len)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(1), "D"(buf), "S"(len) : "memory");
    return ret;
}

static inline long sys_read(char *buf)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(5), "D"(buf) : "memory");
    return ret;
}

static inline long sys_yield(void)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(2) : "memory");
    return ret;
}

static inline long sys_getpid(void)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(3) : "memory");
    return ret;
}

static inline void sys_exit(int code)
{
    __asm__ volatile("int $0x80" : : "a"(4), "D"((long)code) : "memory");
}

#endif
