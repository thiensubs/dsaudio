// fonts_cjk.h - embedded CJK subsets (Noto Sans SC/JP/KR), loaded as fallbacks.
#ifndef FONTS_CJK_H
#define FONTS_CJK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DS_CJK_ZH = 0, DS_CJK_JA = 1, DS_CJK_KO = 2, DS_CJK_COUNT = 3 };

typedef struct ds_cjk_font { const unsigned char *data; size_t len; } ds_cjk_font;

extern const ds_cjk_font kCjkFonts[DS_CJK_COUNT];

#ifdef __cplusplus
}
#endif

#endif
