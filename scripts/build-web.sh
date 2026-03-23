#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-web"
BUILD_VERSION="$(date +%Y%m%d%H%M%S)"
TEMP_SHELL="$BUILD_DIR/shell.${BUILD_VERSION}.html"

rm -rf "$BUILD_DIR"
rm -f "$ROOT_DIR/index.html" "$ROOT_DIR/index.js" "$ROOT_DIR/index.wasm" "$ROOT_DIR/index.data"
mkdir -p "$BUILD_DIR"

sed "s/__BUILD_VERSION__/${BUILD_VERSION}/g" "$ROOT_DIR/web/shell.html" > "$TEMP_SHELL"

emcmake cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLATFORM=Web -DWEB_SHELL_FILE="$TEMP_SHELL"
cmake --build "$BUILD_DIR" --config Release

printf '\nBuilt web bundle:\n'
printf '  %s\n' "$ROOT_DIR/index.html" "$ROOT_DIR/index.js" "$ROOT_DIR/index.wasm"
printf 'Build version: %s\n' "$BUILD_VERSION"
