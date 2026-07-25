#ifndef LOCALE_H
#define LOCALE_H

#define LANG_EN 0
#define LANG_ZH 1

void locale_init(void);
void locale_set_lang(int lang);
int locale_get_lang(void);
const char *locale_get(const char *key);

#endif