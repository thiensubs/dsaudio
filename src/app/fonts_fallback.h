// fonts_fallback.h - embedded fallback subsets (Inter for Vietnamese, Noto
// Sans SC/JP/KR for CJK), chained after CUI's own faces.
#ifndef FONTS_FALLBACK_H
#define FONTS_FALLBACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DS_FB_VI = 0, DS_FB_ZH = 1, DS_FB_JA = 2, DS_FB_KO = 3, DS_FB_COUNT = 4 };

typedef struct ds_fb_font { const unsigned char *data; size_t len; } ds_fb_font;

extern const ds_fb_font kFallbackFonts[DS_FB_COUNT];

#ifdef __cplusplus
}
#endif

#endif
