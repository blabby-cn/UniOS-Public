#include "pci.h"
#include "io.h"

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

static uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    return (uint32_t)((1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                      ((uint32_t)func << 8) | (off & 0xFC));
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    outl(PCI_ADDR, pci_addr(bus, slot, func, off));
    return inl(PCI_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    uint32_t v = pci_read32(bus, slot, func, off);
    return (uint16_t)((v >> ((off & 2) * 8)) & 0xFFFF);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val)
{
    outl(PCI_ADDR, pci_addr(bus, slot, func, off));
    outl(PCI_DATA, val);
}

static int pci_probe(uint8_t bus, uint8_t slot, uint8_t func, struct pci_dev *out)
{
    uint32_t id = pci_read32(bus, slot, func, 0x00);
    uint16_t vendor = (uint16_t)(id & 0xFFFF);
    if (vendor == 0xFFFF)
        return 0;
    out->bus = bus;
    out->slot = slot;
    out->func = func;
    out->vendor = vendor;
    out->device = (uint16_t)(id >> 16);
    uint32_t cls = pci_read32(bus, slot, func, 0x08);
    out->class_code = (uint8_t)(cls >> 24);
    out->subclass = (uint8_t)(cls >> 16);
    for (uint8_t i = 0; i < 6; i++)
        out->bar[i] = pci_read32(bus, slot, func, (uint8_t)(0x10 + i * 4));
    return 1;
}

int pci_find(uint16_t vendor, uint16_t device, struct pci_dev *out)
{
    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t slot = 0; slot < 32; slot++)
        {
            for (uint8_t func = 0; func < 8; func++)
            {
                struct pci_dev d;
                if (!pci_probe((uint8_t)bus, slot, func, &d))
                    continue;
                if (d.vendor == vendor && d.device == device)
                {
                    *out = d;
                    return 1;
                }
                if (func == 0)
                {
                    uint32_t hdr = pci_read32((uint8_t)bus, slot, 0, 0x0C);
                    if (((hdr >> 16) & 0x80) == 0)
                        break;
                }
            }
        }
    }
    return 0;
}

uint32_t pci_bar_base(uint32_t bar)
{
    if (bar & 1)
        return bar & 0xFFFFFFFCu;
    return bar & 0xFFFFFFF0u;
}

int pci_bar_is_io(uint32_t bar)
{
    return (int)(bar & 1);
}
