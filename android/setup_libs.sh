#!/usr/bin/env bash
# Setup prebuilt native libraries for Android build.
# Copies .so files from vendor/ to jniLibs/ for Gradle packaging.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$SCRIPT_DIR/launcher/app"
VENDOR_DIR="$SCRIPT_DIR/../vendor"

abis=("arm64-v8a" "armeabi-v7a")

for abi in "${abis[@]}"; do
    mkdir -p "$APP_DIR/jniLibs/$abi"

    cp "$VENDOR_DIR/sdl2/libs/Android/$abi/libSDL2.so"       "$APP_DIR/jniLibs/$abi/"
    cp "$VENDOR_DIR/openal-soft/libs/Android/$abi/libopenal.so" "$APP_DIR/jniLibs/$abi/"
    cp "$VENDOR_DIR/mpg123/lib/Android/$abi/libmpg123.so"     "$APP_DIR/jniLibs/$abi/"

    echo "✔ $abi: libSDL2.so, libopenal.so, libmpg123.so"
done

echo "Done. Prebuilt libs copied to jniLibs/"
