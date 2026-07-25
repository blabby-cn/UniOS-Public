#include "ata.h"
#include "io.h"
#include "kprintf.h"

#define ATA_IO 0x1F0
#define ATA_CTRL 0x3F6

#define REG_DATA (ATA_IO + 0)
#define REG_ERR (ATA_IO + 1)
#define REG_COUNT (ATA_IO + 2)
#define REG_LBA0 (ATA_IO + 3)
#define REG_LBA1 (ATA_IO + 4)
#define REG_LBA2 (ATA_IO + 5)
#define REG_DRIVE (ATA_IO + 6)
#define REG_CMD (ATA_IO + 7)
#define REG_STATUS (ATA_IO + 7)

#define ST_ERR 0x01
#define ST_DRQ 0x08
#define ST_DF 0x20
#define ST_BSY 0x80

#define CMD_READ 0x20
#define CMD_WRITE 0x30
#define CMD_FLUSH 0xE7
#define CMD_IDENTIFY 0xEC

static uint32_t g_sectors;

static void delay400(void)
{
    inb(ATA_CTRL);
    inb(ATA_CTRL);
    inb(ATA_CTRL);
    inb(ATA_CTRL);
}

static int wait_bsy_clear(void)
{
    for (uint32_t i = 0; i < 1000000; i++)
    {
        uint8_t s = inb(REG_STATUS);
        if (!(s & ST_BSY))
            return 0;
    }
    return -1;
}

static int wait_drq(void)
{
    for (uint32_t i = 0; i < 1000000; i++)
    {
        uint8_t s = inb(REG_STATUS);
        if (s & (ST_ERR | ST_DF))
            return -1;
        if (!(s & ST_BSY) && (s & ST_DRQ))
            return 0;
    }
    return -1;
}

int ata_init(void)
{
    outb(ATA_CTRL, 0x02);
    outb(REG_DRIVE, 0xA0);
    delay400();
    if (wait_bsy_clear())
        return -1;
    outb(REG_COUNT, 0);
    outb(REG_LBA0, 0);
    outb(REG_LBA1, 0);
    outb(REG_LBA2, 0);
    outb(REG_CMD, CMD_IDENTIFY);
    delay400();
    if (inb(REG_STATUS) == 0)
        return -1;
    if (wait_bsy_clear())
        return -1;
    if (inb(REG_LBA1) != 0 || inb(REG_LBA2) != 0)
        return -1;
    if (wait_drq())
        return -1;
    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(REG_DATA);
    g_sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    return 0;
}

uint32_t ata_sectors(void)
{
    return g_sectors;
}

static int setup_lba(uint32_t lba, uint32_t count)
{
    if (wait_bsy_clear())
        return -1;
    outb(REG_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    delay400();
    outb(REG_COUNT, (uint8_t)count);
    outb(REG_LBA0, (uint8_t)lba);
    outb(REG_LBA1, (uint8_t)(lba >> 8));
    outb(REG_LBA2, (uint8_t)(lba >> 16));
    return 0;
}

int ata_read(uint32_t lba, uint32_t count, void *buf)
{
    uint16_t *p = (uint16_t *)buf;
    while (count)
    {
        uint32_t n = count > 256 ? 256 : count;
        if (setup_lba(lba, n & 0xFF))
            return -1;
        outb(REG_CMD, CMD_READ);
        for (uint32_t s = 0; s < n; s++)
        {
            if (wait_drq())
                return -1;
            for (int i = 0; i < 256; i++)
                *p++ = inw(REG_DATA);
        }
        lba += n;
        count -= n;
    }
    return 0;
}

int ata_write(uint32_t lba, uint32_t count, const void *buf)
{
    const uint16_t *p = (const uint16_t *)buf;
    while (count)
    {
        uint32_t n = count > 256 ? 256 : count;
        if (setup_lba(lba, n & 0xFF))
            return -1;
        outb(REG_CMD, CMD_WRITE);
        for (uint32_t s = 0; s < n; s++)
        {
            if (wait_drq())
                return -1;
            for (int i = 0; i < 256; i++)
                outw(REG_DATA, *p++);
        }
        outb(REG_CMD, CMD_FLUSH);
        if (wait_bsy_clear())
            return -1;
        lba += n;
        count -= n;
    }
    return 0;
}

void ata_flush(void)
{
    outb(REG_CMD, CMD_FLUSH);
    wait_bsy_clear();
}
