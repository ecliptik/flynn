#!/bin/bash
# Build Flynn for classic Macintosh using Retro68 toolchain
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TOOLCHAIN="$SCRIPT_DIR/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake"
BUILD_DIR="$SCRIPT_DIR/build"

# --- Feature flag defaults (= full preset) ---
FLYNN_MAX_SESSIONS=4
FLYNN_SCROLLBACK=192
FLYNN_FINGER=ON
FLYNN_COLOR=ON
FLYNN_GLYPHS=ON
FLYNN_CP437=ON
FLYNN_CLIPBOARD=ON
FLYNN_SAVEFILE=ON
FLYNN_LOGGING=ON
FLYNN_PRINTING=ON
FLYNN_FAVORITES=ON
FLYNN_DARK_MODE=ON
FLYNN_DBLWIDTH=ON
FLYNN_ALT_SCREEN=ON
FLYNN_OFFSCREEN=ON
FLYNN_STATUS_BAR=ON
FLYNN_CURSOR_STYLES=ON
FLYNN_TAB_STOPS=ON

PRESET=""

# --- Apply preset ---
apply_preset() {
    case "$1" in
        minimal)
            FLYNN_MAX_SESSIONS=1
            FLYNN_SCROLLBACK=0
            FLYNN_FINGER=OFF
            FLYNN_COLOR=OFF
            FLYNN_GLYPHS=OFF
            FLYNN_CP437=OFF
            FLYNN_CLIPBOARD=ON
            FLYNN_SAVEFILE=OFF
            FLYNN_LOGGING=OFF
            FLYNN_PRINTING=OFF
            FLYNN_FAVORITES=OFF
            FLYNN_DARK_MODE=OFF
            FLYNN_DBLWIDTH=OFF
            FLYNN_ALT_SCREEN=ON
            FLYNN_OFFSCREEN=OFF
            FLYNN_STATUS_BAR=OFF
            FLYNN_CURSOR_STYLES=OFF
            FLYNN_TAB_STOPS=OFF
            ;;
        lite|macplus)
            # "macplus" is a legacy alias for "lite"
            FLYNN_MAX_SESSIONS=1
            FLYNN_SCROLLBACK=96
            FLYNN_FINGER=ON
            FLYNN_COLOR=OFF
            FLYNN_GLYPHS=ON
            FLYNN_CP437=ON
            FLYNN_CLIPBOARD=ON
            FLYNN_SAVEFILE=ON
            FLYNN_LOGGING=ON
            FLYNN_PRINTING=ON
            FLYNN_FAVORITES=ON
            FLYNN_DARK_MODE=ON
            FLYNN_DBLWIDTH=ON
            FLYNN_ALT_SCREEN=ON
            FLYNN_OFFSCREEN=ON
            FLYNN_STATUS_BAR=ON
            FLYNN_CURSOR_STYLES=ON
            FLYNN_TAB_STOPS=ON
            ;;
        full|default)
            # Already the defaults, but set explicitly for clarity
            FLYNN_MAX_SESSIONS=4
            FLYNN_SCROLLBACK=192
            FLYNN_FINGER=ON
            FLYNN_COLOR=ON
            FLYNN_GLYPHS=ON
            FLYNN_CP437=ON
            FLYNN_CLIPBOARD=ON
            FLYNN_SAVEFILE=ON
            FLYNN_LOGGING=ON
            FLYNN_PRINTING=ON
            FLYNN_FAVORITES=ON
            FLYNN_DARK_MODE=ON
            FLYNN_DBLWIDTH=ON
            FLYNN_ALT_SCREEN=ON
            FLYNN_OFFSCREEN=ON
            FLYNN_STATUS_BAR=ON
            FLYNN_CURSOR_STYLES=ON
            FLYNN_TAB_STOPS=ON
            ;;
        *)
            echo "Error: unknown preset '$1' (valid: minimal, lite, full; macplus is alias for lite)"
            exit 1
            ;;
    esac
}

# --- Parse command-line flags ---
# Order: defaults → preset → individual overrides
MAKE_ARGS=()
ARGS=("$@")
i=0
# First pass: find and apply preset
while [ $i -lt ${#ARGS[@]} ]; do
    case "${ARGS[$i]}" in
        --preset)
            PRESET="${ARGS[$((i+1))]}"
            apply_preset "$PRESET"
            i=$((i + 2))
            ;;
        *)
            i=$((i + 1))
            ;;
    esac
done

# Second pass: apply individual overrides
while [[ $# -gt 0 ]]; do
    case $1 in
        --preset)
            shift 2  # already handled
            ;;
        --sessions)
            FLYNN_MAX_SESSIONS="$2"
            if ! [[ "$FLYNN_MAX_SESSIONS" =~ ^[1-4]$ ]]; then
                echo "Error: --sessions must be 1-4"
                exit 1
            fi
            shift 2
            ;;
        --scrollback)
            FLYNN_SCROLLBACK="$2"
            if ! [[ "$FLYNN_SCROLLBACK" =~ ^[0-9]+$ ]] || [ "$FLYNN_SCROLLBACK" -gt 256 ]; then
                echo "Error: --scrollback must be 0-256"
                exit 1
            fi
            shift 2
            ;;
        --finger)        FLYNN_FINGER=ON;        shift ;;
        --no-finger)     FLYNN_FINGER=OFF;       shift ;;
        --color)         FLYNN_COLOR=ON;          shift ;;
        --no-color)      FLYNN_COLOR=OFF;         shift ;;
        --glyphs)        FLYNN_GLYPHS=ON;         shift ;;
        --no-glyphs)     FLYNN_GLYPHS=OFF;        shift ;;
        --cp437)         FLYNN_CP437=ON;           shift ;;
        --no-cp437)      FLYNN_CP437=OFF;          shift ;;
        --clipboard)     FLYNN_CLIPBOARD=ON;       shift ;;
        --no-clipboard)  FLYNN_CLIPBOARD=OFF;      shift ;;
        --savefile)      FLYNN_SAVEFILE=ON;         shift ;;
        --no-savefile)   FLYNN_SAVEFILE=OFF;        shift ;;
        --logging)       FLYNN_LOGGING=ON;          shift ;;
        --no-logging)    FLYNN_LOGGING=OFF;         shift ;;
        --printing)      FLYNN_PRINTING=ON;         shift ;;
        --no-printing)   FLYNN_PRINTING=OFF;        shift ;;
        --favorites)     FLYNN_FAVORITES=ON;        shift ;;
        --no-favorites)  FLYNN_FAVORITES=OFF;       shift ;;
        --bookmarks)     FLYNN_FAVORITES=ON;        shift ;;  # legacy alias
        --no-bookmarks)  FLYNN_FAVORITES=OFF;       shift ;;  # legacy alias
        --darkmode)      FLYNN_DARK_MODE=ON;        shift ;;
        --no-darkmode)   FLYNN_DARK_MODE=OFF;       shift ;;
        --dblwidth)      FLYNN_DBLWIDTH=ON;         shift ;;
        --no-dblwidth)   FLYNN_DBLWIDTH=OFF;        shift ;;
        --altscreen)     FLYNN_ALT_SCREEN=ON;       shift ;;
        --no-altscreen)  FLYNN_ALT_SCREEN=OFF;      shift ;;
        --offscreen)     FLYNN_OFFSCREEN=ON;         shift ;;
        --no-offscreen)  FLYNN_OFFSCREEN=OFF;        shift ;;
        --statusbar)     FLYNN_STATUS_BAR=ON;        shift ;;
        --no-statusbar)  FLYNN_STATUS_BAR=OFF;       shift ;;
        --cursorstyles)  FLYNN_CURSOR_STYLES=ON;     shift ;;
        --no-cursorstyles) FLYNN_CURSOR_STYLES=OFF;  shift ;;
        --tabstops)      FLYNN_TAB_STOPS=ON;         shift ;;
        --no-tabstops)   FLYNN_TAB_STOPS=OFF;        shift ;;
        *)
            MAKE_ARGS+=("$1")
            shift
            ;;
    esac
done

# --- Dependency resolution ---
# DBLWIDTH requires ALT_SCREEN (for alt_line_attr save/restore)
if [ "$FLYNN_DBLWIDTH" = "ON" ] && [ "$FLYNN_ALT_SCREEN" = "OFF" ]; then
    echo "Note: --dblwidth requires --altscreen, enabling it"
    FLYNN_ALT_SCREEN=ON
fi

# CP437 requires GLYPHS (CP437 table entries reference glyph IDs)
if [ "$FLYNN_CP437" = "ON" ] && [ "$FLYNN_GLYPHS" = "OFF" ]; then
    echo "Note: --cp437 requires --glyphs for full rendering, enabling it"
    FLYNN_GLYPHS=ON
fi

# --- Compute SIZE resource partition ---
compute_size() {
    local base=150
    local sessions=$FLYNN_MAX_SESSIONS
    local scrollback_kb=$(( (FLYNN_SCROLLBACK * 132 * 2 + 1023) / 1024 ))

    # Per-session cost (TermCell buffers are inline in Session struct)
    local session_kb=$(( 13 + 13 + 13 + scrollback_kb ))
    [ "$FLYNN_ALT_SCREEN" = "ON" ] && session_kb=$(( session_kb + 13 ))

    # Color per-session cost: CellColor arrays allocated via NewPtr
    # on System 7 (screen_color: always, sb_color: lazy on scroll)
    if [ "$FLYNN_COLOR" = "ON" ]; then
        session_kb=$(( session_kb + 13 ))                  # screen_color (always)
        session_kb=$(( session_kb + scrollback_kb ))        # sb_color (lazy but common)
        [ "$FLYNN_ALT_SCREEN" = "ON" ] && session_kb=$(( session_kb + 13 ))  # alt_color
    fi

    # Shared costs
    local shared=0
    [ "$FLYNN_OFFSCREEN" = "ON" ] && shared=$(( shared + 18 ))
    [ "$FLYNN_FINGER" = "ON" ] && shared=$(( shared + 2 ))
    [ "$FLYNN_GLYPHS" = "ON" ] && shared=$(( shared + 4 ))
    [ "$FLYNN_CP437" = "ON" ] && shared=$(( shared + 1 ))
    [ "$FLYNN_FAVORITES" = "ON" ] && shared=$(( shared + 3 ))
    [ "$FLYNN_CLIPBOARD" = "ON" ] && shared=$(( shared + 1 ))
    [ "$FLYNN_SAVEFILE" = "ON" ] && shared=$(( shared + 1 ))
    [ "$FLYNN_LOGGING" = "ON" ] && shared=$(( shared + 1 ))
    [ "$FLYNN_PRINTING" = "ON" ] && shared=$(( shared + 2 ))
    [ "$FLYNN_COLOR" = "ON" ] && shared=$(( shared + 1 ))
    [ "$FLYNN_DBLWIDTH" = "ON" ] && shared=$(( shared + 1 ))
    [ "$FLYNN_STATUS_BAR" = "ON" ] && shared=$(( shared + 1 ))

    # Color GWorld offscreen: ~130KB for 80x24 default window at 8bpp
    if [ "$FLYNN_COLOR" = "ON" ] && [ "$FLYNN_OFFSCREEN" = "ON" ]; then
        shared=$(( shared + 130 ))
    fi

    # Total with 30% headroom
    local computed=$(( base + shared + session_kb * sessions ))
    SIZE_PREFERRED=$(( computed * 130 / 100 ))
    SIZE_MINIMUM=$(( SIZE_PREFERRED - 64 ))

    # Clamp (color builds need larger partitions for CellColor
    # arrays + GWorld; safe on System 7 Macs with 4MB+ RAM)
    [ $SIZE_PREFERRED -lt 256 ] && SIZE_PREFERRED=256 || true
    [ $SIZE_MINIMUM -lt 192 ] && SIZE_MINIMUM=192 || true
    [ $SIZE_PREFERRED -gt 1536 ] && SIZE_PREFERRED=1536 || true
    [ $SIZE_MINIMUM -gt 1472 ] && SIZE_MINIMUM=1472 || true
}

compute_size

# --- Toolchain check ---
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
sed -i "s/\"Flynn ${VERSION}\"/\"Flynn ${VERSION_DISPLAY}\"/" "$REZ_FILE"

# Stamp SIZE resource partition
sed -i "s/768 \* 1024/${SIZE_PREFERRED} * 1024/" "$REZ_FILE"
sed -i "s/640 \* 1024/${SIZE_MINIMUM} * 1024/" "$REZ_FILE"

# Build (restore .r file on exit, even if build fails)
cleanup() { cp "$REZ_BACKUP" "$REZ_FILE" 2>/dev/null; rm -f "$REZ_BACKUP"; }
trap cleanup EXIT

cd "$BUILD_DIR"
cmake "$SCRIPT_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DFLYNN_MAX_SESSIONS="$FLYNN_MAX_SESSIONS" \
    -DFLYNN_SCROLLBACK="$FLYNN_SCROLLBACK" \
    -DFLYNN_FINGER="$FLYNN_FINGER" \
    -DFLYNN_COLOR="$FLYNN_COLOR" \
    -DFLYNN_GLYPHS="$FLYNN_GLYPHS" \
    -DFLYNN_CP437="$FLYNN_CP437" \
    -DFLYNN_CLIPBOARD="$FLYNN_CLIPBOARD" \
    -DFLYNN_SAVEFILE="$FLYNN_SAVEFILE" \
    -DFLYNN_LOGGING="$FLYNN_LOGGING" \
    -DFLYNN_PRINTING="$FLYNN_PRINTING" \
    -DFLYNN_FAVORITES="$FLYNN_FAVORITES" \
    -DFLYNN_DARK_MODE="$FLYNN_DARK_MODE" \
    -DFLYNN_DBLWIDTH="$FLYNN_DBLWIDTH" \
    -DFLYNN_ALT_SCREEN="$FLYNN_ALT_SCREEN" \
    -DFLYNN_OFFSCREEN="$FLYNN_OFFSCREEN" \
    -DFLYNN_STATUS_BAR="$FLYNN_STATUS_BAR" \
    -DFLYNN_CURSOR_STYLES="$FLYNN_CURSOR_STYLES" \
    -DFLYNN_TAB_STOPS="$FLYNN_TAB_STOPS"
make "${MAKE_ARGS[@]}"

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

# --- Determine file prefix from preset ---
PRESET_LABEL="${PRESET:-full}"
# Normalize macplus → lite for display/filenames
[ "$PRESET_LABEL" = "macplus" ] && PRESET_LABEL="lite"
# Normalize default → full for display/filenames
[ "$PRESET_LABEL" = "default" ] && PRESET_LABEL="full"
case "$PRESET_LABEL" in
    full)    FILE_PREFIX="Flynn" ;;
    minimal) FILE_PREFIX="Flynn-Minimal" ;;
    *)       FILE_PREFIX="Flynn-Lite" ;;
esac

# Create versioned copies with preset in filename
cp "$BUILD_DIR/Flynn.bin" "$BUILD_DIR/${FILE_PREFIX}-${VERSION_DISPLAY}.bin"
cp "$BUILD_DIR/Flynn.dsk" "$BUILD_DIR/${FILE_PREFIX}-${VERSION_DISPLAY}.dsk"
[ -f "$BUILD_DIR/Flynn.hqx" ] && cp "$BUILD_DIR/Flynn.hqx" "$BUILD_DIR/${FILE_PREFIX}-${VERSION_DISPLAY}.hqx"

# --- Build summary ---
ENABLED=""
DISABLED=""
for feat in finger glyphs cp437 clipboard savefile logging printing favorites darkmode altscreen offscreen statusbar cursorstyles tabstops color dblwidth; do
    case $feat in
        finger)       val=$FLYNN_FINGER ;;
        glyphs)       val=$FLYNN_GLYPHS ;;
        cp437)        val=$FLYNN_CP437 ;;
        clipboard)    val=$FLYNN_CLIPBOARD ;;
        savefile)     val=$FLYNN_SAVEFILE ;;
        logging)      val=$FLYNN_LOGGING ;;
        printing)     val=$FLYNN_PRINTING ;;
        favorites)    val=$FLYNN_FAVORITES ;;
        darkmode)     val=$FLYNN_DARK_MODE ;;
        altscreen)    val=$FLYNN_ALT_SCREEN ;;
        offscreen)    val=$FLYNN_OFFSCREEN ;;
        statusbar)    val=$FLYNN_STATUS_BAR ;;
        cursorstyles) val=$FLYNN_CURSOR_STYLES ;;
        tabstops)     val=$FLYNN_TAB_STOPS ;;
        color)        val=$FLYNN_COLOR ;;
        dblwidth)     val=$FLYNN_DBLWIDTH ;;
    esac
    if [ "$val" = "ON" ]; then
        ENABLED="$ENABLED $feat"
    else
        DISABLED="$DISABLED $feat"
    fi
done

echo ""
echo "Build complete (v${VERSION_DISPLAY}, ${PRESET_LABEL} preset):"
echo "  Sessions: ${FLYNN_MAX_SESSIONS}, Scrollback: ${FLYNN_SCROLLBACK} lines"
echo "  Features:${ENABLED}"
[ -n "$DISABLED" ] && echo "  Disabled:${DISABLED}"
echo "  SIZE: ${SIZE_PREFERRED}KB preferred / ${SIZE_MINIMUM}KB minimum"
ls -la "$BUILD_DIR"/${FILE_PREFIX}-${VERSION_DISPLAY}.* 2>/dev/null
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
