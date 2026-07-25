#ifndef UNI_ATA_H
#define UNI_ATA_H

#include <stdint.h>

int ata_init(void);
int ata_read(uint32_t lba, uint32_t count, void *buf);
int ata_write(uint32_t lba, uint32_t count, const void *buf);
uint32_t ata_sectors(void);
void ata_flush(void);

#endif
