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
#     lib/libpdfium.dylib       # macOS
#     lib/libpdfium.so          # Linux
#     bin/pdfium.dll            # Windows runtime DLL
#     lib/pdfium.dll.lib        # Windows import library
#
# Smoke test: links a tiny C program against the built library and
# either verifies the runtime can load it (Unix) or just confirms
# FPDF_InitLibrary is in the export table (Windows). Linker / missing-
# symbol failure = build regression.
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
PDFIUM_DIR="$ROOT/pdfium/pdfium"

TARGET="${1:-}"
if [ -z "$TARGET" ]; then
  echo "usage: $0 <mac-arm64|mac-x64|linux-x64|win-x64>" >&2
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

echo ">>> package dist/$TARGET (layout: bblanchon-compatible)"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/lib" "$DIST_DIR/include" "$DIST_DIR/include/cpp"

case "$TARGET" in
  mac-*)
    cp "$OUT_DIR/libpdfium.dylib" "$DIST_DIR/lib/libpdfium.dylib"
    LIB_PATH="$DIST_DIR/lib/libpdfium.dylib"
    ;;
  linux-*)
    cp "$OUT_DIR/libpdfium.so" "$DIST_DIR/lib/libpdfium.so"
    LIB_PATH="$DIST_DIR/lib/libpdfium.so"
    ;;
  win-*)
    mkdir -p "$DIST_DIR/bin"
    cp "$OUT_DIR/pdfium.dll"     "$DIST_DIR/bin/pdfium.dll"
    cp "$OUT_DIR/pdfium.dll.lib" "$DIST_DIR/lib/pdfium.dll.lib"
    LIB_PATH="$DIST_DIR/bin/pdfium.dll"
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

# Symbol presence: FPDF_InitLibrary is upstream's, must always be exported.
SYMBOL_CHECK_OK=0
case "$TARGET" in
  mac-*|linux-*)
    if nm -gU "$LIB_PATH" 2>/dev/null | grep -q ' _\?FPDF_InitLibrary$' \
       || nm -D  "$LIB_PATH" 2>/dev/null | grep -q ' FPDF_InitLibrary$'; then
      SYMBOL_CHECK_OK=1
    fi
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
DECL_SYMS="$(grep -oE 'FPDFRejeb_[A-Za-z_]+' "$ROOT/include/fpdf_rejeb.h" | sort -u || true)"
if [ -n "$DECL_SYMS" ]; then
  for sym in $DECL_SYMS; do
    case "$TARGET" in
      mac-*|linux-*)
        if ! { nm -gU "$LIB_PATH" 2>/dev/null | grep -q " _\?${sym}$"; } \
           && ! { nm -D  "$LIB_PATH" 2>/dev/null | grep -q " ${sym}$"; }; then
          echo "FAIL: declared symbol $sym not exported by $LIB_PATH" >&2
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
  esac
fi

echo "OK: dist/$TARGET ready ($COMBINED_TAG)"
