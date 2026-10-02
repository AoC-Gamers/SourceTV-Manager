#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${DEPS_DIR:-$ROOT_DIR/.deps/exts-linux}"
VENV_DIR="${VENV_DIR:-$DEPS_DIR/.venv}"

clone_dependency() {
  local url="$1"
  local branch="$2"
  local target="$3"

  if [[ -e "$target" ]]; then
    echo "Dependency path already exists; keeping it untouched: $target" >&2
    echo "Choose a fresh DEPS_DIR or remove that generated dependency directory." >&2
    exit 1
  fi

  git clone --branch "$branch" --single-branch "$url" "$target"
}

mkdir -p "$DEPS_DIR"
if [[ -f "$ROOT_DIR/.gitmodules" ]]; then
  git -C "$ROOT_DIR" submodule sync --recursive
  git -C "$ROOT_DIR" submodule update --init --recursive
fi

clone_dependency https://github.com/alliedmodders/hl2sdk.git l4d2 "$DEPS_DIR/hl2sdk-l4d2"
clone_dependency https://github.com/alliedmodders/sourcemod.git 1.12-dev "$DEPS_DIR/sourcemod-1.12"
clone_dependency https://github.com/alliedmodders/metamod-source.git 1.12-dev "$DEPS_DIR/mmsource-1.12"
clone_dependency https://github.com/alliedmodders/ambuild.git master "$DEPS_DIR/ambuild"

git -C "$DEPS_DIR/sourcemod-1.12" submodule update --init --recursive
git -C "$DEPS_DIR/mmsource-1.12" submodule update --init --recursive

python3 -m venv "$VENV_DIR"
"$VENV_DIR/bin/python" -m pip install --upgrade pip
"$VENV_DIR/bin/python" -m pip install "$DEPS_DIR/ambuild"

cat <<EOF
Dependencies ready.
DEPS_DIR=$DEPS_DIR
HL2SDK_DIR=$DEPS_DIR/hl2sdk-l4d2
SOURCEMOD_DIR=$DEPS_DIR/sourcemod-1.12
MMSOURCE_DIR=$DEPS_DIR/mmsource-1.12
VENV_DIR=$VENV_DIR
EOF
