#ifndef UNI_PCI_H
#define UNI_PCI_H

#include <stdint.h>

struct pci_dev
{
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor;
    uint16_t device;
    uint8_t class_code;
    uint8_t subclass;
    uint32_t bar[6];
};

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val);

int pci_find(uint16_t vendor, uint16_t device, struct pci_dev *out);
uint32_t pci_bar_base(uint32_t bar);
int pci_bar_is_io(uint32_t bar);

#endif
