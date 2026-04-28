#!/usr/bin/env bash
# 01-fetch-pdfium.sh [target] — gclient sync PDFium source at the pinned
# commit into ./pdfium/. Idempotent: re-runs are safe and only sync deltas.
#
# The optional |target| argument controls cross-compile setup:
#   * android-* → writes target_os = ['android'] into .gclient before sync
#                 so gclient hooks fetch the Android NDK toolchain, then
#                 runs build/install-build-deps.sh --android for apt deps.
#   * ios-*     → writes target_os = ['ios']; no extras needed.
#   * wasm      → writes target_os = ['emscripten'] and clones + activates
#                 emsdk EMSDK_VERSION (3.1.72 — pinned to bblanchon's
#                 verified-working version). Adds emcc/em++/llvm-nm to PATH.
#   * default   → host-OS build, no target_os, no extra setup.
#
# Cache-friendly: .gclient is rewritten on every run so cached pdfium/
# trees from a previous target switch over cleanly. This is cheap.
set -euo pipefail

ROOT="${PDFIUM_PATCHED_ROOT:-$(pwd)}"
VERSION="$(cat "$ROOT/PDFIUM_VERSION" | tr -d '[:space:]')"

if [ -z "$VERSION" ]; then
  echo "error: PDFIUM_VERSION is empty" >&2
  exit 1
fi

EMSDK_VERSION="${EMSDK_VERSION:-3.1.72}"

TARGET="${1:-}"
TARGET_OS=""
case "$TARGET" in
  android-*) TARGET_OS="android" ;;
  ios-*)     TARGET_OS="ios" ;;
  wasm)      TARGET_OS="emscripten" ;;
  # mac-*, linux-*, win-*, "" → host build, no target_os entry needed.
esac

command -v gclient >/dev/null || { echo "error: depot_tools not on PATH (need gclient, gn, ninja)" >&2; exit 1; }
command -v gn      >/dev/null || { echo "error: 'gn' not on PATH" >&2; exit 1; }
command -v ninja   >/dev/null || { echo "error: 'ninja' not on PATH" >&2; exit 1; }
command -v git     >/dev/null || { echo "error: 'git' not on PATH" >&2; exit 1; }

mkdir -p "$ROOT/pdfium"
cd "$ROOT/pdfium"

# Always rewrite .gclient — fast and deterministic. Lets a cached pdfium/
# tree switch target_os between runs without manual cleanup.
echo ">>> gclient config (target_os=${TARGET_OS:-default-host})"
rm -f .gclient
gclient config --unmanaged --name=pdfium https://pdfium.googlesource.com/pdfium.git
if [ -n "$TARGET_OS" ]; then
  echo "target_os = ['$TARGET_OS']" >> .gclient
fi

echo ">>> gclient sync to $VERSION"
# --reset wipes any local changes (including patches from a previous cached
# build) before syncing; --no-history keeps fetches shallow.
gclient sync --revision "pdfium@$VERSION" --no-history --reset

# Android cross-compile needs apt-installed sysroot tools. install-build-deps
# is part of chromium's //build checkout that gclient just fetched. Only
# meaningful on Linux runners — locally on macOS we'd never build android-*
# anyway, so skip with a warning.
if [ "$TARGET_OS" = "android" ]; then
  if [ "$(uname -s)" = "Linux" ]; then
    echo ">>> install Android cross-compile build deps"
    "$ROOT/pdfium/pdfium/build/install-build-deps.sh" --android --no-prompt
    # gclient runhooks re-evaluates DEPS hooks now that target_os=android is
    # set, fetching the Android NDK toolchain into pdfium/third_party.
    ( cd pdfium && gclient runhooks )
  else
    echo "warn: skipping Android build-deps install — host is $(uname -s), not Linux" >&2
  fi
fi

# WASM build needs the Emscripten SDK on PATH so the //build/toolchain/wasm
# gn toolchain can find emcc / em++ / llvm-nm. Install into the PDFium
# third_party tree (matches bblanchon's layout: emsdk_path resolves
# //third_party/emsdk relative to PDFium root).
if [ "$TARGET_OS" = "emscripten" ]; then
  EMSDK_DIR="$ROOT/pdfium/pdfium/third_party/emsdk"
  echo ">>> install Emscripten SDK $EMSDK_VERSION → $EMSDK_DIR"
  if [ ! -d "$EMSDK_DIR/.git" ]; then
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
  else
    git -C "$EMSDK_DIR" fetch --depth 1 origin main
    git -C "$EMSDK_DIR" reset --hard origin/main
  fi
  ( cd "$EMSDK_DIR" && ./emsdk install "$EMSDK_VERSION" && ./emsdk activate "$EMSDK_VERSION" )
  # Add emscripten tools to the workflow's PATH so 04-build.sh's em++
  # link step can invoke them. Locally, source emsdk/emsdk_env.sh.
  if [ -n "${GITHUB_PATH:-}" ]; then
    echo "$EMSDK_DIR/upstream/emscripten" >> "$GITHUB_PATH"
    echo "$EMSDK_DIR/upstream/bin" >> "$GITHUB_PATH"
  else
    echo "warn: GITHUB_PATH not set — for local wasm builds, run:"
    echo "      source $EMSDK_DIR/emsdk_env.sh"
  fi
fi

echo "OK: PDFium synced at $VERSION → $ROOT/pdfium/pdfium"
