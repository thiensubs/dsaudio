#!/usr/bin/env python3
"""Regenerate src/app/fonts_fallback.c from the strings the app actually shows.

Two kinds of fallback are embedded, because CUI's own Inter is cut to Latin and
the Noto faces are cut to the exact codepoints ds_i18n.c uses:

  * Noto Sans SC / JP / KR, for the CJK languages;
  * Inter, for Vietnamese -- CUI 1.5 subsetted Inter from 1,045 glyphs to 376 and
    dropped the horn vowels (`ơ`, `ư`) and every precomposed Vietnamese tone
    mark, so the Vietnamese interface would render as boxes without this.

Re-run this after editing the string table:

    python3 -m pip install fonttools          # subsetting needs it
    python3 scripts/gen_fonts.py

Downloads the variable fonts from the google/fonts repository, so it needs
network access. The generated file is committed, so a normal build does not
need any of this.

Measured result (2026-11-26, CUI 1.5): vi 52,169 B, zh 164,016 B,
ja 223,684 B, ko 173,720 B.
"""
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "core" / "ds_i18n.c"
OUT_C = ROOT / "src" / "app" / "fonts_fallback.c"
OUT_H = ROOT / "src" / "app" / "fonts_fallback.h"

FONTS = {
    "vi": "https://github.com/google/fonts/raw/main/ofl/inter/Inter%5Bopsz%2Cwght%5D.ttf",
    "zh": "https://github.com/google/fonts/raw/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf",
    "ja": "https://github.com/google/fonts/raw/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf",
    "ko": "https://github.com/google/fonts/raw/main/ofl/notosanskr/NotoSansKR%5Bwght%5D.ttf",
}
# The Noto faces are instantiated at wght=400; Inter also carries opsz.
AXES = {"vi": "wght=400", "zh": "wght=400", "ja": "wght=400", "ko": "wght=400"}
NAMES = {"vi": "VI", "zh": "ZH", "ja": "JA", "ko": "KO"}


def codepoints():
    """The non-ASCII codepoints the string table uses, grouped by script."""
    text = SRC.read_text(encoding="utf-8")
    chars = set()
    for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', text):
        lit = m.group(1)
        chars.update(lit)
        for u in re.finditer(r"\\u([0-9a-fA-F]{4})", lit):
            chars.add(chr(int(u.group(1), 16)))

    def rng(a, b):
        return {chr(c) for c in range(a, b + 1)}

    han = {c for c in chars if 0x4E00 <= ord(c) <= 0x9FFF or 0x3400 <= ord(c) <= 0x4DBF}
    kana = {c for c in chars if 0x3040 <= ord(c) <= 0x30FF or 0x31F0 <= ord(c) <= 0x31FF}
    hangul = {c for c in chars if 0xAC00 <= ord(c) <= 0xD7A3 or 0x1100 <= ord(c) <= 0x11FF}
    base = rng(0x20, 0x7E) | set("—–''\"\"…•·、。‹›«»€©®™°±×÷→←↑↓")

    # Vietnamese: the whole alphabet, not just the chars ds_i18n.c happens to use,
    # because the editor shows user text too. Horn vowels live in Latin
    # Extended-B; the precomposed tones in Latin Extended Additional.
    vi = (base | rng(0xA0, 0xFF) | rng(0x100, 0x17F)
          | {"Ơ", "ơ", "Ư", "ư"} | rng(0x1EA0, 0x1EFF))

    return {"vi": vi, "zh": base | han, "ja": base | han | kana, "ko": base | han | hangul}


def download(url, dest):
    urllib.request.urlretrieve(url, dest)


def subset(tag, chars, work):
    """Instantiate the weight axis, then cut to `chars`. Returns the font bytes."""
    static = work / f"{tag}-static.ttf"
    subprocess.run(
        [sys.executable, "-m", "fontTools.varLib.instancer", str(work / f"{tag}.ttf"),
         AXES[tag], "-o", str(static)],
        check=True, capture_output=True)
    uni = ",".join(f"U+{ord(c):04X}" for c in sorted(chars))
    out = work / f"{tag}.ttf"
    subprocess.run(
        [sys.executable, "-m", "fontTools.subset", str(static), f"--unicodes={uni}",
         f"--output-file={out}", "--layout-features=*", "--hinting", "--desubroutinize"],
        check=True, capture_output=True)
    return out.read_bytes()


def emit(fonts):
    parts = [
        "// fonts_fallback.c - embedded fallback subsets, generated from the exact glyph\n"
        "// set the app needs. Regenerate with scripts/gen_fonts.py.\n",
        '#include "fonts_fallback.h"\n\n',
    ]
    decl = []
    for tag in ("vi", "zh", "ja", "ko"):
        data = fonts[tag]
        parts.append(f"static const unsigned char kFb{NAMES[tag]}[{len(data)}] = {{\n")
        row = []
        for b in data:
            row.append(str(b))
            if len(row) == 18:
                parts.append(",".join(row) + ",\n")
                row = []
        if row:
            parts.append(",".join(row) + ",\n")
        parts.append("};\n\n")
        decl.append(f"    {{ kFb{NAMES[tag]}, sizeof kFb{NAMES[tag]} }}")
    parts.append("const ds_fb_font kFallbackFonts[DS_FB_COUNT] = {\n" + ",\n".join(decl) + "\n};\n")
    OUT_C.write_text("".join(parts))

    OUT_H.write_text(
        "// fonts_fallback.h - embedded fallback subsets (Inter for Vietnamese, Noto\n"
        "// Sans SC/JP/KR for CJK), chained after CUI's own faces.\n"
        "#ifndef FONTS_FALLBACK_H\n#define FONTS_FALLBACK_H\n\n#include <stddef.h>\n\n"
        '#ifdef __cplusplus\nextern "C" {\n#endif\n\n'
        "enum { DS_FB_VI = 0, DS_FB_ZH = 1, DS_FB_JA = 2, DS_FB_KO = 3, DS_FB_COUNT = 4 };\n\n"
        "typedef struct ds_fb_font { const unsigned char *data; size_t len; } ds_fb_font;\n\n"
        "extern const ds_fb_font kFallbackFonts[DS_FB_COUNT];\n\n"
        '#ifdef __cplusplus\n}\n#endif\n\n#endif\n')


def main():
    chars = codepoints()
    fonts = {}
    with tempfile.TemporaryDirectory() as tmp:
        work = pathlib.Path(tmp)
        for tag, url in FONTS.items():
            src = work / f"{tag}.ttf"
            print(f"downloading {tag} ...", flush=True)
            download(url, src)
            fonts[tag] = subset(tag, chars[tag], work)
            print(f"  {tag}: {len(fonts[tag])} bytes", flush=True)
    emit(fonts)
    print(f"wrote {OUT_C} ({OUT_C.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
