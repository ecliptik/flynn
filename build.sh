#!/bin/bash
# Build Flynn for classic Macintosh using Retro68 toolchain
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLCHAIN="$SCRIPT_DIR/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake"
BUILD_DIR="$SCRIPT_DIR/build"

if [ ! -f "$TOOLCHAIN" ]; then
    echo "Error: Retro68 toolchain not found at $TOOLCHAIN"
    echo "Build it first:"
    echo "  cd Retro68-build && bash ../Retro68/build-toolchain.bash --no-ppc --no-carbon --prefix=\$(pwd)/toolchain"
    exit 1
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$SCRIPT_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
make "$@"

echo ""
echo "Build complete. Output:"
ls -la "$BUILD_DIR"/Flynn.* 2>/dev/null || echo "  (no output files found)"
