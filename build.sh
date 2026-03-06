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

# Generate BinHex (.hqx) archive if macutils is available
if command -v binhex >/dev/null 2>&1; then
    binhex "$BUILD_DIR/Flynn.bin" > "$BUILD_DIR/Flynn.hqx"
    echo "BinHex archive created: Flynn.hqx"
else
    echo "Note: Install macutils for BinHex output: sudo apt install macutils"
fi

echo ""
echo "Build complete. Output:"
ls -la "$BUILD_DIR"/Flynn.* 2>/dev/null || echo "  (no output files found)"

# Convert Read Me line endings to Mac CR format
README_SRC="$SCRIPT_DIR/docs/Flynn Read Me"
README_OUT="$BUILD_DIR/Flynn Read Me"
if [ -f "$README_SRC" ]; then
    tr '\n' '\r' < "$README_SRC" > "$README_OUT"
    echo "  Read Me: $README_OUT"
fi

echo ""
echo "To deploy to HFS image:"
echo "  hmount diskimages/snow-sys608.img"
echo "  hmkdir :Flynn"
echo "  hcopy -m build/Flynn.bin ':Flynn:Flynn'"
echo "  hattrib -t APPL -c FLYN ':Flynn:Flynn'"
echo "  hcopy -r 'build/Flynn Read Me' ':Flynn:Flynn Read Me'"
echo "  hattrib -t TEXT -c ttxt ':Flynn:Flynn Read Me'"
echo "  humount"
