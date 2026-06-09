# pdf-md-bridge

Convert PDF files to Markdown entirely on the client — no server, no upload.

**License: [GPLv2](LICENSE)** — required because the browser build links [Poppler](https://poppler.freedesktop.org/) (GPLv2) compiled to WebAssembly. See [docs/gpl-compliance.md](docs/gpl-compliance.md) for the distribution checklist.

**Source:** https://github.com/mercadogabriel91/pdf-md-bridge

---

## What’s in this repo

| Component | Path | Purpose |
|-----------|------|---------|
| Native CLI | `main.cpp`, `CMakeLists.txt` | Local PDF → Markdown via Homebrew Poppler |
| WASM module | `scripts/build-wasm.sh` → `web/pdf_md_bridge.{js,wasm}` | Poppler-linked converter for browser / extension |
| Web test page | `web/` | Worker-based UI; serve with any static HTTP server |
| Chrome extension (MV3) | `extension/` | Popup file picker; loads WASM from extension package |

WASM binaries and copied extension JS are **not** committed — rebuild with the script below.

---

## Build: native CLI (macOS)

Requires Poppler (e.g. `brew install poppler`).

```bash
cmake -B build -S .
cmake --build build
./build/pdf-md-bridge input.pdf output.md
```

---

## Build: WASM + extension

### Prerequisites

1. **Emscripten** — install via [emsdk](https://emscripten.org/) (project uses `pdf-md-bridge/emsdk/` locally, or set `EMSDK`). Tested with Emscripten **5.0.x** (`emcc --version`).
2. **WASM dependencies** — Poppler, FreeType, and zlib cross-compiled for `wasm32-unknown-emscripten`. Full steps: [docs/wasm-deps.md](docs/wasm-deps.md).

Default dependency workspace: `~/dev/pdf-md-wasm-deps` (override with `WASM_DEPS=...`).

### Commands

```bash
source ~/.zprofile          # or: source emsdk/emsdk_env.sh
./scripts/build-wasm.sh     # main.cpp → web/pdf_md_bridge.{js,wasm}
                            # then syncs web/ → extension/
```

Sync web assets into the extension only (no WASM rebuild):

```bash
./scripts/sync-extension.sh
```

### Test

**Web:** serve `web/` over HTTP (not `file://`):

```bash
cd web && npx serve .
```

**Extension:** Chrome → `chrome://extensions` → Load unpacked → select `extension/`.

---

## Store releases

Tag each extension upload so the public repo matches what you ship:

```bash
git tag extension-v0.1.0    # match manifest.json "version"
git push origin extension-v0.1.0
```

Mention the tag in release notes or the store changelog.

### Pre-publish checklist

1. Repo is public; tag exists for this build.
2. Root `LICENSE` and `NOTICE` are present.
3. `extension/NOTICE.txt` is in the packaged folder.
4. Popup shows **Source code (GPLv2)** link.
5. Store listing includes the repo URL (see below).
6. A rebuilder can follow [docs/wasm-deps.md](docs/wasm-deps.md) + `scripts/build-wasm.sh` at the tagged commit.

Full checklist: [docs/gpl-compliance.md](docs/gpl-compliance.md).

### Chrome Web Store description (append)

```text
Source code (GPLv2): https://github.com/mercadogabriel91/pdf-md-bridge
```

Use the repo URL in the store **Website** or **Support** field as well.

### Firefox Add-ons

Same MV3 manifest and CSP (`wasm-unsafe-eval` on extension pages). Package the `extension/` folder as a ZIP; include `NOTICE.txt` and the source link in the listing description. No `browser_specific_settings` block is required for a basic Chrome-ported MV3 add-on, but verify WASM loads after signing — AMO review may ask for the same source URL as Chrome.

---

## Third-party notices

See [NOTICE](NOTICE). Libraries linked into `pdf_md_bridge.wasm`:

- Poppler 26.04.0 (GPLv2)
- FreeType 2.13.3 (FTL)
- zlib 1.3.1 (zlib License)

Poppler requires the Emscripten patch in `scripts/poppler-emscripten.patch`.

---

## Documentation

| Doc | Purpose |
|-----|---------|
| [docs/gpl-compliance.md](docs/gpl-compliance.md) | GPL distribution checklist and templates |
| [docs/wasm-deps.md](docs/wasm-deps.md) | Rebuild Poppler/FreeType/zlib for WASM |
