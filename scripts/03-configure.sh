#!/usr/bin/env bash
# 03-configure.sh <target> — gn gen the build directory using
# args/<target>.args.gn. Idempotent: re-running just regenerates ninja files.
#
# target ∈ {mac-arm64, mac-x64, linux-x64, win-x64}
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
PDFIUM_DIR="$ROOT/pdfium/pdfium"

TARGET="${1:-}"
if [ -z "$TARGET" ]; then
  echo "usage: $0 <mac-arm64|mac-x64|linux-x64|win-x64>" >&2
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
# Nuke any cached out/ dir from a previous build — keeps the source-tree
# cache fast (gclient sync skipped on cache hit) without inheriting stale
# build.ninja / .o / dylib artifacts from a build that ran without our
# current patches. Without this, ninja can re-link a cached dylib that
# silently lacks symbols introduced by patches added in this run.
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
cp "$ARGS_FILE" "$OUT_DIR/args.gn"

cd "$PDFIUM_DIR"

echo ">>> gn gen out/$TARGET (clean)"
gn gen "out/$TARGET"

echo "OK: configured $TARGET → $OUT_DIR"
