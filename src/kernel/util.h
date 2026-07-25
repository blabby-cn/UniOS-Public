#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

void *memset(void *dst, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);
void *memmove(void *dst, const void *src, unsigned long n);
unsigned long strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned long n);
char *strncpy(char *dst, const char *src, unsigned long n);
char *strchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
long strtol(const char *s, char **endptr, int base);
long long strtoll(const char *s, char **endptr, int base);
void qsort(void *base, unsigned long nmemb, unsigned long size, int (*compar)(const void *, const void *));

#endif