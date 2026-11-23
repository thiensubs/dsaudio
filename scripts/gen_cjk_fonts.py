#!/usr/bin/env python3
"""Regenerate src/app/fonts_cjk.c from the strings the app actually shows.

The embedded CJK fonts are subsets of Noto Sans SC / JP / KR, cut to the exact
codepoints used by src/core/ds_i18n.c. That keeps them at ~560 KB total instead
of the megabytes a full CJK font costs.

Re-run this after editing the string table:

    python3 -m pip install fonttools          # subsetting needs it
    python3 scripts/gen_cjk_fonts.py

Downloads the three variable fonts from the google/fonts repository, so it
needs network access. The generated file is committed, so a normal build does
not need any of this.

Measured result (2026-11-23): zh 164,016 B, ja 223,684 B, ko 173,720 B;
app binary 2,508,584 B, up from 1,980,416 B.
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
OUT_C = ROOT / "src" / "app" / "fonts_cjk.c"
OUT_H = ROOT / "src" / "app" / "fonts_cjk.h"

FONTS = {
    "zh": "https://github.com/google/fonts/raw/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf",
    "ja": "https://github.com/google/fonts/raw/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf",
    "ko": "https://github.com/google/fonts/raw/main/ofl/notosanskr/NotoSansKR%5Bwght%5D.ttf",
}
NAMES = {"zh": "ZH", "ja": "JA", "ko": "KO"}


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
    return {"zh": base | han, "ja": base | han | kana, "ko": base | han | hangul}


def download(url, dest):
    urllib.request.urlretrieve(url, dest)


def subset(tag, chars, work):
    """Instantiate wght=400, then cut to `chars`. Returns the font bytes."""
    static = work / f"noto-{tag}-static.ttf"
    subprocess.run(
        [sys.executable, "-m", "fontTools.varLib.instancer", str(work / f"Noto{tag}.ttf"),
         "wght=400", "-o", str(static)],
        check=True, capture_output=True)
    uni = ",".join(f"U+{ord(c):04X}" for c in sorted(chars))
    out = work / f"noto-{tag}.ttf"
    subprocess.run(
        [sys.executable, "-m", "fontTools.subset", str(static), f"--unicodes={uni}",
         f"--output-file={out}", "--layout-features=*", "--hinting", "--desubroutinize"],
        check=True, capture_output=True)
    return out.read_bytes()


def emit(fonts):
    parts = [
        "// fonts_cjk.c - Noto Sans SC/JP/KR subsets, generated from the exact glyph set\n"
        "// used by src/core/ds_i18n.c. Regenerate with scripts/gen_cjk_fonts.py.\n",
        '#include "fonts_cjk.h"\n\n',
    ]
    decl = []
    for tag in ("zh", "ja", "ko"):
        data = fonts[tag]
        parts.append(f"static const unsigned char kCjk{NAMES[tag]}[{len(data)}] = {{\n")
        row = []
        for b in data:
            row.append(str(b))
            if len(row) == 18:
                parts.append(",".join(row) + ",\n")
                row = []
        if row:
            parts.append(",".join(row) + ",\n")
        parts.append("};\n\n")
        decl.append(f"    {{ kCjk{NAMES[tag]}, sizeof kCjk{NAMES[tag]} }}")
    parts.append("const ds_cjk_font kCjkFonts[DS_CJK_COUNT] = {\n" + ",\n".join(decl) + "\n};\n")
    OUT_C.write_text("".join(parts))

    OUT_H.write_text(
        "// fonts_cjk.h - embedded CJK subsets (Noto Sans SC/JP/KR), loaded as fallbacks.\n"
        "#ifndef FONTS_CJK_H\n#define FONTS_CJK_H\n\n#include <stddef.h>\n\n"
        '#ifdef __cplusplus\nextern "C" {\n#endif\n\n'
        "enum { DS_CJK_ZH = 0, DS_CJK_JA = 1, DS_CJK_KO = 2, DS_CJK_COUNT = 3 };\n\n"
        "typedef struct ds_cjk_font { const unsigned char *data; size_t len; } ds_cjk_font;\n\n"
        "extern const ds_cjk_font kCjkFonts[DS_CJK_COUNT];\n\n"
        '#ifdef __cplusplus\n}\n#endif\n\n#endif\n')


def main():
    chars = codepoints()
    fonts = {}
    with tempfile.TemporaryDirectory() as tmp:
        work = pathlib.Path(tmp)
        for tag, url in FONTS.items():
            src = work / f"Noto{tag}.ttf"
            print(f"downloading {tag} ...", flush=True)
            download(url, src)
            fonts[tag] = subset(tag, chars[tag], work)
            print(f"  {tag}: {len(fonts[tag])} bytes", flush=True)
    emit(fonts)
    print(f"wrote {OUT_C} ({OUT_C.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
