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
