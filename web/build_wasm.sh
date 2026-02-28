#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT_DIR}/frontend/public/engine"

if ! command -v emcc >/dev/null 2>&1; then
  echo "error: emcc not found. Install Emscripten and activate it before building." >&2
  echo "hint: source <emsdk>/emsdk_env.sh" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

echo "Building Commander engine WASM..."

emcc \
  -std=c++17 \
  -O3 \
  -DNDEBUG \
  -fexceptions \
  -I"${ROOT_DIR}/backend" \
  "${ROOT_DIR}/backend/engine.cpp" \
  "${ROOT_DIR}/backend/engine_api.cpp" \
  -o "${OUT_DIR}/commander_engine.js" \
  -sWASM=1 \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=CommanderEngine \
  -sEXPORTED_FUNCTIONS='["_cc_init","_cc_new_game","_cc_set_position","_cc_get_position","_cc_get_best_move","_cc_apply_move","_cc_get_sprites_json","_cc_get_last_error"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  -sALLOW_MEMORY_GROWTH=1 \
  -sDISABLE_EXCEPTION_CATCHING=0 \
  -sFILESYSTEM=0 \
  -sENVIRONMENT=web,worker \
  -sASSERTIONS=0 \
  -sUSE_SDL=2 \
  -sUSE_SDL_IMAGE=2 \
  -sUSE_SDL_TTF=2

echo "WASM build complete: ${OUT_DIR}/commander_engine.js"
echo "WASM build complete: ${OUT_DIR}/commander_engine.wasm"
