#include "fat32.h"
#include "ata.h"
#include "kprintf.h"

void *memset(void *dst, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);

#define SEC_SIZE 512
#define ATTR_VOLID 0x08
#define ATTR_DIR 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LFN 0x0F
#define EOC 0x0FFFFFF8
#define BAD_CLUSTER 0x0FFFFFF7

static uint32_t g_part_lba;
static uint32_t g_spc;
static uint32_t g_resv;
static uint32_t g_nfats;
static uint32_t g_fatsz;
static uint32_t g_root;
static uint32_t g_data_lba;
static uint32_t g_clusters;
static uint32_t g_next_free = 3;
static int g_mounted;

static uint8_t g_fat_buf[SEC_SIZE];
static uint32_t g_fat_sec = 0xFFFFFFFF;
static int g_fat_dirty;

static uint8_t g_sec_buf[SEC_SIZE];

static uint32_t rd16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t kstrlen(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static char upch(char c)
{
    if (c >= 'a' && c <= 'z')
        return (char)(c - 32);
    return c;
}

static int name_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (upch(*a) != upch(*b))
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int fat_flush(void)
{
    if (!g_fat_dirty || g_fat_sec == 0xFFFFFFFF)
        return 0;
    uint32_t lba = g_part_lba + g_resv + g_fat_sec;
    if (ata_write(lba, 1, g_fat_buf))
        return -1;
    if (g_nfats > 1 && ata_write(lba + g_fatsz, 1, g_fat_buf))
        return -1;
    g_fat_dirty = 0;
    return 0;
}

static int fat_load(uint32_t sec)
{
    if (g_fat_sec == sec)
        return 0;
    if (fat_flush())
        return -1;
    if (ata_read(g_part_lba + g_resv + sec, 1, g_fat_buf))
        return -1;
    g_fat_sec = sec;
    return 0;
}

static uint32_t fat_get(uint32_t cl)
{
    if (fat_load((cl * 4) / SEC_SIZE))
        return BAD_CLUSTER;
    return rd32(g_fat_buf + (cl * 4) % SEC_SIZE) & 0x0FFFFFFF;
}

static int fat_set(uint32_t cl, uint32_t val)
{
    if (fat_load((cl * 4) / SEC_SIZE))
        return -1;
    uint8_t *p = g_fat_buf + (cl * 4) % SEC_SIZE;
    uint32_t old = rd32(p);
    wr32(p, (old & 0xF0000000) | (val & 0x0FFFFFFF));
    g_fat_dirty = 1;
    return 0;
}

static uint32_t cluster_lba(uint32_t cl)
{
    return g_data_lba + (cl - 2) * g_spc;
}

static uint32_t fat_alloc(uint32_t prev)
{
    for (uint32_t pass = 0; pass < 2; pass++)
    {
        uint32_t start = pass == 0 ? g_next_free : 3;
        uint32_t end = g_clusters + 2;
        for (uint32_t cl = start; cl < end; cl++)
        {
            if (fat_get(cl) == 0)
            {
                if (fat_set(cl, EOC))
                    return 0;
                if (prev && fat_set(prev, cl))
                    return 0;
                g_next_free = cl + 1;
                return cl;
            }
        }
    }
    return 0;
}

static int cluster_zero(uint32_t cl)
{
    memset(g_sec_buf, 0, SEC_SIZE);
    for (uint32_t s = 0; s < g_spc; s++)
        if (ata_write(cluster_lba(cl) + s, 1, g_sec_buf))
            return -1;
    return 0;
}

int fat32_mount(void)
{
    if (ata_read(0, 1, g_sec_buf))
        return -1;
    if (g_sec_buf[510] != 0x55 || g_sec_buf[511] != 0xAA)
        return -1;
    g_part_lba = 0;
    for (int i = 0; i < 4; i++)
    {
        const uint8_t *pe = g_sec_buf + 446 + i * 16;
        uint8_t type = pe[4];
        if (type == 0x0B || type == 0x0C)
        {
            g_part_lba = rd32(pe + 8);
            break;
        }
    }
    if (!g_part_lba)
        return -1;
    if (ata_read(g_part_lba, 1, g_sec_buf))
        return -1;
    if (rd16(g_sec_buf + 0x0B) != SEC_SIZE)
        return -1;
    g_spc = g_sec_buf[0x0D];
    g_resv = rd16(g_sec_buf + 0x0E);
    g_nfats = g_sec_buf[0x10];
    g_fatsz = rd32(g_sec_buf + 0x24);
    g_root = rd32(g_sec_buf + 0x2C);
    uint32_t total = rd32(g_sec_buf + 0x20);
    g_data_lba = g_part_lba + g_resv + g_nfats * g_fatsz;
    g_clusters = (total - g_resv - g_nfats * g_fatsz) / g_spc;
    if (g_clusters < 65525)
        return -1;
    g_mounted = 1;
    return 0;
}

uint32_t fat32_cluster_size(void)
{
    return g_spc * SEC_SIZE;
}

uint32_t fat32_total_clusters(void)
{
    return g_clusters;
}

uint32_t fat32_free_clusters(void)
{
    uint32_t n = 0;
    for (uint32_t cl = 2; cl < g_clusters + 2; cl++)
        if (fat_get(cl) == 0)
            n++;
    return n;
}

static uint8_t sfn_checksum(const uint8_t *sn)
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + sn[i]);
    return sum;
}

static void sfn_to_name(const uint8_t *e, char *out)
{
    int n = 0;
    int lb = (e[12] & 0x08) != 0;
    int le = (e[12] & 0x10) != 0;
    for (int i = 0; i < 8 && e[i] != ' '; i++)
    {
        char c = (char)e[i];
        if (lb && c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        out[n++] = c;
    }
    if (e[8] != ' ')
    {
        out[n++] = '.';
        for (int i = 8; i < 11 && e[i] != ' '; i++)
        {
            char c = (char)e[i];
            if (le && c >= 'A' && c <= 'Z')
                c = (char)(c + 32);
            out[n++] = c;
        }
    }
    out[n] = 0;
}

struct dir_iter
{
    uint32_t cluster;
    uint32_t sec_in_cl;
    uint32_t off;
    char lfn[FAT32_NAME_MAX];
    int lfn_ready;
    uint8_t lfn_sum;
};

static int iter_next_raw(struct dir_iter *it, uint8_t **entry)
{
    if (it->off >= SEC_SIZE)
    {
        it->off = 0;
        it->sec_in_cl++;
        if (it->sec_in_cl >= g_spc)
        {
            it->sec_in_cl = 0;
            uint32_t nx = fat_get(it->cluster);
            if (nx >= EOC || nx == BAD_CLUSTER || nx < 2)
                return 0;
            it->cluster = nx;
        }
        if (ata_read(cluster_lba(it->cluster) + it->sec_in_cl, 1, g_sec_buf))
            return -1;
    }
    *entry = g_sec_buf + it->off;
    it->off += 32;
    return 1;
}

static int iter_begin(struct dir_iter *it, uint32_t cluster)
{
    it->cluster = cluster;
    it->sec_in_cl = 0;
    it->off = 0;
    it->lfn_ready = 0;
    return ata_read(cluster_lba(cluster), 1, g_sec_buf) ? -1 : 0;
}

static void lfn_collect(struct dir_iter *it, const uint8_t *e)
{
    uint8_t ord = e[0];
    uint8_t seq = ord & 0x1F;
    if (ord & 0x40)
    {
        memset(it->lfn, 0, FAT32_NAME_MAX);
        it->lfn_sum = e[13];
    }
    if (seq == 0 || seq > 5)
        return;
    static const int offs[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    uint32_t base = (uint32_t)(seq - 1) * 13;
    for (int i = 0; i < 13; i++)
    {
        if (base + (uint32_t)i >= FAT32_NAME_MAX - 1)
            break;
        uint32_t u = rd16(e + offs[i]);
        if (u == 0 || u == 0xFFFF)
        {
            it->lfn[base + i] = 0;
            continue;
        }
        it->lfn[base + i] = (u < 128) ? (char)u : '?';
    }
    it->lfn_ready = 1;
}

static int dir_find(uint32_t dir_cl, const char *name, struct fat32_dirent *out,
                    uint32_t *ent_cl, uint32_t *ent_sec, uint32_t *ent_off)
{
    struct dir_iter it;
    if (iter_begin(&it, dir_cl))
        return -1;
    uint8_t *e;
    int r;
    while ((r = iter_next_raw(&it, &e)) == 1)
    {
        if (e[0] == 0x00)
            return 0;
        if (e[0] == 0xE5)
        {
            it.lfn_ready = 0;
            continue;
        }
        if (e[11] == ATTR_LFN)
        {
            lfn_collect(&it, e);
            continue;
        }
        if (e[11] & ATTR_VOLID)
        {
            it.lfn_ready = 0;
            continue;
        }
        char nm[FAT32_NAME_MAX];
        if (it.lfn_ready && it.lfn_sum == sfn_checksum(e))
        {
            memcpy(nm, it.lfn, FAT32_NAME_MAX);
        }
        else
        {
            sfn_to_name(e, nm);
        }
        it.lfn_ready = 0;
        if (name_eq(nm, name))
        {
            if (out)
            {
                memcpy(out->name, nm, FAT32_NAME_MAX);
                out->size = rd32(e + 28);
                out->first_cluster = (rd16(e + 20) << 16) | rd16(e + 26);
                out->attr = e[11];
            }
            if (ent_cl)
            {
                *ent_cl = it.cluster;
                *ent_sec = it.sec_in_cl;
                *ent_off = it.off - 32;
            }
            return 1;
        }
    }
    return r < 0 ? -1 : 0;
}

static int path_walk(const char *path, uint32_t *dir_cl, char *leaf)
{
    if (!g_mounted || path[0] != '/')
        return -1;
    uint32_t cur = g_root;
    uint32_t i = 1;
    uint32_t len = kstrlen(path);
    while (i < len)
    {
        uint32_t j = i;
        while (j < len && path[j] != '/')
            j++;
        uint32_t seg = j - i;
        if (seg == 0 || seg >= FAT32_NAME_MAX)
            return -1;
        char nm[FAT32_NAME_MAX];
        memcpy(nm, path + i, seg);
        nm[seg] = 0;
        if (j >= len)
        {
            memcpy(leaf, nm, seg + 1);
            *dir_cl = cur;
            return 0;
        }
        struct fat32_dirent de;
        int r = dir_find(cur, nm, &de, 0, 0, 0);
        if (r != 1 || !(de.attr & ATTR_DIR))
            return -1;
        cur = de.first_cluster;
        i = j + 1;
    }
    return -1;
}

int fat32_stat(const char *path, struct fat32_dirent *out)
{
    if (path[0] == '/' && path[1] == 0)
    {
        memcpy(out->name, "/", 2);
        out->size = 0;
        out->first_cluster = g_root;
        out->attr = ATTR_DIR;
        return 0;
    }
    uint32_t dir_cl;
    char leaf[FAT32_NAME_MAX];
    if (path_walk(path, &dir_cl, leaf))
        return -1;
    return dir_find(dir_cl, leaf, out, 0, 0, 0) == 1 ? 0 : -1;
}

int fat32_list(const char *path, fat32_list_cb cb, void *ctx)
{
    struct fat32_dirent d;
    if (fat32_stat(path, &d) || !(d.attr & ATTR_DIR))
        return -1;
    uint32_t cl = d.first_cluster ? d.first_cluster : g_root;
    struct dir_iter it;
    if (iter_begin(&it, cl))
        return -1;
    uint8_t *e;
    int r;
    while ((r = iter_next_raw(&it, &e)) == 1)
    {
        if (e[0] == 0x00)
            return 0;
        if (e[0] == 0xE5)
        {
            it.lfn_ready = 0;
            continue;
        }
        if (e[11] == ATTR_LFN)
        {
            lfn_collect(&it, e);
            continue;
        }
        if (e[11] & ATTR_VOLID)
        {
            it.lfn_ready = 0;
            continue;
        }
        struct fat32_dirent de;
        if (it.lfn_ready && it.lfn_sum == sfn_checksum(e))
        {
            memcpy(de.name, it.lfn, FAT32_NAME_MAX);
        }
        else
        {
            sfn_to_name(e, de.name);
        }
        it.lfn_ready = 0;
        de.size = rd32(e + 28);
        de.first_cluster = (rd16(e + 20) << 16) | rd16(e + 26);
        de.attr = e[11];
        if (de.name[0] == '.' && (de.name[1] == 0 || (de.name[1] == '.' && de.name[2] == 0)))
            continue;
        cb(&de, ctx);
    }
    return r < 0 ? -1 : 0;
}

int64_t fat32_read(const char *path, void *buf, uint32_t maxlen)
{
    struct fat32_dirent d;
    if (fat32_stat(path, &d) || (d.attr & ATTR_DIR))
        return -1;
    uint32_t left = d.size < maxlen ? d.size : maxlen;
    uint32_t done = 0;
    uint32_t cl = d.first_cluster;
    uint8_t *out = (uint8_t *)buf;
    while (left && cl >= 2 && cl < EOC)
    {
        for (uint32_t s = 0; s < g_spc && left; s++)
        {
            if (ata_read(cluster_lba(cl) + s, 1, g_sec_buf))
                return -1;
            uint32_t n = left < SEC_SIZE ? left : SEC_SIZE;
            memcpy(out + done, g_sec_buf, n);
            done += n;
            left -= n;
        }
        cl = fat_get(cl);
        if (cl == BAD_CLUSTER)
            return -1;
    }
    return (int64_t)done;
}

static int make_sfn(const char *name, uint8_t *sfn, int *needs_lfn)
{
    memset(sfn, ' ', 11);
    uint32_t len = kstrlen(name);
    uint32_t dot = len;
    for (uint32_t i = len; i > 0; i--)
    {
        if (name[i - 1] == '.')
        {
            dot = i - 1;
            break;
        }
    }
    uint32_t base_len = dot;
    uint32_t ext_len = dot < len ? len - dot - 1 : 0;
    *needs_lfn = 0;
    if (base_len > 8 || ext_len > 3)
        *needs_lfn = 1;
    for (uint32_t i = 0; i < len; i++)
    {
        char c = name[i];
        if (c >= 'a' && c <= 'z')
            *needs_lfn = 1;
    }
    uint32_t bl = base_len > 8 ? 6 : base_len;
    for (uint32_t i = 0; i < bl; i++)
    {
        char c = upch(name[i]);
        if (c == ' ' || c == '.')
            c = '_';
        sfn[i] = (uint8_t)c;
    }
    if (base_len > 8)
    {
        sfn[6] = '~';
        sfn[7] = '1';
    }
    uint32_t el = ext_len > 3 ? 3 : ext_len;
    for (uint32_t i = 0; i < el; i++)
        sfn[8 + i] = (uint8_t)upch(name[dot + 1 + i]);
    return 0;
}

static int dir_alloc_slots(uint32_t dir_cl, uint32_t nslots,
                           uint32_t *out_cl, uint32_t *out_sec, uint32_t *out_off)
{
    uint32_t cl = dir_cl;
    for (;;)
    {
        for (uint32_t s = 0; s < g_spc; s++)
        {
            if (ata_read(cluster_lba(cl) + s, 1, g_sec_buf))
                return -1;
            uint32_t run = 0;
            for (uint32_t off = 0; off + 32 <= SEC_SIZE; off += 32)
            {
                uint8_t m = g_sec_buf[off];
                if (m == 0x00 || m == 0xE5)
                {
                    if (run == 0)
                    {
                        *out_cl = cl;
                        *out_sec = s;
                        *out_off = off;
                    }
                    run++;
                    if (run >= nslots)
                        return 0;
                }
                else
                {
                    run = 0;
                }
            }
        }
        uint32_t nx = fat_get(cl);
        if (nx >= EOC || nx < 2)
        {
            uint32_t ncl = fat_alloc(cl);
            if (!ncl)
                return -1;
            if (cluster_zero(ncl))
                return -1;
            nx = ncl;
        }
        cl = nx;
    }
}

static int write_dirent(uint32_t dir_cl, const char *name, uint32_t first_cl,
                        uint32_t size, uint8_t attr)
{
    uint8_t sfn[11];
    int needs_lfn;
    make_sfn(name, sfn, &needs_lfn);
    uint32_t nlen = kstrlen(name);
    uint32_t nlfn = needs_lfn ? (nlen + 12) / 13 : 0;
    uint32_t slots = nlfn + 1;
    uint32_t cl, sec, off;
    if (dir_alloc_slots(dir_cl, slots, &cl, &sec, &off))
        return -1;
    if (ata_read(cluster_lba(cl) + sec, 1, g_sec_buf))
        return -1;
    uint8_t sum = sfn_checksum(sfn);
    static const int offs[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    uint32_t cur_sec = sec;
    for (uint32_t k = 0; k < slots; k++)
    {
        uint32_t o = off + k * 32;
        if (o >= SEC_SIZE)
        {
            if (ata_write(cluster_lba(cl) + cur_sec, 1, g_sec_buf))
                return -1;
            cur_sec++;
            off -= SEC_SIZE;
            o = off + k * 32;
            if (ata_read(cluster_lba(cl) + cur_sec, 1, g_sec_buf))
                return -1;
        }
        uint8_t *e = g_sec_buf + o;
        memset(e, 0, 32);
        if (k < nlfn)
        {
            uint32_t seq = nlfn - k;
            e[0] = (uint8_t)(seq | (k == 0 ? 0x40 : 0));
            e[11] = ATTR_LFN;
            e[13] = sum;
            uint32_t base = (seq - 1) * 13;
            for (int i = 0; i < 13; i++)
            {
                uint32_t idx = base + (uint32_t)i;
                uint32_t u;
                if (idx < nlen)
                    u = (uint8_t)name[idx];
                else if (idx == nlen)
                    u = 0;
                else
                    u = 0xFFFF;
                wr16(e + offs[i], u);
            }
        }
        else
        {
            memcpy(e, sfn, 11);
            e[11] = attr;
            wr16(e + 20, first_cl >> 16);
            wr16(e + 26, first_cl & 0xFFFF);
            wr32(e + 28, size);
        }
    }
    if (ata_write(cluster_lba(cl) + cur_sec, 1, g_sec_buf))
        return -1;
    return fat_flush();
}

static int update_dirent(uint32_t cl, uint32_t sec, uint32_t off,
                         uint32_t first_cl, uint32_t size)
{
    if (ata_read(cluster_lba(cl) + sec, 1, g_sec_buf))
        return -1;
    uint8_t *e = g_sec_buf + off;
    wr16(e + 20, first_cl >> 16);
    wr16(e + 26, first_cl & 0xFFFF);
    wr32(e + 28, size);
    return ata_write(cluster_lba(cl) + sec, 1, g_sec_buf);
}

static int chain_free(uint32_t cl)
{
    while (cl >= 2 && cl < EOC)
    {
        uint32_t nx = fat_get(cl);
        if (nx == BAD_CLUSTER)
            return -1;
        if (fat_set(cl, 0))
            return -1;
        if (cl < g_next_free)
            g_next_free = cl;
        cl = nx;
    }
    return 0;
}

static int write_data(const void *data, uint32_t len, uint32_t *first_out)
{
    *first_out = 0;
    if (len == 0)
        return 0;
    const uint8_t *src = (const uint8_t *)data;
    uint32_t left = len;
    uint32_t prev = 0;
    while (left)
    {
        uint32_t cl = fat_alloc(prev);
        if (!cl)
            return -1;
        if (!*first_out)
            *first_out = cl;
        for (uint32_t s = 0; s < g_spc; s++)
        {
            uint32_t n = left < SEC_SIZE ? left : SEC_SIZE;
            memset(g_sec_buf, 0, SEC_SIZE);
            if (n)
                memcpy(g_sec_buf, src, n);
            if (ata_write(cluster_lba(cl) + s, 1, g_sec_buf))
                return -1;
            src += n;
            left -= n;
            if (!left)
                break;
        }
        prev = cl;
    }
    return fat_flush();
}

int fat32_write(const char *path, const void *data, uint32_t len)
{
    uint32_t dir_cl;
    char leaf[FAT32_NAME_MAX];
    if (path_walk(path, &dir_cl, leaf))
        return -1;
    struct fat32_dirent old;
    uint32_t ecl, esec, eoff;
    int r = dir_find(dir_cl, leaf, &old, &ecl, &esec, &eoff);
    if (r < 0)
        return -1;
    uint32_t first;
    if (write_data(data, len, &first))
        return -1;
    if (r == 1)
    {
        if (old.first_cluster >= 2 && chain_free(old.first_cluster))
            return -1;
        if (fat_flush())
            return -1;
        return update_dirent(ecl, esec, eoff, first, len);
    }
    return write_dirent(dir_cl, leaf, first, len, ATTR_ARCHIVE);
}

int fat32_mkdir(const char *path)
{
    uint32_t dir_cl;
    char leaf[FAT32_NAME_MAX];
    if (path_walk(path, &dir_cl, leaf))
        return -1;
    if (dir_find(dir_cl, leaf, 0, 0, 0, 0) == 1)
        return -1;
    uint32_t cl = fat_alloc(0);
    if (!cl)
        return -1;
    if (cluster_zero(cl))
        return -1;
    if (ata_read(cluster_lba(cl), 1, g_sec_buf))
        return -1;
    uint8_t *e = g_sec_buf;
    memset(e, ' ', 11);
    e[0] = '.';
    e[11] = ATTR_DIR;
    wr16(e + 20, cl >> 16);
    wr16(e + 26, cl & 0xFFFF);
    e = g_sec_buf + 32;
    memset(e, ' ', 11);
    e[0] = '.';
    e[1] = '.';
    e[11] = ATTR_DIR;
    uint32_t parent = dir_cl == g_root ? 0 : dir_cl;
    wr16(e + 20, parent >> 16);
    wr16(e + 26, parent & 0xFFFF);
    if (ata_write(cluster_lba(cl), 1, g_sec_buf))
        return -1;
    if (fat_flush())
        return -1;
    return write_dirent(dir_cl, leaf, cl, 0, ATTR_DIR);
}
