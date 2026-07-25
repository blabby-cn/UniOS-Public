#ifndef UNI_PMM_H
#define UNI_PMM_H

#include <stdint.h>

#define PMM_PAGE_SIZE 4096

void pmm_init(uint32_t info_addr);
void *pmm_alloc(void);
void pmm_free(void *p);
void pmm_stats(uint64_t *total, uint64_t *used, uint64_t *free);

#endif
