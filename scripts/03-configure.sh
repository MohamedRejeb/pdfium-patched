#!/usr/bin/env bash
# 03-configure.sh <target> — gn gen the build directory using
# args/<target>.args.gn. Idempotent: re-running just regenerates ninja files.
#
# target ∈ {mac-arm64, mac-x64, linux-x64, win-x64,
#           android-arm, android-arm64, android-x86, android-x64}
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
PDFIUM_DIR="$ROOT/pdfium/pdfium"

TARGET="${1:-}"
if [ -z "$TARGET" ]; then
  echo "usage: $0 <mac-arm64|mac-x64|linux-x64|win-x64|android-arm|android-arm64|android-x86|android-x64>" >&2
  exit 1
fi

ARGS_FILE="$ROOT/args/$TARGET.args.gn"
if [ ! -f "$ARGS_FILE" ]; then
  echo "error: $ARGS_FILE does not exist" >&2
  exit 1
fi

if [ ! -d "$PDFIUM_DIR" ]; then
  echo "error: $PDFIUM_DIR missing — run scripts/01-fetch-pdfium.sh + 02-apply-patches.sh first" >&2
  exit 1
fi

OUT_DIR="$PDFIUM_DIR/out/$TARGET"
mkdir -p "$OUT_DIR"
cp "$ARGS_FILE" "$OUT_DIR/args.gn"

cd "$PDFIUM_DIR"

echo ">>> gn gen out/$TARGET"
gn gen "out/$TARGET"

echo "OK: configured $TARGET → $OUT_DIR"
