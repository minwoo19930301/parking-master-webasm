#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-web"

rm -rf "$BUILD_DIR"
rm -f "$ROOT_DIR/index.html" "$ROOT_DIR/index.js" "$ROOT_DIR/index.wasm" "$ROOT_DIR/index.data"

emcmake cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLATFORM=Web
cmake --build "$BUILD_DIR" --config Release

printf '\nBuilt web bundle:\n'
printf '  %s\n' "$ROOT_DIR/index.html" "$ROOT_DIR/index.js" "$ROOT_DIR/index.wasm"
