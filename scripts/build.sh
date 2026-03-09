#!/bin/bash
# Build Flynn for classic Macintosh using Retro68 toolchain
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TOOLCHAIN="$SCRIPT_DIR/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake"
BUILD_DIR="$SCRIPT_DIR/build"

if [ ! -f "$TOOLCHAIN" ]; then
    echo "Error: Retro68 toolchain not found at $TOOLCHAIN"
    echo "Build it first:"
    echo "  cd Retro68-build && bash ../Retro68/build-toolchain.bash --no-ppc --no-carbon --prefix=\$(pwd)/toolchain"
    exit 1
fi

# Read version from CMakeLists.txt
VERSION=$(grep -oP 'project\(Flynn VERSION \K[0-9]+\.[0-9]+\.[0-9]+' "$SCRIPT_DIR/CMakeLists.txt")
if [ -z "$VERSION" ]; then
    echo "Warning: Could not read version from CMakeLists.txt, using 'unknown'"
    VERSION="unknown"
fi

# Compute display version:
#   Tagged release (v1.1.0 on HEAD) → "1.1.0" / artifacts: Flynn-1.1.0.*
#   Dev build (no tag)              → "d724f2b" / artifacts: Flynn-d724f2b.*
SHORT_SHA=$(git -C "$SCRIPT_DIR" rev-parse --short HEAD 2>/dev/null || echo "")
GIT_TAG=$(git -C "$SCRIPT_DIR" tag --points-at HEAD 2>/dev/null | grep -x "v${VERSION}" || true)
if [ -n "$GIT_TAG" ]; then
    VERSION_DISPLAY="${VERSION}"
elif [ -n "$SHORT_SHA" ]; then
    VERSION_DISPLAY="${SHORT_SHA}"
else
    VERSION_DISPLAY="${VERSION}"
fi

# Stamp version into resource file for About dialog before building
REZ_FILE="$SCRIPT_DIR/resources/telnet.r"
REZ_BACKUP="$BUILD_DIR/.telnet.r.bak"
mkdir -p "$BUILD_DIR"
cp "$REZ_FILE" "$REZ_BACKUP"
sed -i "s/\"Version ${VERSION}\"/\"Version ${VERSION_DISPLAY}\"/" "$REZ_FILE"

# Build (restore .r file on exit, even if build fails)
cleanup() { cp "$REZ_BACKUP" "$REZ_FILE" 2>/dev/null; rm -f "$REZ_BACKUP"; }
trap cleanup EXIT

cd "$BUILD_DIR"
cmake "$SCRIPT_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=MinSizeRel
make "$@"

# Fix creator code in MacBinary header (Retro68 sets '????' instead of 'FLYN')
# Then recalculate MacBinary II CRC-16 (XMODEM) over header bytes 0-123
printf 'FLYN' | dd of="$BUILD_DIR/Flynn.bin" bs=1 seek=69 count=4 conv=notrunc 2>/dev/null
python3 -c "
import struct
with open('$BUILD_DIR/Flynn.bin', 'r+b') as f:
    hdr = bytearray(f.read(128))
    crc = 0
    for b in hdr[:124]:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xFFFF
    f.seek(124)
    f.write(struct.pack('>H', crc))
"

# Generate BinHex (.hqx) archive if macutils is available
if command -v binhex >/dev/null 2>&1; then
    binhex "$BUILD_DIR/Flynn.bin" > "$BUILD_DIR/Flynn.hqx"
    echo "BinHex archive created: Flynn.hqx"
else
    echo "Note: Install macutils for BinHex output: sudo apt install macutils"
fi

# Convert About Flynn line endings to Mac CR format, stamping display version
ABOUT_SRC="$SCRIPT_DIR/docs/About Flynn"
ABOUT_OUT="$BUILD_DIR/About Flynn"
if [ -f "$ABOUT_SRC" ]; then
    sed "s/Version ${VERSION}/Version ${VERSION_DISPLAY}/" "$ABOUT_SRC" | tr '\n' '\r' > "$ABOUT_OUT"
fi

# Post-process 800K floppy image: set creator code and add About Flynn
if [ -f "$BUILD_DIR/Flynn.dsk" ]; then
    hmount "$BUILD_DIR/Flynn.dsk"
    hattrib -t APPL -c FLYN :Flynn
    if [ -f "$ABOUT_OUT" ]; then
        hcopy -r "$ABOUT_OUT" ":About Flynn"
        hattrib -t ttro -c ttxt ":About Flynn"
    fi
    humount
fi

# Create versioned copies (use display version with SHA for non-tagged builds)
cp "$BUILD_DIR/Flynn.bin" "$BUILD_DIR/Flynn-${VERSION_DISPLAY}.bin"
cp "$BUILD_DIR/Flynn.dsk" "$BUILD_DIR/Flynn-${VERSION_DISPLAY}.dsk"
[ -f "$BUILD_DIR/Flynn.hqx" ] && cp "$BUILD_DIR/Flynn.hqx" "$BUILD_DIR/Flynn-${VERSION_DISPLAY}.hqx"

echo ""
echo "Build complete (v${VERSION_DISPLAY}):"
ls -la "$BUILD_DIR"/Flynn-${VERSION_DISPLAY}.* 2>/dev/null
[ -f "$ABOUT_OUT" ] && echo "  About Flynn included in disk image"

echo ""
echo "To deploy to HFS image:"
echo "  hmount diskimages/snow-sys608.img"
echo "  hmkdir :Flynn"
echo "  hcopy -m build/Flynn.bin ':Flynn:Flynn'"
echo "  hattrib -t APPL -c FLYN ':Flynn:Flynn'"
echo "  hcopy -r 'build/About Flynn' ':Flynn:About Flynn'"
echo "  hattrib -t ttro -c ttxt ':Flynn:About Flynn'"
echo "  humount"
