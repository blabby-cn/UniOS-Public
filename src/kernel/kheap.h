#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>

#define KHEAP_BASE 0xFFFFA00000000000ULL
#define KHEAP_INIT_PAGES 8
#define KHEAP_GROW_PAGES 4

void kheap_init(uint64_t base, uint32_t initial_pages);
void *kmalloc(uint32_t size);
void *krealloc(void *ptr, uint32_t size);
void kfree(void *ptr);
uint32_t kheap_total(void);
uint32_t kheap_used(void);
uint32_t kheap_free(void);
uint32_t kheap_allocs(void);

#endif
