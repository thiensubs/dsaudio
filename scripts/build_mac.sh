#!/bin/bash
# Build dsaudio for macOS and package it as build/dsaudio.app.
# Usage: bash scripts/build_mac.sh [Debug|Release]
set -e
cd "$(dirname "$0")/.."
TYPE="${1:-Release}"

cmake -S . -B build -DCMAKE_BUILD_TYPE="$TYPE"
cmake --build build -j"$(sysctl -n hw.ncpu)"
bash scripts/make_app.sh
