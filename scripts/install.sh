#!/bin/sh
# One-click install for the dsaudio MCP server.
#
#   curl -fsSL https://github.com/thiensubs/dsaudio/releases/latest/download/install.sh | sh
#
# Picks the right asset for this machine, checks its SHA-256, and puts the
# binary on PATH. Then prints the MCP client snippet.
set -eu

REPO="${DSAUDIO_REPO:-thiensubs/dsaudio}"
PREFIX="${DSAUDIO_PREFIX:-}"
TMP="${TMPDIR:-/tmp}/dsaudio-install.$$"

die() { printf 'install: %s\n' "$1" >&2; exit 1; }

OS="$(uname -s)"
ARCH="$(uname -m)"
case "$OS" in
    Darwin)  OS_TAG="darwin" ;;
    Linux)   OS_TAG="linux" ;;
    *)       die "unsupported OS: $OS (windows: use install.ps1)" ;;
esac
case "$ARCH" in
    x86_64|amd64) ARCH_TAG="x86_64" ;;
    arm64|aarch64) ARCH_TAG="arm64" ;;
    *)            die "unsupported architecture: $ARCH" ;;
esac

ASSET="dsaudio-mcp-${OS_TAG}-${ARCH_TAG}.tar.gz"
URL="https://github.com/${REPO}/releases/latest/download/${ASSET}"

command -v curl >/dev/null 2>&1 || die "curl is required"
command -v tar  >/dev/null 2>&1 || die "tar is required"

mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

printf 'downloading %s\n' "$URL"
curl -fL --progress-bar -o "$TMP/$ASSET" "$URL"

if curl -fsSL -o "$TMP/$ASSET.sha256" "$URL.sha256" 2>/dev/null; then
    EXPECTED="$(awk '{print $1}' "$TMP/$ASSET.sha256")"
    ACTUAL="$(shasum -a 256 "$TMP/$ASSET" 2>/dev/null | awk '{print $1}' || \
              sha256sum "$TMP/$ASSET" 2>/dev/null | awk '{print $1}')"
    [ -n "$ACTUAL" ] || die "no sha256 tool available"
    [ "$EXPECTED" = "$ACTUAL" ] || die "checksum mismatch: expected $EXPECTED, got $ACTUAL"
    printf 'checksum ok\n'
else
    printf 'warning: no .sha256 published, skipping verification\n'
fi

tar -xzf "$TMP/$ASSET" -C "$TMP"
BIN="$TMP/dsaudio-mcp"
[ -f "$BIN" ] || BIN="$(find "$TMP" -type f -name 'dsaudio-mcp*' | head -n 1)"
[ -f "$BIN" ] || die "the archive did not contain dsaudio-mcp"

if [ -n "$PREFIX" ]; then
    DEST_DIR="$PREFIX"
elif [ "${DSAUDIO_PREFIX:-}" != "" ]; then
    DEST_DIR="$DSAUDIO_PREFIX"
else
    DEST_DIR="$HOME/.local/bin"
fi
mkdir -p "$DEST_DIR"
install -m 0755 "$BIN" "$DEST_DIR/dsaudio-mcp"

# Make sure the directory is on PATH in the usual shells.
case ":$PATH:" in
    *":$DEST_DIR:"*) ;;
    *) printf 'note: %s is not on PATH; add it in your shell profile\n' "$DEST_DIR" ;;
esac

printf '\ninstalled %s/dsaudio-mcp\n\n' "$DEST_DIR"
printf 'add this to your MCP client:\n\n'
printf '{\n  "mcpServers": {\n    "deepseek-audio": {\n'
printf '      "command": "%s/dsaudio-mcp",\n    "args": []\n    }\n  }\n}\n\n' "$DEST_DIR"
printf 'keys: $DEEPSEEK_API_KEY, then %s/.config/dsaudio/config.json\n' "$HOME"
