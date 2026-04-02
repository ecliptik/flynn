# Building Flynn

Flynn's build system supports fully customizable builds via feature flags. You can
build anything from a stripped-down 82KB telnet client for a 1MB Macintosh Plus to a
full-featured 119KB multi-session powerhouse for System 7.

## Prerequisites

- Linux host (cross-compilation)
- Debian/Ubuntu packages:
  ```
  sudo apt-get install cmake libgmp-dev libmpfr-dev libmpc-dev libboost-all-dev bison flex texinfo ruby hfsutils macutils
  ```
- [Retro68](https://github.com/autc04/Retro68) toolchain built from source:
  ```
  cd Retro68-build
  bash ../Retro68/build-toolchain.bash --no-ppc --no-carbon --prefix=$(pwd)/toolchain
  ```

## Quick Start

```bash
# Default build (full preset) — good for most users
./scripts/build.sh

# Smallest possible build
./scripts/build.sh --preset minimal

# Mac Plus — everything except color, 1 session
./scripts/build.sh --preset lite
```

## Build Presets

Three presets cover the most common configurations. Use `--preset NAME` to select one.

### minimal (~256KB partition, ~82KB binary)

Bare-bones telnet client. Ideal for 1MB Macs or embedded use. Released as **Flynn Minimal**.

| Feature         | Status |
|-----------------|--------|
| Sessions        | 1      |
| Scrollback      | 0 (disabled) |
| Alt screen      | ON (vi/nano/tmux work) |
| Clipboard       | ON (copy/paste) |
| Everything else | OFF    |

What you lose: no scroll-back history, no bookmarks, no Finger protocol, no Unicode
glyphs or box-drawing characters, no file save, no dark mode, no offscreen buffering
(may see flicker), no status bar, no custom cursor styles or tab stops, no
double-width/height line rendering.

### lite (~384KB partition, ~115KB binary)

Recommended for Macintosh Plus and other compact Macs running System 6. Released as
**Flynn Lite**. `--preset macplus` is accepted as a legacy alias for `--preset lite`.

| Feature         | Status |
|-----------------|--------|
| Sessions        | 1      |
| Scrollback      | 96 lines (4 pages) |
| Finger          | ON     |
| Glyphs/emoji    | ON     |
| CP437            | ON     |
| Clipboard       | ON     |
| Save file       | ON     |
| Bookmarks       | ON     |
| Dark mode       | ON     |
| Alt screen      | ON     |
| Offscreen       | ON (flicker-free) |
| Status bar      | ON     |
| Cursor styles   | ON     |
| Tab stops       | ON     |
| Double-width    | ON     |
| Color           | OFF (no Color QuickDraw on Plus) |

### full (~768KB partition, ~119KB binary) — DEFAULT

Everything enabled. This is what you get with a plain `./scripts/build.sh` with no
arguments. For System 7 machines with Color QuickDraw and plenty of RAM. Released as
**Flynn** (no suffix, backwards compatible with prior releases).

| Feature         | Status |
|-----------------|--------|
| Sessions        | 4      |
| Scrollback      | 192 lines (8 pages) |
| All features    | ON (including color and double-width) |

## Feature Flags

Every feature can be individually toggled with `--feature` to enable or `--no-feature`
to disable. Presets are applied first, then individual flags override.

```bash
# Start with minimal, add back finger and bookmarks
./scripts/build.sh --preset minimal --finger --bookmarks

# Default (full) but with 2 sessions and no dark mode
./scripts/build.sh --sessions 2 --no-darkmode

# Disable color (targeting a mono System 7 Mac)
./scripts/build.sh --no-color
```

### Numeric Flags

| Flag | Range | Default | Description |
|------|-------|---------|-------------|
| `--sessions N` | 1-4 | 4 | Maximum simultaneous terminal sessions |
| `--scrollback N` | 0-256 | 192 | Scrollback buffer lines (0 = disabled) |

### Boolean Flags

| Flag | Default | Description |
|------|---------|-------------|
| `--finger / --no-finger` | ON | Finger protocol client (RFC 1288) |
| `--color / --no-color` | ON | 256-color support (requires System 7 + Color QuickDraw) |
| `--glyphs / --no-glyphs` | ON | Unicode glyph rendering (box-drawing, emoji, symbols) |
| `--cp437 / --no-cp437` | ON | Code Page 437 character set (ANSI-BBS) |
| `--clipboard / --no-clipboard` | ON | Copy/paste and text selection |
| `--savefile / --no-savefile` | ON | Save session text to file |
| `--logging / --no-logging` | ON | Session logging to text file |
| `--printing / --no-printing` | ON | Page Setup and Print support |
| `--bookmarks / --no-bookmarks` | ON | Favorites/bookmarks system |
| `--darkmode / --no-darkmode` | ON | Dark mode toggle (white on black) |
| `--dblwidth / --no-dblwidth` | ON | Double-width/height line rendering (DECDWL/DECDHL) |
| `--altscreen / --no-altscreen` | ON | Alternate screen buffer (used by vi, less, tmux) |
| `--offscreen / --no-offscreen` | ON | Offscreen double-buffer rendering (eliminates flicker) |
| `--statusbar / --no-statusbar` | ON | Status bar display |
| `--cursorstyles / --no-cursorstyles` | ON | DECSCUSR cursor styles (block, underline, bar) |
| `--tabstops / --no-tabstops` | ON | Custom tab stops via HTS/TBC escape sequences |

## Feature Details

### Sessions (--sessions N)

Controls the maximum number of simultaneous terminal windows. Each session uses
~40-95KB of RAM depending on other features. A Window menu appears when sessions > 1.

- **1 session**: No Window menu. Simplest, lowest memory.
- **2-4 sessions**: Window menu for switching. Each session is independent with its own
  connection, terminal state, font, and scrollback buffer.

### Scrollback (--scrollback N)

Lines of scroll-back history retained as content scrolls off the top of the screen.
Scrollback is the single largest memory consumer: each line costs 264 bytes
(132 columns x 2 bytes/cell).

| Lines | RAM/session | Pages (24-row) |
|-------|-------------|----------------|
| 0     | 0           | none           |
| 48    | ~12KB       | 2              |
| 96    | ~25KB       | 4              |
| 192   | ~50KB       | 8              |
| 256   | ~66KB       | 10             |

Setting scrollback to 0 also disables the scroll bar.

### Alt Screen (--altscreen)

The alternate screen buffer is used by full-screen programs like vi, nano, less, tmux,
and htop. When enabled, these programs draw on a separate screen and restore the
original content when they exit.

**Warning**: Disabling this means full-screen programs draw directly into the main
screen. They will still work, but the screen will not be restored when they exit.

### Offscreen (--offscreen)

Double-buffer rendering draws to an offscreen bitmap first, then copies to the screen
in a single
operation. This eliminates the visible flicker from erase-then-draw cycles.

Without offscreen buffering, rendering is done directly to the screen. This saves
~29KB of RAM but you may see flicker during screen updates.

### Color (--color)

256-color support for System 7 machines with Color QuickDraw. Colors are detected at
runtime — on monochrome hardware, color code is never executed.

This flag controls whether the color code is compiled in at all. On a Mac Plus or other
monochrome Mac, leave this OFF to save code space. On System 7 with a color display,
enable it for full xterm 256-color rendering.

### Glyphs (--glyphs)

Custom rendering for 180+ Unicode symbols including box-drawing characters, block
elements, braille patterns, and emoji. Without glyphs, unrecognized Unicode characters
display as `?`.

### CP437 (--cp437)

Code Page 437 character mapping for ANSI-BBS terminal mode. Required for connecting to
BBS systems that use IBM PC character sets. Requires glyphs (auto-enabled if needed).

### Finger (--finger)

RFC 1288 Finger protocol client. Query user information on remote hosts via port 79.
Accessed from the File menu.

### Logging (--logging)

Continuous session logging to a text file (like Unix `script`). Records all terminal
output with ANSI escape sequences stripped, producing clean readable text. Accessible
via File > Start Logging (Cmd+L). The log header includes a warning that the file may
contain sensitive information such as passwords.

### Printing (--printing)

Page Setup and Print (Cmd+P) support via the Macintosh Printing Manager. Prints
scrollback and screen buffer content with Geneva 9pt header/footer (hostname and page
numbers) and Monaco 9pt monospace terminal content. Supports ImageWriter spool printing.
Uses lazy printer record allocation (no overhead if never used).

### Bookmarks (--bookmarks)

Favorites system for saving frequently-used connections with per-bookmark host, port,
username, terminal type, and font settings. Includes a bookmark manager dialog,
quick-add from active sessions, and most-recently-used list in the File menu.

### Dark Mode (--darkmode)

Toggle between light mode (black on white) and dark mode (white on black). On
monochrome Macs this uses QuickDraw pattern inversion. On color System 7, explicit
foreground/background colors are swapped.

### Double-Width (--dblwidth)

DECDWL/DECDHL double-width and double-height line rendering. Used by some terminal
applications for banners and headers. Rarely encountered in practice.

Requires alt screen (auto-enabled if needed).

## Feature Dependencies

Some features require others to function. The build script resolves these automatically:

- `--dblwidth` requires `--altscreen` (double-width line attributes are saved/restored
  with the alternate screen)
- `--cp437` requires `--glyphs` (CP437 table entries reference glyph rendering
  primitives)

If a dependency is missing, the build script enables it automatically and prints a note.

## Memory Partition (SIZE Resource)

Classic Mac OS allocates a fixed memory partition to each application at launch. The
SIZE resource in the application binary tells the Finder how much memory to reserve.

The build script automatically computes the SIZE resource based on which features are
enabled:

```
Preferred = (base + shared_features + per_session_cost * sessions) * 1.30
Minimum   = Preferred - 64KB
```

The 30% headroom accounts for heap fragmentation, stack growth, and Toolbox overhead.
You will see the computed SIZE in the build summary output.

The SIZE value shown in Finder's "About This Macintosh" is the partition size, not
actual memory usage. Your actual usage will be lower.

## Build Output

The build produces three artifacts in the `build/` directory:

- **Flynn.bin** — MacBinary archive (for transfer via serial, network, or emulator)
- **Flynn.dsk** — 800K floppy disk image (bootable, includes About Flynn)
- **Flynn.hqx** — BinHex archive (for email/BBS distribution, requires `macutils`)

Versioned copies include the preset name (e.g., `Flynn-1.9.8.bin` for full,
`Flynn-Lite-1.9.8.bin` for lite, `Flynn-Minimal-1.9.8.bin` for minimal).

## Build Summary

After building, a summary shows the configuration:

```
Build complete (v1.9.8, full preset):
  Sessions: 4, Scrollback: 192 lines
  Features: finger glyphs cp437 clipboard savefile bookmarks
            darkmode altscreen offscreen statusbar cursorstyles tabstops color dblwidth
  SIZE: 768KB preferred / 704KB minimum
```

## Examples

```bash
# Mac Plus, single session, no color
./scripts/build.sh --preset lite

# Mac Plus, single session, maximum scrollback
./scripts/build.sh --preset lite --scrollback 256

# Default but with 2 sessions instead of 4
./scripts/build.sh --sessions 2

# 1MB Mac 512Ke, absolute minimum
./scripts/build.sh --preset minimal --no-clipboard --no-altscreen

# BBS user: needs CP437 and glyphs, skip everything else
./scripts/build.sh --preset minimal --glyphs --cp437

# CI build: all features, parallel make
./scripts/build.sh -j$(nproc)
```

## Deploying to HFS Disk Image

After building, deploy to a Snow emulator or real Mac HFS volume:

```bash
hmount diskimages/snow-sys608.img
hmkdir :Flynn
hcopy -m build/Flynn.bin ':Flynn:Flynn'
hattrib -t APPL -c FLYN ':Flynn:Flynn'
hcopy -r 'build/About Flynn' ':Flynn:About Flynn'
hattrib -t ttro -c ttxt ':Flynn:About Flynn'
humount
```

## CMake Direct Usage

For advanced users, flags can be passed directly to CMake without using `build.sh`:

```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake \
  -DFLYNN_MAX_SESSIONS=2 \
  -DFLYNN_SCROLLBACK=192 \
  -DFLYNN_COLOR=ON \
  -DFLYNN_FINGER=OFF
make
```

Note: when using CMake directly, you must set the SIZE resource manually in
`resources/telnet.r`. The `build.sh` script handles this automatically.
