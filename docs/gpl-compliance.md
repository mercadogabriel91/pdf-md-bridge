# GPL Compliance Guide (Free Tier / Browser Extension)

This document is a practical checklist for distributing the **free-tier browser extension** when it includes Poppler compiled to WebAssembly. Poppler is [GPLv2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html). Shipping the WASM module counts as **distributing a derivative work**, so the combined extension must comply with GPLv2.

This is **not legal advice**. It summarizes common open-source practice for this project. For a commercial launch, have a lawyer review your exact distribution model.

**Repository:** https://github.com/mercadogabriel91/pdf-md-bridge

---

## When these steps apply

| You ship… | GPL compliance needed? |
|---|---|
| Browser extension with Poppler WASM | **Yes** |
| Native CLI you only run locally (never distributed) | No |
| Server-side premium tier (users never receive the Poppler binary) | Generally no (GPL triggers on **distribution**) |
| Closed-source premium build that still bundles Poppler WASM | **Yes** — same rules as the free tier |

If the free tier uses **PDF.js** (Apache 2.0) instead of Poppler, this guide does not apply to that build path.

---

## What GPLv2 requires (plain language)

When you distribute the extension, recipients must be able to:

1. **Know** they received GPL-licensed software.
2. **Get the source** for the version you shipped — your code plus enough to rebuild the WASM.
3. **Exercise GPL freedoms** — use, modify, and redistribute under GPLv2.

You do **not** pay Poppler. You **do** make source accessible and license your distributed code under GPL-compatible terms.

---

## Checklist before each store release

### 1. Repository (corresponding source)

The public repo must match what you ship.

- [ ] Tag each store release (e.g. `extension-v1.0.0`) pointing at the exact commit built for the store.
- [ ] Add a root `LICENSE` file with the full GPLv2 text.
- [ ] Add a `NOTICE` file (or README section) crediting third-party libraries, at minimum:
  - **Poppler** — GPLv2 — https://poppler.freedesktop.org/
  - Any other libraries linked into the WASM binary (FreeType, OpenJPEG, etc.) with their licenses.
- [ ] Document **how to rebuild the WASM module** (see [Build documentation](#build-documentation-for-rebuilders) below). Source alone is not enough if nobody can reproduce the binary.

### 2. Inside the extension package

Include license text in the shipped ZIP, not only on GitHub.

- [ ] Add `NOTICE.txt` (or `LICENSE.txt`) to the extension bundle with:
  - Short statement that the extension contains GPLv2 software.
  - Link to the repository.
  - Poppler copyright and license reference.
- [ ] Add a visible **Source code** link in the UI (popup footer, About page, or settings). It must be easy to find without contacting you.

### 3. Store listing

- [ ] Add a **Source code** line in the Chrome Web Store / Firefox Add-ons description pointing to the repo (and optionally the release tag).
- [ ] If the store has a “website” or “support” field, use the repo URL or a project page that links to it.

### 4. Release hygiene

- [ ] Store build comes from a tagged commit; tag name is mentioned in release notes or store changelog.
- [ ] If the extension version is `1.2.0`, tag `extension-v1.2.0` (or similar) exists at the built commit.
- [ ] README states the project license: **GPLv2**.

---

## Where to put the “here is the repo” link

Use **at least two** of these so users and reviewers can find source easily:

| Location | Recommendation |
|---|---|
| Extension popup | Footer link: “Source code” → GitHub repo |
| About / Settings screen | Full sentence + link (see templates below) |
| `NOTICE.txt` in package | Repo URL + GPLv2 reference |
| Chrome Web Store description | One line with repo URL |
| GitHub README | Prominent link; badge optional |

The link should be **stable** (repo root or releases page), not a branch that gets force-pushed away.

---

## Copy-paste templates

### Extension UI (short)

```text
Source code (GPLv2): https://github.com/mercadogabriel91/pdf-md-bridge
```

### Extension UI (About screen)

```text
This extension includes software licensed under the GNU General Public License
version 2 (GPLv2), including Poppler (https://poppler.freedesktop.org/).

Source code for this extension:
https://github.com/mercadogabriel91/pdf-md-bridge

You may copy, modify, and redistribute this software under the terms of GPLv2.
```

### `NOTICE.txt` (shipped inside the extension)

```text
pdf-md-bridge browser extension
Copyright (C) Gabriel Mercado and contributors

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

Full license: https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

Corresponding source code:
https://github.com/mercadogabriel91/pdf-md-bridge

Third-party components:

  Poppler — GPLv2
  https://poppler.freedesktop.org/

  (List other libraries linked into pdf_md_bridge.wasm here.)
```

### Chrome Web Store description (append)

```text
Source code (GPLv2): https://github.com/mercadogabriel91/pdf-md-bridge
```

### README badge line (optional)

```markdown
License: GPLv2 — see [LICENSE](../LICENSE).
```

---

## Build documentation for rebuilders

GPLv2 requires **corresponding source** — the preferred form for making modifications. For WASM that means documenting the build, not only checking in `main.cpp`.

Add a section to the README (or `docs/building-wasm.md` once Phase 2 exists) covering:

1. **Toolchain** — Emscripten version (e.g. `3.1.x`), activation steps (`emsdk install`, `source emsdk_env.sh`).
2. **Dependencies** — Poppler and transitive libs, versions, and how they were built for WASM.
3. **Commands** — exact `emcmake` / `emmake` or script invocations to produce `pdf_md_bridge.wasm`.
4. **Output** — which artifacts go into the extension (`pdf_md_bridge.wasm`, glue JS, etc.).
5. **Tag** — which git tag matches the store release.

Example skeleton (fill in when the WASM build exists):

```markdown
## Building the WASM module

Requires Emscripten 3.x and Poppler built for `wasm32-unknown-emscripten`.

    git clone https://github.com/mercadogabriel91/pdf-md-bridge.git
    cd pdf-md-bridge
    git checkout extension-v1.0.0
    ./scripts/build-wasm.sh
    # Produces: extension/wasm/pdf_md_bridge.wasm

See docs/building-wasm.md for dependency build order.
```

---

## Suggested repo files (not all exist yet)

| File | Purpose |
|---|---|
| `LICENSE` | Full GPLv2 text at repository root |
| `NOTICE` | Third-party attributions (Poppler, etc.) |
| `docs/gpl-compliance.md` | This guide |
| `docs/building-wasm.md` | WASM rebuild instructions (add in Phase 2) |
| `extension/NOTICE.txt` | Shipped with the store package |

When you add the extension scaffold (Phase 4), wire the UI link and copy `NOTICE.txt` into the build output in your Vite/esbuild pipeline.

---

## Premium tier reminder

You can open-source the free extension and still charge for premium **if** the premium offering does not distribute Poppler-linked binaries without the same GPL obligations.

Safe patterns:

- **Premium = server-side** processing (users never receive Poppler WASM).
- **Premium = different stack** (e.g. no Poppler in the closed component).

Risky pattern:

- Free extension is GPL + public repo, but premium ships a **closed-source** build that still bundles Poppler WASM — that build must also comply with GPLv2.

---

## Quick pre-publish review

Before uploading to the Chrome Web Store or Firefox Add-ons:

1. Repo is public and tagged for this release.
2. `LICENSE` and third-party `NOTICE` exist in the repo.
3. Extension UI has a visible source link.
4. `NOTICE.txt` is inside the packaged extension.
5. Store listing mentions the repo URL.
6. WASM rebuild steps are documented (or linked) for the tagged commit.

If all six are true, you are following standard practice for a GPLv2 browser extension. Adjust as your lawyer recommends for your jurisdiction and business structure.
