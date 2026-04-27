#!/usr/bin/env bash
# 01-fetch-pdfium.sh — gclient sync PDFium source at the pinned commit
# into ./pdfium/. Idempotent: re-runs are safe and only sync deltas.
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
VERSION="$(cat "$ROOT/PDFIUM_VERSION" | tr -d '[:space:]')"

if [ -z "$VERSION" ]; then
  echo "error: PDFIUM_VERSION is empty" >&2
  exit 1
fi

command -v gclient >/dev/null || { echo "error: depot_tools not on PATH (need gclient, gn, ninja)" >&2; exit 1; }
command -v gn      >/dev/null || { echo "error: 'gn' not on PATH" >&2; exit 1; }
command -v ninja   >/dev/null || { echo "error: 'ninja' not on PATH" >&2; exit 1; }
command -v git     >/dev/null || { echo "error: 'git' not on PATH" >&2; exit 1; }

mkdir -p "$ROOT/pdfium"
cd "$ROOT/pdfium"

if [ ! -f .gclient ]; then
  echo ">>> gclient config (first run)"
  gclient config --unmanaged --name=pdfium https://pdfium.googlesource.com/pdfium.git
fi

echo ">>> gclient sync to $VERSION"
# --reset wipes any local changes (including patches from a previous cached
# build) before syncing; --no-history keeps fetches shallow.
gclient sync --revision "pdfium@$VERSION" --no-history --reset

echo "OK: PDFium synced at $VERSION → $ROOT/pdfium/pdfium"
