#!/usr/bin/env bash
# 02-apply-patches.sh — reset PDFium to clean upstream state, apply
# infra/ patches (always), then patches/ feature patches in lexical
# order. Finally, copy our public header into PDFium's public/ dir.
#
# Idempotent: re-running is safe but destroys any manual edits inside
# pdfium/pdfium/ via git reset --hard + git clean -fd.
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
PDFIUM_DIR="$ROOT/pdfium/pdfium"

if [ ! -d "$PDFIUM_DIR" ]; then
  echo "error: $PDFIUM_DIR does not exist — run scripts/01-fetch-pdfium.sh first" >&2
  exit 1
fi

cd "$PDFIUM_DIR"

echo ">>> reset PDFium working tree to clean state"
git reset --hard HEAD
git clean -fd

apply_patches() {
  local label="$1" dir="$2"
  shopt -s nullglob
  local patches=("$dir"/*.patch)
  shopt -u nullglob
  if [ "${#patches[@]}" -eq 0 ]; then
    echo ">>> no $label patches"
    return
  fi
  for patch in "${patches[@]}"; do
    echo ">>> applying $label/$(basename "$patch")"
    git apply --check "$patch"
    git apply "$patch"
  done
}

# Infra patches first (build-pipeline fixups; not counted in PATCHES_VERSION).
apply_patches infra "$ROOT/infra"

# Feature patches in lexical order (one FPDFRejeb_* concept per patch).
apply_patches features "$ROOT/patches"

echo ">>> drop fpdf_rejeb.h into PDFium public/"
cp "$ROOT/include/fpdf_rejeb.h" "$PDFIUM_DIR/public/fpdf_rejeb.h"

echo "OK: patches applied"
