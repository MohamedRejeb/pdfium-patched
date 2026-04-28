#!/usr/bin/env bash
# 02-apply-patches.sh — reset PDFium to clean upstream state, apply
# infra/ patches (always), then patches/ feature patches in lexical
# order. Finally, copy our public header into PDFium's public/ dir.
#
# Patches are applied from $PDFIUM_DIR by default. A patch can opt into
# being applied from a sub-checkout (e.g. //build, which is its own
# gclient-managed git repo) by including a header line of the form:
#
#   # Apply-from: <subdir>
#
# The android infra patch uses this to land in pdfium/build/.
#
# Idempotent: re-running is safe but destroys any manual edits inside
# pdfium/pdfium/ (and the listed sub-checkouts) via git reset --hard.
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
PDFIUM_DIR="$ROOT/pdfium/pdfium"

if [ ! -d "$PDFIUM_DIR" ]; then
  echo "error: $PDFIUM_DIR does not exist — run scripts/01-fetch-pdfium.sh first" >&2
  exit 1
fi

# Reset every git checkout we might patch. //build is gclient-managed
# alongside pdfium/, so a previous infra patch on it survives a sync
# unless we hard-reset it explicitly.
reset_checkout() {
  local dir="$1"
  if [ -d "$dir/.git" ] || [ -f "$dir/.git" ]; then
    echo ">>> reset $dir to clean state"
    git -C "$dir" reset --hard HEAD
    git -C "$dir" clean -fd
  fi
}
reset_checkout "$PDFIUM_DIR"
reset_checkout "$PDFIUM_DIR/build"

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
    # Optional `# Apply-from: <subdir>` header tells us which checkout
    # the patch targets (relative to PDFIUM_DIR). Default: PDFIUM_DIR itself.
    # Anchor to end-of-line so we don't pick up literal "Apply-from:"
    # phrases buried in surrounding prose comments.
    local apply_from
    apply_from="$(grep -m1 -oE '^# Apply-from: [a-zA-Z0-9_/.-]+$' "$patch" | awk '{print $3}' || true)"
    local apply_dir="$PDFIUM_DIR"
    [ -n "$apply_from" ] && apply_dir="$PDFIUM_DIR/$apply_from"

    if [ ! -d "$apply_dir" ]; then
      echo "error: patch $(basename "$patch") wants Apply-from=$apply_from but $apply_dir is missing" >&2
      exit 1
    fi

    local abs_patch
    abs_patch="$(cd "$(dirname "$patch")" && pwd)/$(basename "$patch")"
    local tag="$label/$(basename "$patch")"
    [ -n "$apply_from" ] && tag="$tag (in $apply_from/)"
    echo ">>> applying $tag"
    ( cd "$apply_dir" && git apply --check "$abs_patch" )
    ( cd "$apply_dir" && git apply "$abs_patch" )
  done
}

# Infra patches first (build-pipeline fixups; not counted in PATCHES_VERSION).
apply_patches infra "$ROOT/infra"

# Feature patches in lexical order (one FPDFRejeb_* concept per patch).
apply_patches features "$ROOT/patches"

echo ">>> drop fpdf_rejeb.h into PDFium public/"
cp "$ROOT/include/fpdf_rejeb.h" "$PDFIUM_DIR/public/fpdf_rejeb.h"

# Wasm build config: infra/0006-wasm-build.patch references the
# //build/config/wasm:compiler target which doesn't exist upstream.
# Drop our small BUILD.gn defining it. Harmless on non-wasm builds —
# nothing pulls it in unless target_os=emscripten.
WASM_CONFIG_SRC="$ROOT/infra/wasm-config.gn"
WASM_CONFIG_DEST="$PDFIUM_DIR/build/config/wasm/BUILD.gn"
if [ -f "$WASM_CONFIG_SRC" ]; then
  echo ">>> drop infra/wasm-config.gn → build/config/wasm/BUILD.gn"
  mkdir -p "$(dirname "$WASM_CONFIG_DEST")"
  cp "$WASM_CONFIG_SRC" "$WASM_CONFIG_DEST"
fi

echo "OK: patches applied"
