// ds_i18n.h - UI string table: one key, eight languages, English fallback.
#ifndef DS_I18N_H
#define DS_I18N_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DS_LANG_EN = 0, DS_LANG_VI, DS_LANG_ZH, DS_LANG_JA,
    DS_LANG_KO, DS_LANG_ES, DS_LANG_FR, DS_LANG_DE,
    DS_LANG_COUNT
} ds_lang;

const char *ds_language_code(int index);   /* "en","vi","zh","ja","ko","es","fr","de"; NULL if out of range */
int ds_language_index(const char *code);   /* 0..DS_LANG_COUNT-1, or -1 if unknown */
int ds_language_detect(void);              /* from the machine locale, one of the enum values */
void ds_i18n_set(int lang);
int  ds_i18n_get(void);
const char *ds_i18n(const char *key);      /* current language's string; English fallback; NULL for unknown key */

#ifdef __cplusplus
}
#endif

#endif
