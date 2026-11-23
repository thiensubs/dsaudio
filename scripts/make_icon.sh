#!/bin/bash
# Render AppIcon.icns: one 1024x1024 CoreGraphics render, downsampled to the
# ten sizes iconutil requires, then packed into an icns.
# Usage: bash scripts/make_icon.sh [out.icns]
set -e
cd "$(dirname "$0")/.."

OUT="${1:-build/AppIcon.icns}"
SET="$PWD/build/AppIcon.iconset"

mkdir -p "$SET" "$(dirname "$OUT")"

cc -O2 -o build/make_icon assets/make_icon.m \
   -framework CoreGraphics -framework CoreFoundation -framework ImageIO
./build/make_icon build/AppIcon_1024.png

for entry in \
  "icon_16x16.png 16" \
  "icon_16x16@2x.png 32" \
  "icon_32x32.png 32" \
  "icon_32x32@2x.png 64" \
  "icon_128x128.png 128" \
  "icon_128x128@2x.png 256" \
  "icon_256x256.png 256" \
  "icon_256x256@2x.png 512" \
  "icon_512x512.png 512" \
  "icon_512x512@2x.png 1024"
do
  name=${entry% *}
  size=${entry##* }
  sips -z "$size" "$size" build/AppIcon_1024.png --out "$SET/$name" >/dev/null
done

iconutil -c icns "$SET" -o "$OUT"
rm -rf "$SET" build/AppIcon_1024.png
echo "icon: $OUT"
