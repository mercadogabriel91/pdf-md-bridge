#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB="$ROOT/web"
EXT="$ROOT/extension"

for file in app.js converter-worker.js pdf_md_bridge.js pdf_md_bridge.wasm; do
  src="$WEB/$file"
  if [[ ! -f "$src" ]]; then
    echo "Missing $src — run ./scripts/build-wasm.sh first." >&2
    exit 1
  fi
  cp "$src" "$EXT/$file"
done

echo "Synced WASM runtime into $EXT/"
