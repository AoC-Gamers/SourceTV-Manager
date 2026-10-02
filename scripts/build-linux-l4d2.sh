#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${DEPS_DIR:-$ROOT_DIR/.deps/exts-linux}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/.build/linux-l4d2}"
HL2SDK_DIR="${HL2SDK_DIR:-$DEPS_DIR/hl2sdk-l4d2}"
SOURCEMOD_DIR="${SOURCEMOD_DIR:-$DEPS_DIR/sourcemod-1.12}"
MMSOURCE_DIR="${MMSOURCE_DIR:-$DEPS_DIR/mmsource-1.12}"
VENV_DIR="${VENV_DIR:-$DEPS_DIR/.venv}"

if [[ "$(uname -s)" != Linux ]]; then
  echo "This build script requires Linux." >&2
  exit 1
fi

for required_dir in "$HL2SDK_DIR" "$SOURCEMOD_DIR" "$MMSOURCE_DIR"; do
  if [[ ! -d "$required_dir" ]]; then
    echo "Missing required directory: $required_dir. Run make deps-linux or set its path." >&2
    exit 1
  fi
done

if [[ ! -f "$ROOT_DIR/safetyhook/AMBuilder" ]]; then
  echo "Missing SafetyHook submodule. Run make deps-linux to initialize it." >&2
  exit 1
fi

if [[ ! -x "$VENV_DIR/bin/python" || ! -x "$VENV_DIR/bin/ambuild" ]]; then
  echo "Missing AMBuild environment at $VENV_DIR. Run make deps-linux or set VENV_DIR." >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" >/dev/null
"$VENV_DIR/bin/python" "$ROOT_DIR/configure.py" \
  --sdks l4d2 \
  --targets x86 \
  --hl2sdk-root "$(dirname "$HL2SDK_DIR")" \
  --mms-path "$MMSOURCE_DIR" \
  --sm-path "$SOURCEMOD_DIR"

if [[ ! -f "$BUILD_DIR/.ambuild2/vars" ]]; then
  echo "AMBuild configure did not create $BUILD_DIR/.ambuild2/vars." >&2
  exit 1
fi

"$VENV_DIR/bin/ambuild"
popd >/dev/null

EXT_BIN="$BUILD_DIR/package/addons/sourcemod/extensions/sourcetvmanager.ext.2.l4d2.so"
if [[ ! -f "$EXT_BIN" ]]; then
  echo "Build completed but the expected extension was not found: $EXT_BIN" >&2
  exit 1
fi

printf 'Build complete.\nBUILD_DIR=%s\nEXTENSION=%s\n' "$BUILD_DIR" "$EXT_BIN"
