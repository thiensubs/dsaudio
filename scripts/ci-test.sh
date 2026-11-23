#!/bin/sh
# The CI gate, shared by the GitHub and GitLab pipelines.
#
#   scripts/ci-test.sh            core tests, MCP selftest, MCP request path, CLI
#   scripts/ci-test.sh --with-gui  + the real window under Xvfb (Linux)
#
# Every step is offline: no key, no network calls.
set -eu

BUILD="${BUILD_DIR:-build}"
GUI="${1:-}"

# Resolve the binaries: MSVC puts them in build/Release and adds .exe.
pick() {
  # MSVC writes to build/Release and adds .exe; skip the project files.
  local c
  for pat in "$BUILD/Release/$1.exe" "$BUILD/$1.exe" "$BUILD/Release/$1" "$BUILD/$1"; do
    c=$(ls $pat 2>/dev/null | head -n 1)
    [ -n "$c" ] && { echo "$c"; return; }
  done
  ls "$BUILD"/$1* 2>/dev/null | rg -v '\.(vcxproj|sln|filters|user|txt|dir)$' | head -n 1
}
MCP="$(pick dsaudio-mcp)"
CLI="$(pick dsaudio-cli)"
APP="$(pick dsaudio | rg -v 'mcp|cli' || true)"
[ -n "$MCP" ] || { echo "dsaudio-mcp not built"; exit 1; }

echo "== ctest =="
ctest --test-dir "$BUILD" --output-on-failure -C Release

echo "== mcp selftest =="
"$MCP" --selftest | python3 -c \
  "import json,sys; d=json.load(sys.stdin); assert len(d['tools'])==4, d; \
print('tools:', ', '.join(t['name'] for t in d['tools']))"

echo "== mcp request path =="
printf '%s\n' \
  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
  '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"health","arguments":{}}}' \
  '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"nope","arguments":{}}}' \
  | "$MCP" > mcp-out.txt
python3 - <<'PY'
import json
lines = [json.loads(l) for l in open('mcp-out.txt') if l.strip()]
assert lines[0]['id'] == 1 and 'protocolVersion' in lines[0]['result'], lines[0]
assert lines[1]['id'] == 2 and lines[1]['result']['isError'] is False, lines[1]
assert lines[2]['id'] == 3 and lines[2]['error']['code'] == -32602, lines[2]
print('json-rpc request path ok')
PY

echo "== cli reports a missing key cleanly =="
set +e
TMPHOME=$(mktemp -d)
trap "rm -rf $TMPHOME" EXIT
OUT=$(env -u DEEPSEEK_API_KEY HOME="$TMPHOME" "$CLI" config 2>&1)
RC=$?
set -e
echo "$OUT"
[ "$RC" = 2 ] || { echo "expected rc 2 (no key), got $RC"; exit 1; }
echo "$OUT" | grep -qi "no key" || { echo "expected a no-key message"; exit 1; }

if [ "$GUI" = "--with-gui" ]; then
  echo "== app smoke test under Xvfb =="
  xvfb-run -a env DSAUDIO_QUIT_AFTER=6 "$APP"
fi

rm -f mcp-out.txt
echo "CI GATE PASSED"
