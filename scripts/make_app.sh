#!/bin/bash
# Assemble build/dsaudio.app: bundle layout, resources, version metadata,
# rendered icon, ad-hoc signature.
set -e
cd "$(dirname "$0")/.."

VERSION="$(tr -d '[:space:]' < VERSION.txt)"
BUILD_NUMBER="$(tr -d '[:space:]' < BUILD_NUMBER.txt)"
APP="build/dsaudio.app"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cp build/dsaudio "$APP/Contents/MacOS/dsaudio"
[ -f Resources/config.json ] && cp Resources/config.json "$APP/Contents/Resources/config.json"

sed -e "s/@VERSION@/$VERSION/" -e "s/@BUILD@/$BUILD_NUMBER/" \
    assets/Info.plist > "$APP/Contents/Info.plist"

bash scripts/make_icon.sh build/AppIcon.icns
cp build/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"

codesign --force --deep -s - "$APP"
echo "assembled: $APP (version $VERSION, build $BUILD_NUMBER)"
