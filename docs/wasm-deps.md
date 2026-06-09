# WASM dependency workspace (`~/dev/pdf-md-wasm-deps`)

This document describes the **external dependency tree** used to compile Poppler for WebAssembly. It lives **outside** the `pdf-md-bridge` repository on purpose.

---

## Why this directory exists

The native Mac build (CLion / Homebrew) links against **Poppler compiled for macOS**. The browser needs **Poppler compiled for WASM** with Emscripten (`emcc` / `em++`).

There is no `brew install poppler-wasm`. You cross-compile Poppler and its dependencies once, install the static libraries into this workspace, and then `scripts/build-wasm.sh` links your `main.cpp` against them.

```
Native (CLion):     main.cpp  +  Homebrew Poppler  →  pdf-md-bridge (Mac binary)
Browser (WASM):     main.cpp  +  pdf-md-wasm-deps   →  web/pdf_md_bridge.wasm + .js
```

Without `pdf-md-wasm-deps`, `./scripts/build-wasm.sh` cannot run.

---

## Location and relationship to this repo

| Item | Path | In git? |
|------|------|---------|
| Your converter + web UI | `~/dev/personal/pdf-md-bridge` | Yes |
| Emscripten SDK | `pdf-md-bridge/emsdk/` | No (gitignored) |
| **WASM dependency tree** | **`~/dev/pdf-md-wasm-deps`** | **No — local machine only** |
| Generated WASM output | `pdf-md-bridge/web/pdf_md_bridge.{js,wasm}` | No (gitignored; rebuild with script) |

Override the deps path when building:

```bash
WASM_DEPS=/some/other/path ./scripts/build-wasm.sh
```

Default is `$HOME/dev/pdf-md-wasm-deps` (see `scripts/build-wasm.sh`).

---

## Directory layout

After a successful setup, the workspace looks like this:

```
~/dev/pdf-md-wasm-deps/
├── freetype/              # Source: FreeType (font rendering)
├── zlib/                  # Source: zlib (compression)
├── poppler/               # Source: Poppler 26.04.0 (+ local Emscripten patch)
├── build-freetype/        # CMake build tree for FreeType (WASM)
├── build-zlib/            # CMake build tree for zlib (WASM)
├── build-poppler/         # CMake build tree for Poppler (WASM)
├── install-freetype/      # Installed WASM static lib + headers
│   └── lib/libfreetype.a
└── install-zlib/          # Installed WASM static lib + headers
    └── lib/libz.a
```

### What each part does

| Path | Role |
|------|------|
| **`freetype/`**, **`zlib/`**, **`poppler/`** | Upstream source repos (cloned with `git`). Safe to delete and re-clone; you must re-apply the Poppler patch after re-cloning. |
| **`build-*`** | CMake/Emscripten object files and intermediates. Can be deleted; rerun `emcmake` + `emmake` to recreate. |
| **`install-freetype/`**, **`install-zlib/`** | Prefix installs consumed by Poppler’s configure step and by `build-wasm.sh`. |
| **`build-poppler/libpoppler.a`** | Core Poppler static library (~6 MB). |
| **`build-poppler/cpp/libpoppler-cpp.a`** | C++ wrapper your code includes via `<poppler-document.h>`. |
| **`build-poppler/config.h`**, **`build-poppler/cpp/`** | Generated headers (`poppler_cpp_export.h`, etc.) required at compile time. |

---

## How it connects to `pdf-md-bridge`

`scripts/build-wasm.sh` expects these files to exist:

```
$WASM_DEPS/build-poppler/libpoppler.a
$WASM_DEPS/build-poppler/cpp/libpoppler-cpp.a
$WASM_DEPS/install-freetype/lib/libfreetype.a
$WASM_DEPS/install-zlib/lib/libz.a
```

It compiles `main.cpp` with Emscripten, links the libraries above, and writes:

```
pdf-md-bridge/web/pdf_md_bridge.js
pdf-md-bridge/web/pdf_md_bridge.wasm
```

Day-to-day workflow after deps are built:

```bash
source ~/.zprofile          # emcc on PATH
./scripts/build-wasm.sh     # after editing main.cpp
cd web && npx serve .       # test in browser
```

You **do not** need to rebuild Poppler when tweaking `main.cpp` — only rerun `build-wasm.sh`.

---

## Required Poppler patch (Emscripten)

Poppler **26.04.0** fails to compile under Emscripten’s libc++ with errors like:

```text
error: invalid application of 'sizeof' to an incomplete type 'Array'
```

Cause: C++23 makes `std::unique_ptr`’s destructor require a complete type, but `Object.h` defines inline constructors using forward-declared `Array` and `Dict`.

**Fix:** move those constructors out of the header into `Object.cc` (where `Array.h` / `Dict.h` are included).

Files patched under `~/dev/pdf-md-wasm-deps/poppler/`:

- `poppler/Object.h` — declare constructors only
- `poppler/Object.cc` — define constructors

Verify the patch is still applied:

```bash
cd ~/dev/pdf-md-wasm-deps/poppler
git diff poppler/Object.h poppler/Object.cc
```

Re-apply manually after re-cloning Poppler, or save the diff:

```bash
git diff poppler/Object.h poppler/Object.cc > ~/dev/personal/pdf-md-bridge/scripts/poppler-emscripten.patch
# later:
cd ~/dev/pdf-md-wasm-deps/poppler && git apply ~/dev/personal/pdf-md-bridge/scripts/poppler-emscripten.patch
```

---

## Prerequisites

[Emscripten](https://emscripten.org/) (`emsdk`), `cmake` on PATH, and `source ~/.zprofile` (or `source emsdk/emsdk_env.sh`).

**Emscripten version:** This project has been built with **Emscripten 5.0.x** (`emcc --version`). Older 3.x releases may work but are not routinely tested. Install via the repo-local SDK:

```bash
cd pdf-md-bridge/emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

Or point `EMSDK` at your own install when running `build-wasm.sh`.

---

## How to rebuild from scratch

### 1. FreeType

```bash
cd ~/dev/pdf-md-wasm-deps
git clone --depth 1 --branch VER-2-13-3 https://gitlab.freedesktop.org/freetype/freetype.git
cd freetype
emcmake cmake -B ../build-freetype -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INSTALL_PREFIX=../install-freetype
emmake make -C ../build-freetype -j$(sysctl -n hw.ncpu)
emmake make -C ../build-freetype install
```

### 2. zlib

```bash
cd ~/dev/pdf-md-wasm-deps
git clone --depth 1 --branch v1.3.1 https://github.com/madler/zlib.git
cd zlib
emcmake cmake -B ../build-zlib -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INSTALL_PREFIX=../install-zlib
emmake make -C ../build-zlib -j$(sysctl -n hw.ncpu)
emmake make -C ../build-zlib install
```

### 3. Poppler

```bash
cd ~/dev/pdf-md-wasm-deps
git clone --depth 1 --branch poppler-26.04.0 https://gitlab.freedesktop.org/poppler/poppler.git
cd poppler
# Apply Emscripten patch (Object.h / Object.cc) — see above

rm -rf ../build-poppler
emcmake cmake -B ../build-poppler -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DFREETYPE_INCLUDE_DIRS=$HOME/dev/pdf-md-wasm-deps/install-freetype/include/freetype2 \
  -DFREETYPE_LIBRARY=$HOME/dev/pdf-md-wasm-deps/install-freetype/lib/libfreetype.a \
  -DZLIB_INCLUDE_DIR=$HOME/dev/pdf-md-wasm-deps/install-zlib/include \
  -DZLIB_LIBRARY=$HOME/dev/pdf-md-wasm-deps/install-zlib/lib/libz.a \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_CPP=ON \
  -DENABLE_UTILS=OFF \
  -DENABLE_GLIB=OFF \
  -DENABLE_GOBJECT_INTROSPECTION=OFF \
  -DENABLE_QT5=OFF \
  -DENABLE_QT6=OFF \
  -DBUILD_GTK_TESTS=OFF \
  -DBUILD_QT5_TESTS=OFF \
  -DBUILD_QT6_TESTS=OFF \
  -DBUILD_CPP_TESTS=OFF \
  -DBUILD_MANUAL_TESTS=OFF \
  -DENABLE_BOOST=OFF \
  -DENABLE_NSS3=OFF \
  -DENABLE_GPGME=OFF \
  -DENABLE_LIBCURL=OFF \
  -DENABLE_LIBTIFF=OFF \
  -DENABLE_LCMS=OFF \
  -DENABLE_DCTDECODER=none \
  -DENABLE_LIBOPENJPEG=none \
  -DFONT_CONFIGURATION=generic

emmake make -C ../build-poppler -j$(sysctl -n hw.ncpu)
```

### 4. Your converter

```bash
cd ~/dev/personal/pdf-md-bridge
./scripts/build-wasm.sh
```

This writes `web/pdf_md_bridge.js` and `web/pdf_md_bridge.wasm`, then runs `scripts/sync-extension.sh`, which copies the WASM runtime plus `web/app.js` and `web/converter-worker.js` into `extension/`. The extension’s `NOTICE.txt` is tracked in git and is **not** overwritten by sync — it must stay in the packaged folder for store releases.

To sync web → extension without recompiling WASM (e.g. after editing `web/app.js` only):

```bash
./scripts/sync-extension.sh
```

For a store release, check out the tagged commit (e.g. `extension-v0.1.0`) before building so the published binary matches [corresponding source](gpl-compliance.md).

---

## How to handle this directory (do’s and don’ts)

### Do

- **Keep the whole tree** on your dev machine while actively working on WASM — rebuilding Poppler takes time.
- **Back up** at minimum: the Poppler patch (diff or patched files) and these notes.
- **Re-run only `build-wasm.sh`** when changing `main.cpp` or conversion logic.
- **Document** if you move the folder; set `WASM_DEPS` accordingly.
- **Use the same Poppler version** as native Homebrew (26.04.0) when possible, so behavior matches CLion.

### Don’t

- **Don’t commit** `pdf-md-wasm-deps` into `pdf-md-bridge` — it is hundreds of MB of sources and build artifacts.
- **Don’t commit** `web/pdf_md_bridge.wasm` — regenerate per machine; binary is large and GPL-linked.
- **Don’t delete** `install-*` or `build-poppler/*.a` unless you plan to rebuild deps.
- **Don’t assume** Homebrew Poppler can be linked from `em++` — architectures and toolchains differ.

### Optional cleanup (safe if you can rebuild)

| Delete | Effect |
|--------|--------|
| `build-freetype`, `build-zlib`, `build-poppler` | Frees disk; rerun `emmake` in each |
| `freetype/`, `zlib/`, `poppler/` sources | Re-clone + patch + rebuild |
| Entire `pdf-md-wasm-deps/` | Full Phase 2 redo |

---

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| `Missing dependency: .../libpoppler.a` | Poppler WASM not built; run Phase 3 above |
| `Could NOT find Freetype` / `ZLIB` during Poppler configure | Build/install freetype or zlib first; pass explicit `-D*_LIBRARY` paths |
| `incomplete type 'Array'` during Poppler compile | Emscripten patch not applied |
| `emcmake: cmake executable not found` | Install CMake (`brew install cmake`) |
| Browser shows blank / fetch error for `.wasm` | Serve over HTTP (`npx serve web`), not `file://` |
| Output differs from native CLion | Expected for some PDFs (no JPEG/JPX decoders in minimal Poppler build); compare on same PDF |

---

## Related files in this repo

| File | Purpose |
|------|---------|
| `scripts/build-wasm.sh` | Links `main.cpp` → `web/pdf_md_bridge.wasm` |
| `scripts/sync-extension.sh` | Copies WASM + web JS into `extension/` |
| `scripts/poppler-emscripten.patch` | Required Poppler fix for Emscripten libc++ |
| `web/index.html`, `web/app.js` | Browser test page |
| `extension/NOTICE.txt` | GPL notice shipped inside the store package |
| `main.cpp` | Shared logic; `#ifdef __EMSCRIPTEN__` exports for WASM |
| `docs/gpl-compliance.md` | Store release / GPL checklist |

---

## Summary

`~/dev/pdf-md-wasm-deps` is your **local WASM toolchain cache**: cross-compiled Poppler + FreeType + zlib that the browser build depends on. It is **necessary** because Poppler must target WASM, not macOS. It stays **outside git**; what you commit in `pdf-md-bridge` is the source code and `build-wasm.sh` that *consumes* these libraries. Treat the directory as **precious but reproducible** — back up the Poppler patch, and you can always rebuild the rest.
