#include "locale.h"
#include "util.h"
#include "kprintf.h"

#define MAX_KEYS 64
#define MAX_KEY_LEN 48
#define MAX_VAL_LEN 128

typedef struct {
    char key[MAX_KEY_LEN];
    char val[MAX_VAL_LEN];
} lc_entry;

static lc_entry g_en[MAX_KEYS];
static int g_en_count;
static lc_entry g_zh[MAX_KEYS];
static int g_zh_count;
static int g_lang;

extern const char lc_data_en[];
extern const char lc_data_en_end[];
extern const char lc_data_zh[];
extern const char lc_data_zh_end[];

static void parse_kv(const char *data, const char *end, lc_entry *entries, int *count)
{
    const char *p = data;
    *count = 0;
    while (p < end && *count < MAX_KEYS)
    {
        while (p < end && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        if (*p == '#') { while (p < end && *p != '\n') p++; continue; }
        const char *keq = 0;
        const char *ks = p;
        while (p < end && *p != '=' && *p != '\n' && *p != '\r') p++;
        keq = p;
        if (p >= end || *p != '=') { while (p < end && *p != '\n') p++; continue; }
        p++;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        const char *vs = p;
        while (p < end && *p != '\n' && *p != '\r') p++;
        int kl = (int)(keq - ks);
        int vl = (int)(p - vs);
        while (kl > 0 && (ks[kl-1] == ' ' || ks[kl-1] == '\t')) kl--;
        while (vl > 0 && (vs[vl-1] == ' ' || vs[vl-1] == '\t')) vl--;
        if (kl <= 0 || kl >= MAX_KEY_LEN) continue;
        if (vl <= 0 || vl >= MAX_VAL_LEN) continue;
        int i;
        for (i = 0; i < kl; i++) entries[*count].key[i] = ks[i];
        entries[*count].key[kl] = 0;
        for (i = 0; i < vl; i++) entries[*count].val[i] = vs[i];
        entries[*count].val[vl] = 0;
        (*count)++;
    }
}

void locale_init(void)
{
    parse_kv(lc_data_en, lc_data_en_end, g_en, &g_en_count);
    parse_kv(lc_data_zh, lc_data_zh_end, g_zh, &g_zh_count);
    g_lang = LANG_EN;
    kprintf("locale: en=%d zh=%d entries\n", g_en_count, g_zh_count);
}

void locale_set_lang(int lang)
{
    if (lang == LANG_EN || lang == LANG_ZH) g_lang = lang;
}

int locale_get_lang(void) { return g_lang; }

const char *locale_get(const char *key)
{
    lc_entry *e = (g_lang == LANG_ZH) ? g_zh : g_en;
    int n = (g_lang == LANG_ZH) ? g_zh_count : g_en_count;
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(e[i].key, key) == 0)
            return e[i].val;
    return key;
}
