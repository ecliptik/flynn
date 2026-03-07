# Flynn

A Telnet client for classic Macintosh (68000/Mac Plus), targeting System 6.0.8 with MacTCP 2.1. Cross-compiled on Linux using [Retro68](https://github.com/autc04/Retro68).

This project was 100% vibe coded using [Claude Code](https://docs.anthropic.com/en/docs/claude-code)
with [Claude Code Teams](https://docs.anthropic.com/en/docs/claude-code/teams) orchestration.
[Read the development journal](JOURNAL.md) — from first build to full VT220 terminal emulator,
built entirely through agentic AI development.

| | |
|:---:|:---:|
| ![Flynn connected to a Linux server showing neofetch system info with Raspberry Pi ASCII art](docs/screenshots/flynn-neofetch-crt.png) | ![Flynn running tmux with three split panes showing cal, ls, and uname output with box-drawing characters](docs/screenshots/flynn-tmux-crt.png) |
| **Telnet Session** — Connected to a Linux server with neofetch displaying system info and Raspberry Pi ASCII art. Shows OSC window title, auto-login, and VT220 terminal emulation. | **tmux Split Panes** — Three-pane tmux layout with box-drawing line characters rendered via QuickDraw. Calendar, file listing, and kernel info running simultaneously. |
| ![Flynn connect dialog showing host, port, and username fields with saved settings](docs/screenshots/flynn-connect-crt.png) | ![Claude Code running inside Flynn on a Macintosh Plus, describing the Flynn project](docs/screenshots/flynn-claudecode-crt.png) |
| **Connect Dialog** — Native Mac dialog with host, port, and username fields. Settings persist across launches for quick reconnection. | **Claude Code via Flynn** — Claude Code running over telnet on a Mac Plus — the AI that built Flynn, running inside its own creation. |

---

[Features](#features) | [Keyboard Shortcuts](#keyboard-shortcuts) | [Building](#building) | [Testing](#testing) | [Acknowledgments](#acknowledgments) | [License](#license)

---

## Features

- **VT100/VT220/xterm terminal emulation** — runs vi, nano, tmux, mc, and other full-screen TUI apps over telnet
- **Box-drawing characters** — DEC Special Graphics rendered natively via QuickDraw for clean tmux panes, mc panels, and dialog borders
- **UTF-8 support** — accented characters, curly quotes, and symbols decoded and mapped to Mac Roman
- **Resizable window** — drag the grow box from 80x24 up to 132x50 cells, with NAWS negotiation
- **6 fonts** — Monaco 9/12, Courier 10, Chicago 12, Geneva 9/10, including proportional font rendering
- **Session bookmarks** — save up to 8 hosts with one-click connect from the Session menu
- **Username auto-login** — sends your username at the login prompt automatically
- **Mouse text selection** — click-drag, double-click to select words, shift-click to extend, Cmd+C/V for copy/paste
- **Scrollback** — 96 lines of history, navigate with Cmd+Up/Down
- **M0110 keyboard support** — Option key as Ctrl, Cmd+. as Escape, Cmd+1-0 for F-keys, designed for the original Mac Plus keyboard
- **Dark mode** — inverted display option for late-night telnet sessions
- **Settings persistence** — host, port, bookmarks, font, and preferences saved across launches
- **4MB Mac Plus** — ~98KB on disk, ~60KB RAM footprint (~1.5% of 4MB). Runs on a Macintosh Plus with System 6.0.8 and MacTCP 2.1

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
| F1-F10 | Cmd+1..0 | For M0110 keyboards without function keys |
| Bookmarks | Cmd+B | Open bookmark manager |
| Connect | Cmd+N | Open connect dialog |

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
./build.sh
```

## Testing

Uses [Snow](https://snowemu.com/) emulator (v1.3.1) with a Mac Plus ROM and System 6.0.8 SCSI hard drive image. Snow supports DaynaPORT SCSI/Link Ethernet emulation for MacTCP networking. The emulator can be fully automated via X11 for unattended testing. See `docs/TESTING.md` for details.

## Acknowledgments

- **[Claude Code](https://claude.ai/code)** — AI-assisted development by [Anthropic](https://www.anthropic.com/). Flynn was built entirely through agentic AI pair programming.
- **[Retro68](https://github.com/autc04/Retro68)** by Wolfgang Thaller — the 68k Macintosh cross-compilation toolchain that makes building classic Mac applications on modern Linux possible.
- **[Snow](https://snowemu.com/)** — a Rust-based classic Macintosh emulator with low-level hardware emulation, DaynaPORT SCSI/Link networking, and BlueSCSI Toolbox support. Used for all development testing.
- **[wallops](https://github.com/jcs/wallops)** by joshua stein — IRC client for classic Macintosh. MacTCP wrapper (`tcp.c`/`tcp.h`), DNS resolution (`dnr.c`/`dnr.h`), utility functions (`util.c`/`util.h`), and `MacTCP.h` are used directly from this project. ISC license.
- **[subtext](https://github.com/jcs/subtext)** by joshua stein — BBS server for classic Macintosh. The Telnet IAC protocol implementation (`telnet.c`/`telnet.h`) served as the reference for Flynn's client-side telnet engine. ISC license.

## License

ISC License. See [LICENSE](LICENSE) for full details.
