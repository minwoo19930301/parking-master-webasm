#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist"
CLIENT_DIR="$DIST_DIR/client"
SERVER_DIR="$DIST_DIR/server"

for artifact in index.html index.js index.wasm og.png; do
  if [[ ! -s "$ROOT_DIR/$artifact" ]]; then
    printf 'Missing deploy artifact: %s\n' "$ROOT_DIR/$artifact" >&2
    exit 1
  fi
done

rm -rf "$DIST_DIR"
mkdir -p "$CLIENT_DIR" "$SERVER_DIR"

cp "$ROOT_DIR/index.html" "$ROOT_DIR/index.js" "$ROOT_DIR/index.wasm" \
  "$ROOT_DIR/og.png" "$CLIENT_DIR/"
if [[ -f "$ROOT_DIR/index.data" ]]; then
  cp "$ROOT_DIR/index.data" "$CLIENT_DIR/"
fi

cp "$ROOT_DIR/sites/_headers" "$CLIENT_DIR/_headers"
mkdir -p "$CLIENT_DIR/docs"
cp -R "$ROOT_DIR/docs/dobong" "$CLIENT_DIR/docs/"
cp -R "$ROOT_DIR/docs/road-driving" "$CLIENT_DIR/docs/"
cp "$ROOT_DIR/sites/worker.js" "$SERVER_DIR/index.js"
cp "$ROOT_DIR/sites/wrangler.json" "$SERVER_DIR/wrangler.json"

node --check "$SERVER_DIR/index.js"

printf '\nPrepared Sites bundle:\n'
printf '  %s\n' "$CLIENT_DIR" "$SERVER_DIR"
