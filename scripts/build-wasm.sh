#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WASM_DEPS="${WASM_DEPS:-$HOME/dev/pdf-md-wasm-deps}"
OUT_DIR="$ROOT/web"

source "${EMSDK:-$ROOT/emsdk}/emsdk_env.sh"

POPPLER_BUILD="$WASM_DEPS/build-poppler"
FREETYPE="$WASM_DEPS/install-freetype"
ZLIB="$WASM_DEPS/install-zlib"

for path in \
  "$POPPLER_BUILD/libpoppler.a" \
  "$POPPLER_BUILD/cpp/libpoppler-cpp.a" \
  "$FREETYPE/lib/libfreetype.a" \
  "$ZLIB/lib/libz.a"; do
  if [[ ! -f "$path" ]]; then
    echo "Missing dependency: $path" >&2
    echo "Build Poppler for WASM first (see docs/free-tier-plan.md Phase 2)." >&2
    exit 1
  fi
done

mkdir -p "$OUT_DIR"

em++ "$ROOT/main.cpp" \
  -I "$WASM_DEPS/poppler/cpp" \
  -I "$POPPLER_BUILD/cpp" \
  -I "$WASM_DEPS/poppler" \
  -I "$POPPLER_BUILD" \
  -I "$POPPLER_BUILD/poppler" \
  "$POPPLER_BUILD/cpp/libpoppler-cpp.a" \
  "$POPPLER_BUILD/libpoppler.a" \
  "$FREETYPE/lib/libfreetype.a" \
  "$ZLIB/lib/libz.a" \
  -O2 -std=c++20 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s EXPORT_NAME=createPdfMdBridgeModule \
  -s EXPORTED_FUNCTIONS='["_malloc","_free","_convert","_free_result"]' \
  -s EXPORTED_RUNTIME_METHODS='["HEAPU8","UTF8ToString"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ENVIRONMENT=web,worker \
  -s DYNAMIC_EXECUTION=0 \
  -o "$OUT_DIR/pdf_md_bridge.js"

echo "Built $OUT_DIR/pdf_md_bridge.js and $OUT_DIR/pdf_md_bridge.wasm"
