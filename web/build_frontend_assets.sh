#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PUBLIC_DIR="$ROOT_DIR/frontend/public"
INDEX_FILE="$PUBLIC_DIR/index.html"

STYLE_SRC="$PUBLIC_DIR/style.css"
APP_SRC="$PUBLIC_DIR/app.js"

if [[ ! -f "$STYLE_SRC" || ! -f "$APP_SRC" || ! -f "$INDEX_FILE" ]]; then
  echo "Missing required frontend files under $PUBLIC_DIR" >&2
  exit 1
fi

engine_wasm="$PUBLIC_DIR/engine/commander_engine.wasm"
engine_js="$PUBLIC_DIR/engine/commander_engine.js"
worker_js="$PUBLIC_DIR/engine/engine_worker.js"
bridge_js="$PUBLIC_DIR/engine/engine_bridge.js"

engine_combined_hash=""
if [[ -f "$engine_wasm" && -f "$engine_js" && -f "$worker_js" && -f "$bridge_js" ]]; then
  wasm_hash="$(shasum -a 256 "$engine_wasm" | awk '{print substr($1,1,12)}')"
  engine_js_hash="$(shasum -a 256 "$engine_js" | awk '{print substr($1,1,12)}')"
  worker_hash="$(shasum -a 256 "$worker_js" | awk '{print substr($1,1,12)}')"
  bridge_hash="$(shasum -a 256 "$bridge_js" | awk '{print substr($1,1,12)}')"
  engine_combined_hash="$(echo "${wasm_hash}${engine_js_hash}${worker_hash}${bridge_hash}" | shasum -a 256 | awk '{print substr($1,1,12)}')"
  
  # Inject engine version into app.js before calculating its hash
  perl -pi -e "s{const ENGINE_VERSION = '[^']*';}{const ENGINE_VERSION = '${engine_combined_hash}';}g" "$APP_SRC"
fi

style_hash="$(shasum -a 256 "$STYLE_SRC" | awk '{print substr($1,1,12)}')"
app_hash="$(shasum -a 256 "$APP_SRC" | awk '{print substr($1,1,12)}')"

style_out="style.${style_hash}.css"
app_out="app.${app_hash}.js"

cp "$STYLE_SRC" "$PUBLIC_DIR/$style_out"
cp "$APP_SRC" "$PUBLIC_DIR/$app_out"

find "$PUBLIC_DIR" -maxdepth 1 -type f -name 'style.*.css' ! -name "$style_out" -delete
find "$PUBLIC_DIR" -maxdepth 1 -type f -name 'app.*.js' ! -name "$app_out" -delete

perl -0pi -e "s{href=\"/style(?:\\.[0-9a-f]{8,64})?\\.css(?:\\?v=[^\"]*)?\"}{href=\"/$style_out\"}g; s{src=\"/app(?:\\.[0-9a-f]{8,64})?\\.js(?:\\?v=[^\"]*)?\"}{src=\"/$app_out\"}g" "$INDEX_FILE"

echo "Built hashed assets: $style_out, $app_out"

SW_FILE="$PUBLIC_DIR/sw.js"
if [[ -f "$SW_FILE" ]]; then
  combined_hash="$(echo "${style_hash}${app_hash}${engine_combined_hash}" | shasum -a 256 | awk '{print substr($1,1,12)}')"
  
  perl -pi -e "s{const CACHE_NAME = 'commander-chess-shell-[^']+';}{const CACHE_NAME = 'commander-chess-shell-${combined_hash}';}g; s{const CACHE_NAME = 'commander-chess-shell-v\\d+';}{const CACHE_NAME = 'commander-chess-shell-${combined_hash}';}g" "$SW_FILE"
  perl -pi -e "s{'/style(?:\\.[0-9a-f]{12})?\\.css'}{'/$style_out'}g" "$SW_FILE"
  perl -pi -e "s{'/app(?:\\.[0-9a-f]{12})?\\.js'}{'/$app_out'}g" "$SW_FILE"
  
  echo "Updated sw.js with cache hash: ${combined_hash}"
fi
