#include "util.h"

void *memset(void *dst, int c, unsigned long n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--)
    {
        *p++ = (unsigned char)c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
    {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s)
    {
        while (n--)
        {
            *d++ = *s++;
        }
    }
    else
    {
        d += n;
        s += n;
        while (n--)
        {
            *--d = *--s;
        }
    }
    return dst;
}

unsigned long strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (unsigned long)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char *)a - *(unsigned char *)b;
}

int strncmp(const char *a, const char *b, unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n; i++)
    {
        if (a[i] != b[i]) return *(unsigned char *)(a + i) - *(unsigned char *)(b + i);
        if (!a[i]) return 0;
    }
    return 0;
}

char *strncpy(char *dst, const char *src, unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (char)c == 0 ? (char *)s : 0;
}

char *strstr(const char *haystack, const char *needle)
{
    unsigned long nl = strlen(needle);
    if (nl == 0) return (char *)haystack;
    while (*haystack)
    {
        if (strncmp(haystack, needle, nl) == 0) return (char *)haystack;
        haystack++;
    }
    return 0;
}

long strtol(const char *s, char **endptr, int base)
{
    int neg = 0;
    long v = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (base == 0)
    {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; }
        else base = 10;
    }
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s)
    {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -v : v;
}

long long strtoll(const char *s, char **endptr, int base)
{
    int neg = 0;
    long long v = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (base == 0)
    {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; }
        else base = 10;
    }
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s)
    {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -v : v;
}

static void qsort_swap(char *a, char *b, unsigned long sz)
{
    unsigned long i;
    for (i = 0; i < sz; i++) {
        char t = a[i]; a[i] = b[i]; b[i] = t;
    }
}

void qsort(void *base, unsigned long nmemb, unsigned long size, int (*compar)(const void *, const void *))
{
    if (nmemb <= 1) return;
    char *b = (char *)base;
    char *pivot = b + (nmemb - 1) * size;
    unsigned long i, pi = 0;
    for (i = 0; i < nmemb - 1; i++) {
        if (compar(b + i * size, pivot) < 0) {
            if (pi != i) qsort_swap(b + pi * size, b + i * size, size);
            pi++;
        }
    }
    qsort_swap(b + pi * size, pivot, size);
    qsort(base, pi, size, compar);
    qsort(b + (pi + 1) * size, nmemb - pi - 1, size, compar);
}
