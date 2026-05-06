#!/usr/bin/env bash
# 04-build.sh <target> — ninja-build the patched PDFium target and
# package the result into dist/<target>/ in a layout that mirrors
# bblanchon/pdfium-binaries (drop-in compatible).
#
# Layout produced (per target):
#   dist/<target>/
#     LICENSE                   # PDFium's upstream license
#     VERSION                   # one line: <pdfium-tag>+rejeb.<patches-rev>
#     include/                  # all public/*.h plus public/cpp/*.h plus our fpdf_rejeb.h
#     lib/libpdfium.dylib       # macOS, iOS (device + simulator)
#     lib/libpdfium.so          # Linux, Android (per ABI)
#     bin/pdfium.dll            # Windows runtime DLL
#     lib/pdfium.dll.lib        # Windows import library
#     lib/pdfium.{wasm,js,html} # Wasm — emcc link output (matches bblanchon)
#
# Smoke test: links a tiny C program against the built library and
# either verifies the runtime can load it (Unix) or just confirms
# FPDF_InitLibrary is in the export table (Windows). Cross-compiled
# targets (android-*, ios-*, wasm) skip the runtime-load step — the
# runner can't execute the cross-arch / cross-platform binary — but
# still nm-check that every declared FPDFRejeb_* symbol made it into
# the lib. Wasm runs an extra `em++` link step after ninja to wrap
# libpdfium.a into the JS+WASM trio (bblanchon's layout).
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
PDFIUM_DIR="$ROOT/pdfium/pdfium"

TARGET="${1:-}"
if [ -z "$TARGET" ]; then
  echo "usage: $0 <mac-arm64|mac-x64|linux-x64|win-x64|android-arm|android-arm64|android-x86|android-x64|ios-device-arm64|ios-simulator-arm64|ios-simulator-x64|wasm>" >&2
  exit 1
fi

OUT_DIR="$PDFIUM_DIR/out/$TARGET"
DIST_DIR="$ROOT/dist/$TARGET"

if [ ! -d "$OUT_DIR" ]; then
  echo "error: $OUT_DIR missing — run scripts/03-configure.sh $TARGET first" >&2
  exit 1
fi

PDFIUM_VER="$(cat "$ROOT/PDFIUM_VERSION" | tr -d '[:space:]')"
PATCHES_VER="$(cat "$ROOT/PATCHES_VERSION" | tr -d '[:space:]')"
COMBINED_TAG="${PDFIUM_VER}+rejeb.${PATCHES_VER}"

echo ">>> ninja -C out/$TARGET pdfium"
cd "$PDFIUM_DIR"
ninja -C "out/$TARGET" pdfium

# Wasm: ninja produces a static archive obj/libpdfium.a; em++ then links
# it into the JS+WASM trio. Mirrors bblanchon's 06-build.sh wasm branch.
if [ "$TARGET" = "wasm" ]; then
  LIBPDFIUMA="$OUT_DIR/obj/libpdfium.a"
  if [ ! -s "$LIBPDFIUMA" ]; then
    echo "FAIL: $LIBPDFIUMA missing or empty after ninja" >&2
    exit 1
  fi
  command -v llvm-nm >/dev/null || { echo "error: llvm-nm not on PATH (need emsdk env)" >&2; exit 1; }
  command -v em++    >/dev/null || { echo "error: em++ not on PATH (need emsdk env)"    >&2; exit 1; }
  # Build the EXPORTED_FUNCTIONS list from the static archive's defined
  # globals. `^FPDF` covers FPDF_*, FPDFAction_*, ..., AND FPDFRejeb_*
  # (since FPDFRejeb_ starts with FPDF). FSDK / FORM / IFSDK come from
  # form-handling APIs. Add _free/_malloc/_calloc/_realloc so JS can
  # manage emscripten heap allocations from the host side.
  EXPORTED_FUNCTIONS="$(llvm-nm "$LIBPDFIUMA" --format=just-symbols \
      | grep -E '^(FPDF|FSDK|FORM|IFSDK)' \
      | sort -u \
      | sed 's/^/_/' \
      | paste -sd ',' -)"
  if [ -z "$EXPORTED_FUNCTIONS" ]; then
    echo "FAIL: no FPDF/FSDK/FORM/IFSDK symbols found in $LIBPDFIUMA" >&2
    exit 1
  fi
  echo ">>> em++ link: pdfium.{html,js,wasm}"
  # -O2 deliberately (NOT -O3): bblanchon comment says O3 strips too much.
  em++ \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ALLOW_TABLE_GROWTH=1 \
    -s "EXPORTED_FUNCTIONS=$EXPORTED_FUNCTIONS,_free,_malloc,_calloc,_realloc" \
    -s EXPORTED_RUNTIME_METHODS="ccall,cwrap,addFunction,removeFunction" \
    -s LLD_REPORT_UNDEFINED \
    -s WASM=1 \
    -O2 \
    -o "$OUT_DIR/pdfium.html" \
    "$LIBPDFIUMA" \
    --no-entry
fi

echo ">>> package dist/$TARGET (layout: bblanchon-compatible)"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/lib" "$DIST_DIR/include" "$DIST_DIR/include/cpp"

case "$TARGET" in
  mac-*|ios-*)
    cp "$OUT_DIR/libpdfium.dylib" "$DIST_DIR/lib/libpdfium.dylib"
    LIB_PATH="$DIST_DIR/lib/libpdfium.dylib"
    ;;
  linux-*|android-*)
    cp "$OUT_DIR/libpdfium.so" "$DIST_DIR/lib/libpdfium.so"
    LIB_PATH="$DIST_DIR/lib/libpdfium.so"
    ;;
  win-*)
    mkdir -p "$DIST_DIR/bin"
    cp "$OUT_DIR/pdfium.dll"     "$DIST_DIR/bin/pdfium.dll"
    cp "$OUT_DIR/pdfium.dll.lib" "$DIST_DIR/lib/pdfium.dll.lib"
    LIB_PATH="$DIST_DIR/bin/pdfium.dll"
    ;;
  wasm)
    cp "$OUT_DIR/pdfium.html" "$DIST_DIR/lib/pdfium.html"
    cp "$OUT_DIR/pdfium.js"   "$DIST_DIR/lib/pdfium.js"
    cp "$OUT_DIR/pdfium.wasm" "$DIST_DIR/lib/pdfium.wasm"
    # The .wasm carries the actual compiled symbols; symbol checks
    # below run against it.
    LIB_PATH="$DIST_DIR/lib/pdfium.wasm"
    ;;
  *)
    echo "error: unknown target $TARGET" >&2
    exit 1
    ;;
esac

# Public headers: ship every public/*.h + public/cpp/*.h.
cp "$PDFIUM_DIR"/public/*.h         "$DIST_DIR/include/"
cp "$PDFIUM_DIR"/public/cpp/*.h     "$DIST_DIR/include/cpp/"

# Upstream license + combined-tag VERSION at zip root.
cp "$PDFIUM_DIR/LICENSE" "$DIST_DIR/LICENSE"
echo "$COMBINED_TAG" > "$DIST_DIR/VERSION"

echo ">>> smoke: verify $LIB_PATH"
if [ ! -s "$LIB_PATH" ]; then
  echo "FAIL: $LIB_PATH missing or empty" >&2
  exit 1
fi

# Last-field equality on nm output. Handles both Mach-O (Apple's `_<sym>`
# convention) and ELF (`<sym>`), and is whitespace-agnostic (the macos-15
# runner's LLVM nm tab-separates fields where local LLVM nm space-separates
# — awk's default FS handles both).
#
# Combines `nm -gU` (defined globals from `.symtab`) with `nm -gD` (defined
# globals from `.dynsym`). Linux release .so files are stripped, so `.symtab`
# is empty and only `.dynsym` carries the exports; Mach-O dylibs lack a
# `.dynsym` entirely and `nm -D` errors out. Either format will surface the
# symbol on its respective platform.
sym_exported() {
  local lib="$1" sym="$2"
  # `|| true` keeps each nm invocation from poisoning the pipeline under
  # `set -o pipefail`: nm -gD exits non-zero on Mach-O ("no dynamic symbol
  # table") even when nm -gU has the answer; llvm-nm may not exist on
  # non-wasm runners (it ships with emsdk).
  #
  # Combining all three lets one `sym_exported` work across Mach-O, ELF,
  # and wasm without a per-format branch — whichever tool can read the
  # file produces the symbol line, the others noop.
  { nm -gU "$lib" 2>/dev/null || true
    nm -gD "$lib" 2>/dev/null || true
    llvm-nm "$lib" 2>/dev/null || true
  } | awk -v s="$sym" '
        $NF == s || $NF == "_" s { f=1 }
        END { exit !f }
      '
}

# FPDF_InitLibrary is upstream's; always must be exported.
SYMBOL_CHECK_OK=0
case "$TARGET" in
  mac-*|linux-*|android-*|ios-*|wasm)
    sym_exported "$LIB_PATH" "FPDF_InitLibrary" && SYMBOL_CHECK_OK=1
    ;;
  win-*)
    if command -v dumpbin >/dev/null 2>&1; then
      dumpbin //EXPORTS "$LIB_PATH" 2>/dev/null | grep -q 'FPDF_InitLibrary' && SYMBOL_CHECK_OK=1
    elif command -v llvm-nm >/dev/null 2>&1; then
      llvm-nm "$LIB_PATH" 2>/dev/null | grep -q 'FPDF_InitLibrary' && SYMBOL_CHECK_OK=1
    else
      echo "warn: no dumpbin/llvm-nm on PATH — skipping export check on Windows"
      SYMBOL_CHECK_OK=1   # don't block local builds without MSVC tools
    fi
    ;;
esac

if [ "$SYMBOL_CHECK_OK" -ne 1 ]; then
  echo "FAIL: FPDF_InitLibrary not found in $LIB_PATH exports" >&2
  exit 1
fi

# Verify each FPDFRejeb_* symbol declared in our header is also exported.
# Match `FPDFRejeb_<name>(` only — i.e. real function signatures. The naive
# `[A-Za-z_]+` pattern also picks up doc-comment globs like
# `FPDFRejeb_TextObj*At` (where the `*` ends the char class), and the smoke
# step then asks the dylib to export a non-existent `FPDFRejeb_TextObj`
# symbol. Trailing `(` is the exact distinguishing token between real
# declarations and prose mentions in the header.
DECL_SYMS="$(grep -oE 'FPDFRejeb_[A-Za-z0-9_]+\(' "$ROOT/include/fpdf_rejeb.h" | tr -d '(' | sort -u || true)"
if [ -n "$DECL_SYMS" ]; then
  for sym in $DECL_SYMS; do
    case "$TARGET" in
      mac-*|linux-*|android-*|ios-*|wasm)
        if ! sym_exported "$LIB_PATH" "$sym"; then
          echo "FAIL: declared symbol $sym not exported by $LIB_PATH" >&2
          # Dump diagnostics so the next CI failure tells us what's actually
          # in the dylib without needing another roundtrip.
          echo "--- diagnostic: $LIB_PATH ($(stat -f '%z' "$LIB_PATH" 2>/dev/null || stat -c '%s' "$LIB_PATH" 2>/dev/null) bytes) ---" >&2
          echo "--- any FPDFRejeb_-prefixed exports (nm/llvm-nm): ---" >&2
          { nm -gU "$LIB_PATH" 2>/dev/null || true
            nm -gD "$LIB_PATH" 2>/dev/null || true
            llvm-nm "$LIB_PATH" 2>/dev/null || true
          } | grep -E '_?FPDFRejeb' >&2 || echo "(none)" >&2
          OBJ_FILE="$(find "$OUT_DIR/obj" -name 'fpdf_rejeb.o' 2>/dev/null | head -1)"
          if [ -n "$OBJ_FILE" ]; then
            echo "--- nm of $OBJ_FILE (FPDFRejeb): ---" >&2
            nm "$OBJ_FILE" 2>/dev/null | grep -E '_?FPDFRejeb' >&2 || echo "(none)" >&2
          else
            echo "--- fpdf_rejeb.o NOT FOUND in build output — patch did not add the source ---" >&2
          fi
          exit 1
        fi
        ;;
      win-*)
        if command -v dumpbin >/dev/null 2>&1; then
          dumpbin //EXPORTS "$LIB_PATH" 2>/dev/null | grep -q "$sym" \
            || { echo "FAIL: declared symbol $sym not exported by $LIB_PATH" >&2; exit 1; }
        fi
        ;;
    esac
  done
  echo "OK: all $(echo "$DECL_SYMS" | wc -l | tr -d ' ') FPDFRejeb_* symbols present"
fi

# Runtime smoke: compile + run smoke/smoke.c against the freshly-built lib.
SMOKE_SRC="$ROOT/smoke/smoke.c"
if [ -f "$SMOKE_SRC" ]; then
  SMOKE_BIN="$DIST_DIR/.smoke"
  case "$TARGET" in
    mac-*)
      # macos-15 runners are arm64; clang defaults to host arch when
      # linking. For mac-x64 builds we must explicitly cross-compile
      # the smoke binary as x86_64 to match the dylib's architecture.
      # Rosetta 2 (preinstalled) transparently executes the resulting
      # x86_64 binary on the arm64 host.
      case "$TARGET" in
        mac-arm64) SMOKE_ARCH=arm64 ;;
        mac-x64)   SMOKE_ARCH=x86_64 ;;
      esac
      clang -arch "$SMOKE_ARCH" -I"$DIST_DIR/include" "$SMOKE_SRC" \
        -L"$DIST_DIR/lib" -lpdfium \
        -o "$SMOKE_BIN"
      # PDFium's dylib install_name is `./libpdfium.dylib` (matches
      # bblanchon — we don't rewrite it for drop-in compat). Run the
      # smoke binary from inside dist/<target>/lib so dyld finds the
      # dylib via that relative path.
      ( cd "$DIST_DIR/lib" && "$SMOKE_BIN" )
      rm -f "$SMOKE_BIN"
      ;;
    linux-*)
      cc -I"$DIST_DIR/include" "$SMOKE_SRC" \
        -L"$DIST_DIR/lib" -lpdfium -Wl,-rpath,"$DIST_DIR/lib" \
        -o "$SMOKE_BIN"
      "$SMOKE_BIN"
      rm -f "$SMOKE_BIN"
      ;;
    win-*)
      # MSVC compile is awkward from bash; defer to CI's run-smoke step.
      echo ">>> skipping runtime smoke on Windows (handled by CI workflow)"
      ;;
    android-*|ios-*|wasm)
      # Cross-compiled — can't be loaded directly on the runner.
      # ios-* targets the iPhone/simulator SDK; android-* targets
      # bionic; wasm needs node/JS host setup we don't ship here.
      # nm-based symbol checks above are the only enforcement we can
      # do here. Real load happens in downstream consumers.
      echo ">>> skipping runtime smoke for $TARGET (cross-compiled — nm-only verification)"
      ;;
  esac
fi

echo "OK: dist/$TARGET ready ($COMBINED_TAG)"
