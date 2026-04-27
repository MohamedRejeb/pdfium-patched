# pdfium-patched

Build-and-distribute pipeline for Google's [PDFium](https://pdfium.googlesource.com/pdfium/) with a small additive patch set that exposes a few internal C++ accessors as `FPDF_EXPORT` C symbols. Releases are drop-in compatible with [bblanchon/pdfium-binaries](https://github.com/bblanchon/pdfium-binaries) so any downstream Gradle / CMake build that today consumes bblanchon's binaries can switch by URL only.

- **Pinned PDFium ref:** see [`PDFIUM_VERSION`](PDFIUM_VERSION)
- **Patches revision:** see [`PATCHES_VERSION`](PATCHES_VERSION)
- **Tag format:** `v<pdfium-tag>+rejeb.<patches-rev>` (e.g. `v7811+rejeb.0`)

## Consuming releases

Each tag publishes one zip per platform under
`https://github.com/MohamedRejeb/pdfium-patched/releases/download/<tag>/libpdfium-<target>-<tag>.zip`.

Targets: `mac-arm64`, `mac-x64`, `linux-x64`, `win-x64`. Mac / Linux zip layout:

```
LICENSE              # PDFium's upstream license
VERSION              # one line: <pdfium-tag>+rejeb.<patches-rev>
lib/libpdfium.{dylib|so}
include/
  fpdfview.h, fpdf_annot.h, ...   # all upstream public/*.h
  cpp/fpdf_deleters.h, ...        # public/cpp/*.h
  fpdf_rejeb.h                    # additional FPDF_EXPORT declarations from this repo
```

Windows splits the runtime DLL and import library across `bin/` and `lib/` — same as bblanchon's layout:

```
bin/pdfium.dll
lib/pdfium.dll.lib   # MSVC import library
```

Gradle example (mirrors a bblanchon-style download task):

```kotlin
val downloadUrl = "https://github.com/MohamedRejeb/pdfium-patched/releases/download/v${pdfiumVersion}+rejeb.${patchesVersion}/libpdfium-$os-$arch.zip"
```

## Building locally

### Prerequisites

- [`depot_tools`](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html) on `PATH` — provides `gn`, `ninja`, `gclient`. Verify with `which gn ninja gclient`.
- macOS: Xcode 15+ command-line tools, Python 3.
- Linux: `apt install build-essential python3 git curl pkg-config libfontconfig1-dev`.
- Windows: Visual Studio 2022 with Desktop C++ workload, Python 3.

### Steps

```bash
export PDFIUM_PATCHED_ROOT="$(pwd)"

./scripts/01-fetch-pdfium.sh                  # gclient sync to pinned commit
./scripts/02-apply-patches.sh                 # git apply patches/*.patch
./scripts/03-configure.sh mac-arm64           # gn gen with args/<target>.args.gn
./scripts/04-build.sh mac-arm64               # ninja + smoke-test + package into dist/<target>/
```

Pick `mac-arm64 | mac-x64 | linux-x64 | win-x64` for the target argument. Output lands in `dist/<target>/{lib,include}/`.

`./scripts/02-apply-patches.sh` resets the PDFium working tree (`git reset --hard && git clean -fd`) before applying — re-running is safe but destroys any manual edits inside `pdfium/`.

## Authoring a new patch

1. Manually clone PDFium at the pinned commit, make the change, commit it, then `git format-patch -1` to produce the `.patch` file.
2. Verify the C++ accessor you're wrapping actually exists at the pinned commit — PDFium internals shift between Chromium milestones.
3. Look at how existing `fpdfsdk/fpdf_*.cpp` files convert `FPDF_PAGEOBJECT` → `CPDF_PageObject*` (`CPDFPageObjectFromFPDFPageObject`) and follow the same pattern.
4. Number the patch sequentially (`0001-…`, `0002-…`).
5. Add the new declarations to [`include/fpdf_rejeb.h`](include/fpdf_rejeb.h) — `02-apply-patches.sh` copies this file into PDFium's `public/`.
6. Bump [`PATCHES_VERSION`](PATCHES_VERSION) and tag with the new `+rejeb.N`.

### Conventions (non-negotiable)

- **Naming:** all new exports prefixed `FPDFRejeb_` to stay out of upstream's namespace.
- **One concept per patch:** patches apply in lexical order; bundling capabilities makes selective rebases impossible.
- **Additive only:** new files only (`fpdfsdk/fpdf_rejeb.cpp`, `public/fpdf_rejeb.h`) plus a one-line addition to `fpdfsdk/BUILD.gn`. Do not modify upstream files.

### `infra/` vs `patches/`

- [`infra/`](infra/) holds build-pipeline patches (always applied; not counted in `PATCHES_VERSION`) — vendored from `bblanchon/pdfium-binaries` for drop-in compatibility. Re-vendor on PDFium bumps if upstream drifts in the patched files. See [`infra/NOTICE`](infra/NOTICE).
- [`patches/`](patches/) holds feature patches that bump `PATCHES_VERSION`. Each adds one `FPDFRejeb_*` capability.

## License

BSD 3-Clause for this repository. PDFium itself is BSD 3-Clause and ships its own license alongside the binary release artifacts. See [LICENSE](LICENSE).
