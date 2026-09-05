#!/bin/bash
# Launch reVC from the game data folder.
#
# Usage:
#   ./scripts/run.sh [GAME_DIR]
# Default game dir: ~/GTA-VC
set -e

GAME="${1:-$HOME/GTA-VC}"

if [ ! -f "$GAME/reVC" ]; then
  echo "reVC binary not found in '$GAME'." >&2
  echo "Build it first (./scripts/build_macos.sh) and copy bin/*/Debug/reVC into the game folder." >&2
  exit 1
fi

cd "$GAME"
exec ./reVC
