#include "svga.h"
#include "pci.h"
#include "io.h"

#define SVGA_VENDOR 0x15AD
#define SVGA_DEVICE 0x0405

#define SVGA_INDEX 0
#define SVGA_VALUE 1

#define SVGA_REG_ID 0
#define SVGA_REG_ENABLE 1
#define SVGA_REG_WIDTH 2
#define SVGA_REG_HEIGHT 3
#define SVGA_REG_MAX_WIDTH 4
#define SVGA_REG_MAX_HEIGHT 5
#define SVGA_REG_BITS_PER_PIXEL 7
#define SVGA_REG_BYTES_PER_LINE 12
#define SVGA_REG_FB_START 13
#define SVGA_REG_FB_OFFSET 14
#define SVGA_REG_VRAM_SIZE 15
#define SVGA_REG_FB_SIZE 16
#define SVGA_REG_CAPABILITIES 17
#define SVGA_REG_MEM_START 18
#define SVGA_REG_MEM_SIZE 19
#define SVGA_REG_CONFIG_DONE 20
#define SVGA_REG_SYNC 21
#define SVGA_REG_BUSY 22

#define SVGA_FIFO_MIN 0
#define SVGA_FIFO_MAX 1
#define SVGA_FIFO_NEXT_CMD 2
#define SVGA_FIFO_STOP 3

#define SVGA_CMD_UPDATE 1

#define SVGA_MAGIC 0x900000
#define SVGA_ID_2 ((SVGA_MAGIC << 8) | 2)
#define SVGA_ID_1 ((SVGA_MAGIC << 8) | 1)
#define SVGA_ID_0 ((SVGA_MAGIC << 8) | 0)

static uint16_t g_io;
static int g_ready;
static volatile uint32_t *g_fifo;
static uint32_t g_fifo_size;
static int g_fifo_ready;
static uint32_t g_caps;

static void svga_write(uint32_t index, uint32_t value)
{
    outl((uint16_t)(g_io + SVGA_INDEX), index);
    outl((uint16_t)(g_io + SVGA_VALUE), value);
}

static uint32_t svga_read(uint32_t index)
{
    outl((uint16_t)(g_io + SVGA_INDEX), index);
    return inl((uint16_t)(g_io + SVGA_VALUE));
}

int svga_init(struct svga_info *out)
{
    struct pci_dev d;
    if (!pci_find(SVGA_VENDOR, SVGA_DEVICE, &d))
        return -1;
    if (!pci_bar_is_io(d.bar[0]))
        return -2;

    g_io = (uint16_t)pci_bar_base(d.bar[0]);

    uint32_t id = SVGA_ID_2;
    svga_write(SVGA_REG_ID, id);
    if (svga_read(SVGA_REG_ID) != id)
    {
        id = SVGA_ID_1;
        svga_write(SVGA_REG_ID, id);
        if (svga_read(SVGA_REG_ID) != id)
        {
            id = SVGA_ID_0;
            svga_write(SVGA_REG_ID, id);
            if (svga_read(SVGA_REG_ID) != id)
                return -3;
        }
    }

    uint64_t fb_phys = svga_read(SVGA_REG_FB_START);
    if (fb_phys == 0)
        fb_phys = pci_bar_base(d.bar[1]);

    g_ready = 1;

    if (out)
    {
        out->io_base = g_io;
        out->fb_phys = fb_phys;
        out->vram_size = svga_read(SVGA_REG_VRAM_SIZE);
        out->fb_size = svga_read(SVGA_REG_FB_SIZE);
        out->max_width = svga_read(SVGA_REG_MAX_WIDTH);
        out->max_height = svga_read(SVGA_REG_MAX_HEIGHT);
        out->id = id;
    }
    return 0;
}

int svga_set_mode(uint32_t width, uint32_t height, uint32_t bpp, struct svga_mode *out)
{
    if (!g_ready)
        return -1;

    svga_write(SVGA_REG_ENABLE, 0);
    svga_write(SVGA_REG_WIDTH, width);
    svga_write(SVGA_REG_HEIGHT, height);
    svga_write(SVGA_REG_BITS_PER_PIXEL, bpp);
    svga_write(SVGA_REG_ENABLE, 1);

    uint32_t pitch = svga_read(SVGA_REG_BYTES_PER_LINE);
    uint64_t base = svga_read(SVGA_REG_FB_START);
    uint32_t offset = svga_read(SVGA_REG_FB_OFFSET);

    if (out)
    {
        out->base = base + offset;
        out->width = width;
        out->height = height;
        out->pitch = pitch;
        out->bpp = bpp;
    }
    return 0;
}

int svga_fifo_init(void)
{
    if (!g_ready)
        return -1;

    g_caps = svga_read(SVGA_REG_CAPABILITIES);
    uint32_t mem_start = svga_read(SVGA_REG_MEM_START);
    uint32_t mem_size = svga_read(SVGA_REG_MEM_SIZE);
    if (mem_start == 0 || mem_size < 0x10000)
        return -2;

    g_fifo = (volatile uint32_t *)(unsigned long)mem_start;
    g_fifo_size = mem_size;

    g_fifo[SVGA_FIFO_MIN] = 16;
    g_fifo[SVGA_FIFO_MAX] = mem_size;
    g_fifo[SVGA_FIFO_NEXT_CMD] = 16;
    g_fifo[SVGA_FIFO_STOP] = 16;

    svga_write(SVGA_REG_CONFIG_DONE, 1);
    g_fifo_ready = 1;
    return 0;
}

int svga_fifo_active(void)
{
    return g_fifo_ready;
}

uint32_t svga_caps(void)
{
    return g_caps;
}

static void fifo_push(uint32_t v)
{
    uint32_t next = g_fifo[SVGA_FIFO_NEXT_CMD];
    uint32_t max = g_fifo[SVGA_FIFO_MAX];
    uint32_t min = g_fifo[SVGA_FIFO_MIN];

    uint32_t after = next + 4;
    if (after >= max)
        after = min;

    for (uint32_t spin = 0; spin < 1000000; spin++)
    {
        if (after != g_fifo[SVGA_FIFO_STOP])
            break;
        svga_write(SVGA_REG_SYNC, 1);
        while (svga_read(SVGA_REG_BUSY))
            ;
    }

    g_fifo[next / 4] = v;
    g_fifo[SVGA_FIFO_NEXT_CMD] = after;
}

void svga_update(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!g_fifo_ready)
        return;
    fifo_push(SVGA_CMD_UPDATE);
    fifo_push(x);
    fifo_push(y);
    fifo_push(w);
    fifo_push(h);
}

void svga_sync(void)
{
    if (!g_fifo_ready)
        return;
    svga_write(SVGA_REG_SYNC, 1);
    uint32_t spin = 0;
    while (svga_read(SVGA_REG_BUSY) && spin < 10000000)
        spin++;
}
