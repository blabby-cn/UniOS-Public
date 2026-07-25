#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define VMM_FLAG_PRESENT 0x1
#define VMM_FLAG_RW 0x2
#define VMM_FLAG_USER 0x4
#define VMM_FLAG_NX 0x8000000000000000ULL

void vmm_init(void);
uint64_t vmm_pml4(void);
void vmm_map(uint64_t vaddr, uint64_t paddr, uint64_t flags);
void vmm_unmap(uint64_t vaddr);
uint64_t vmm_pte(uint64_t vaddr);

#endif
