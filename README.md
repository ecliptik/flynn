# Flynn

A Telnet and Finger client for classic 68000 Macintosh systems, from the Mac Plus and up. Supports monochrome on System 6 and 256 colors on System 7. Works with MacTCP and Open Transport's MacTCP compatibility layer. Cross-compiled on Linux using [Retro68](https://github.com/autc04/Retro68).

This project was 100% built agentically using [Claude Code](https://docs.anthropic.com/en/docs/claude-code).

<p align="center">
<a href="#download">Download</a> · <a href="#features">Features</a> · <a href="#keyboard-shortcuts">Keyboard Shortcuts</a> · <a href="#themes">Themes</a> · <a href="#building">Building</a> · <a href="#testing">Testing</a> · <a href="#acknowledgments">Acknowledgments</a> · <a href="#license">License</a>
</p>

### System 6

| | |
|:---:|:---:|
| ![Flynn telnet session with neofetch](docs/screenshots/flynn-neofetch.png) | ![Flynn running tmux with split panes](docs/screenshots/flynn-tmux.png) |
| **Telnet Session** | **tmux Split Panes** |
| ![Flynn connect dialog](docs/screenshots/flynn-connect.png) | ![Flynn BBS and terminal multi-window](docs/screenshots/flynn-bbs-multiwindow.png) |
| **Connect Dialog** | **Multi-Session and ANSI-BBS** |

### System 7

<p align="center">
<img src="docs/screenshots/flynn-256color.png" alt="Flynn 256-color on System 7" width="100%">
<br>
<strong>256-Color Multi-Session</strong>
</p>

---

## Download

Pre-built binaries are available on the [Releases](https://codeberg.org/ecliptik/flynn/releases) page, [Macintosh Garden](https://macintoshgarden.org/apps/flynn), and [Macintosh Repository](https://www.macintoshrepository.org/87841-flynn):

| Edition | Description | Memory | Download |
|---------|-------------|--------|----------|
| **Flynn** | All features — 4 sessions, 256-color themes, scrollback | ~1024KB | [.dsk](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-1.10.1.dsk) · [.hqx](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-1.10.1.hqx) · [.sit](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-1.10.1.sit) |
| **Flynn Lite** | Core features — 1 session, favorites, monochrome | ~340KB | [.dsk](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-Lite-1.10.1.dsk) · [.hqx](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-Lite-1.10.1.hqx) · [.sit](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-Lite-1.10.1.sit) |
| **Flynn Minimal** | Only essentials — 1 session, smallest footprint | ~256KB | [.dsk](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-Minimal-1.10.1.dsk) · [.hqx](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-Minimal-1.10.1.hqx) · [.sit](https://codeberg.org/ecliptik/flynn/releases/download/v1.10.1/Flynn-Minimal-1.10.1.sit) |

Each edition is available as `.dsk` (800K floppy image), `.hqx` (BinHex archive), or `.sit` (StuffIt 1.5.1 archive). See [docs/BUILD.md](docs/BUILD.md) for custom builds.

## Requirements

- Macintosh Plus or later (4MB RAM, 68000 CPU)
- System 6.0.8 or System 7 with MacTCP or Open Transport
- ~154KB disk space, ~73KB RAM per session (mono) / ~123KB (color)

## Features

**Telnet & Finger**
- **Telnet client** with full IAC negotiation, VT100/VT220/xterm terminal emulation, ANSI-BBS mode with CP437 rendering, and OSC sequences (window title, palette/color queries)
- **Finger client** (RFC 1288) with user queries, verbose mode, and forwarding
- **Up to 4 simultaneous sessions** in resizable windows with 192-line scrollback
- **Bookmarks** with per-bookmark protocol, font, terminal type, and theme

**Display**
- **256-color support** on System 7 with Color QuickDraw
- **11 built-in color themes** plus [Ghostty](https://ghostty.org)-format theme import/export
- **UTF-8 with box-drawing, Unicode glyphs, emoji, and Braille patterns**
- **8 fonts** (Monaco, Geneva, Chicago, Courier, New York, Helvetica, Times, Palatino) in 4 sizes

**Input**
- **M0110 keyboard support** — full mappings for Escape, Control, and function keys
- **Mouse text selection** with copy, paste, Select All, and bracketed paste mode
- **Find in Scrollback** (Cmd+F / Cmd+G) and session logging (Cmd+L)

**System Integration**
- **System 6.0.8 through 7.5.5** — MacTCP and Open Transport compatibility
- **System 7 enhancements**: movable modal dialogs, Apple Events, telnet:// URL handling
- **Persistent settings**, window position, and Preferences folder support
- Aligned with [Apple Human Interface Guidelines](https://archive.org/details/apple-human-interface-guidelines-1992)

## Keyboard Shortcuts

Flynn is designed for the Apple M0110/M0110A keyboard, which lacks Escape and Control keys. These mappings also work on modern USB/ADB keyboards.

| Action | Keys | Notes |
|--------|------|-------|
| Escape | Cmd+. | Classic Mac "Cancel" convention |
| Escape | Clear (keypad) | M0110A numeric keypad key |
| Escape | Esc key | Modern keyboards only (not on M0110) |
| Ctrl+key | Option+key | e.g., Option+C = Ctrl+C |
| Scroll up/down | Cmd+Up/Down | One line at a time |
| Scroll page | Cmd+Shift+Up/Down | One page at a time |
| Select text | Click+drag | Stream selection with inverse video |
| Select word | Double-click | Selects contiguous non-space word |
| Extend selection | Shift+click | Extends selection to click point |
| Copy | Cmd+C | Copies selection, or full screen if none |
| Paste | Cmd+V | Sends clipboard to connection |
| Select All | Cmd+A | Selects entire terminal screen |
| F1-F10 | Cmd+1..0 | For M0110 keyboards without function keys |
| Find | Cmd+F | Search scrollback and screen |
| Find Again | Cmd+G | Repeat last search |
| Show Clipboard | Cmd+K | Show clipboard viewer window |
| Reset Terminal | Cmd+E | Clear screen and reset parser state |
| Bookmarks | Cmd+B | Open bookmark manager |
| Add Favorite | Cmd+D | Save current session as bookmark |
| Finger | Cmd+I | Finger protocol query |
| New Session | Cmd+N | New session (new window if connected) |
| Reconnect | Cmd+R | Reconnect to same host after disconnect |
| Start/Stop Logging | Cmd+L | Log session output to text file |
| Print | Cmd+P | Print scrollback and screen content |
| Save Contents | Cmd+S | Save scrollback and screen to text file |
| Close Window | Cmd+W | Close active session window |
| Quit | Cmd+Q | Quit Flynn |

## Themes

Flynn ships with 11 built-in themes selectable from Options > Theme:

| Theme | Type | Description |
|-------|------|-------------|
| [Light](src/themes/light.h) | Mono | Black on white, default. Works on all systems. |
| [Dark](src/themes/dark.h) | Mono | White on black. Works on all systems. |
| [Solarized Light](src/themes/solarized_light.h) / [Dark](src/themes/solarized_dark.h) | Color | Ethan Schoonover's Solarized palette. |
| [TokyoNight Day](src/themes/tokyo_light.h) / [TokyoNight](src/themes/tokyo_dark.h) | Color | Based on the Tokyo Night color scheme. |
| [Amber CRT](src/themes/amber_crt.h) | Color | Amber phosphor CRT aesthetic. |
| [System 7](src/themes/system7.h) | Color | Macintosh System 7 CLUT palette. |
| [Compact Mac](src/themes/compact_mac.h) | Color | P7 phosphor warmth (Mac Plus/SE/Classic). |
| [Dracula](src/themes/dracula.h) | Color | Dark purple-accented palette. |
| [Nord](src/themes/nord.h) | Color | Arctic blue-tinted palette. |

Mono themes work on all systems including the Mac Plus. Color themes require a Mac II or later with Color QuickDraw (detected automatically at runtime). Up to 4 additional custom themes can be imported from [Ghostty](https://ghostty.org)-format files (Options > Theme > Import Theme).

To create custom themes or learn how the theme engine works, see the full [Theme Guide](docs/THEMES.md).

## Building

Requires the [Retro68](https://github.com/autc04/Retro68) cross-compilation toolchain. Build it from source (68k only):

```bash
git clone https://github.com/autc04/Retro68.git
cd Retro68 && git submodule update --init && cd ..
mkdir Retro68-build && cd Retro68-build
bash ../Retro68/build-toolchain.bash --no-ppc --no-carbon --prefix=$(pwd)/toolchain
```

Then build Flynn:

```bash
./scripts/build.sh
```

### Build Presets

Flynn supports fully customizable builds. Three presets cover common configurations:

| Preset | Sessions | Features | Memory |
|--------|----------|----------|--------|
| `full` | 4 | all features | ~1024KB |
| `lite` | 1 | core features | ~340KB |
| `minimal` | 1 | only essentials | ~256KB |

The default build uses the **full** preset. Select a preset with `--preset`:

```bash
./scripts/build.sh --preset minimal    # stripped, for 1MB Macs
./scripts/build.sh --preset full       # everything, 4 sessions
```

Individual features can be toggled with `--feature` / `--no-feature` flags. Presets are applied first, then individual flags override:

```bash
./scripts/build.sh --preset minimal --finger --bookmarks
./scripts/build.sh --sessions 2 --color --no-darkmode
```

See [docs/BUILD.md](docs/BUILD.md) for the complete list of build flags, feature details, memory costs, and examples.

## Testing

Uses [Snow](https://snowemu.com/) emulator (v1.3.1) with a Mac Plus ROM and System 6.0.8 SCSI hard drive image. Snow supports DaynaPORT SCSI/Link Ethernet emulation for MacTCP networking. The emulator can be fully automated via X11 for unattended testing. See `docs/TESTING.md` for details.

## Acknowledgments

- **[Claude Code](https://claude.ai/code)** by [Anthropic](https://www.anthropic.com/)
- **[Retro68](https://github.com/autc04/Retro68)** by Wolfgang Thaller
- **[Snow](https://snowemu.com/)** emulator
- **[wallops](https://github.com/jcs/wallops)** by joshua stein — MacTCP wrapper (`tcp.c`/`tcp.h`), DNS resolution (`dnr.c`/`dnr.h`), utility functions (`util.c`/`util.h`), and `MacTCP.h` are used directly from this project. ISC license.
- **[subtext](https://github.com/jcs/subtext)** by joshua stein — The Telnet IAC protocol implementation (`telnet.c`/`telnet.h`) served as the reference for Flynn's client-side telnet engine. ISC license.

## License

ISC License. See [LICENSE](LICENSE) for full details.

---

> **Note:** The [GitHub repository](https://github.com/ecliptik/flynn) is a read-only mirror. Please open issues and pull requests on [Codeberg](https://codeberg.org/ecliptik/flynn).
