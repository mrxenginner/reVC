#!/bin/bash
# Build reVC for macOS (Apple Silicon) using Apple's clang.
# Handles the two gotchas that otherwise break a fresh build:
#   1) Homebrew LLVM on PATH breaks librw  -> force CC/CXX to Apple's clang.
#   2) src/extras/GitSHA1.cpp is gitignored/generated -> create it if missing.
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

CONFIG="${1:-debug_macosx-arm64-librw_gl3_glfw-oal}"

echo "== reVC macOS build -- config: $CONFIG =="

# 1) Ensure the premake build tree exists
if [ ! -f build/Makefile ]; then
  if ! command -v premake5 >/dev/null; then
    echo "premake5 not found. Install it:  brew install premake" >&2
    exit 1
  fi
  echo "== Generating premake build tree =="
  premake5 --with-librw gmake2
fi

# 2) Generate the git hash source file if missing
if [ ! -f src/extras/GitSHA1.cpp ]; then
  echo "== Generating src/extras/GitSHA1.cpp =="
  ./printHash.sh src/extras/GitSHA1.cpp
fi

# 3) Build (Apple clang only — Homebrew LLVM breaks librw)
echo "== make -j$(sysctl -n hw.ncpu) config=$CONFIG =="
cd build
make -j"$(sysctl -n hw.ncpu)" CC=/usr/bin/clang CXX=/usr/bin/clang++ config="$CONFIG"

if [[ "$CONFIG" == debug_* ]]; then BUILDCFG=Debug
elif [[ "$CONFIG" == release_* ]]; then BUILDCFG=Release
else BUILDCFG="?"
fi
PLATFORM="${CONFIG#debug_}"
PLATFORM="${PLATFORM#release_}"
echo
echo "Done. Binary: $ROOT/bin/$PLATFORM/$BUILDCFG/reVC"
