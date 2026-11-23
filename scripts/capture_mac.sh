#!/bin/bash
# Launch dsaudio and capture its real window with macshot.
# Usage: bash scripts/capture_mac.sh [outfile]
set -e
cd "$(dirname "$0")/.."

MACSHOT="${MACSHOT:-$(command -v macshot_shot || echo "$HOME/.kimi-code/plugins/managed/macshot/bin/macshot_shot")}"
OUT="${1:-/tmp/dsaudio_mac.png}"

if [ ! -x build/dsaudio ]; then
  echo "build/dsaudio missing - run: cmake --build build -j" >&2
  exit 1
fi

./build/dsaudio >/tmp/dsaudio_run.log 2>&1 &
APP=$!
trap 'kill "$APP" 2>/dev/null' EXIT

for _ in $(seq 1 80); do
  "$MACSHOT" list 2>/dev/null | grep -q "dsaudio" && break
  sleep 0.25
done
sleep "${DSAUDIO_CAPTURE_DELAY:-2}"

"$MACSHOT" shot --owner dsaudio --out "$OUT"
echo "captured: $OUT"
