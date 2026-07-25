#ifndef UNI_UDLL_H
#define UNI_UDLL_H

#include <stdint.h>

struct UdllHandle;

struct UdllHandle *udll_load(const char *path);
void *udll_get_proc(struct UdllHandle *h, const char *name);
void udll_unload(struct UdllHandle *h);

#endif
