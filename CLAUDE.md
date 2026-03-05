# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A Telnet client application for classic Macintosh (68000/Macintosh Plus) written in C, targeting System 6.0.8 with MacTCP 2.1. The primary use case is connecting to modern Linux telnetd servers for interactive terminal sessions (vi, nano, etc.).

## Target Platform Constraints

- Motorola 68000 CPU only (no PowerPC, no 68020+ instructions)
- 4 MiB RAM on Macintosh Plus
- System 6.0.8 primary target, System 7 secondary
- MacTCP for networking (not Open Transport)
- Latency and responsiveness are critical priorities
- Monochrome only, no color or grayscale
- Start with VT100 

## Build System

- Cross-compile on Linux using [Retro68](https://github.com/autc04/Retro68) toolchain
- Toolchain installed at `/opt/Retro68-build/toolchain/` (m68k-apple-macos-gcc 12.2.0)
- CMake flag: `-m68000` for Mac Plus compatibility
- MacTCP.h is NOT in Retro68's Multiversal Interfaces — copied from wallops-146
- Retro68 API quirks vs classic Toolbox: `qd.thePort` not `thePort`, `GetMenuHandle` not `GetMHandle`, `AppendResMenu` not `AddResMenu`, `LMGetApplLimit()` not `GetApplLimit`

## Testing

### Emulator: Snow (Primary)

[Snow](https://snowemu.com/) v1.3.1, a Rust-based classic Mac emulator with low-level hardware emulation.

- **Binary**: `tools/snow/snowemu` (local copy, gitignored)
- **Workspace**: `diskimages/telnet-m68k.snoww` (Mac Plus, 1.5x scale)
- **HDD image**: `diskimages/snow-sys608.img` (90MB SCSI, System 6.0.8 installed)
- **ROM**: `roms/68k/128k/Macintosh Plus/1986-03 - 4D1F8172 - MacPlus v3.ROM`
- **Floppies**: `tools/floppies/*.img` (System 6.0.8 set, 800K each)
- **Keyboard**: Right ALT = Command key (`map_cmd_ralt: true`)
- **Networking**: DaynaPORT SCSI/Link Ethernet emulation, NAT mode (10.0.0.0/8)
- **File sharing**: BlueSCSI Toolbox protocol (`Tools > File Sharing`)
- **Launch**: `DISPLAY=:0 tools/snow/snowemu diskimages/telnet-m68k.snoww &`

### GUI Automation

Snow can be fully automated via X11 for unattended testing. See `docs/SNOW-GUI-AUTOMATION.md` for the complete guide.

- **Window manager**: WindowMaker (`wmaker`) — KDE lacks `_NET_ACTIVE_WINDOW` support
- **Click method**: `xdotool mousedown 1 && sleep 0.05 && xdotool mouseup 1` (NOT `xdotool click`)
- **Screenshots**: `DISPLAY=:0 import -window root screenshot.png`
- **Coordinates**: Screenshot pixel coords = X11 screen coords (1:1 at 1280x800)

### Basilisk II (Deprecated)

Basilisk II was tested extensively but has critical incompatibilities with System 6.0.8. All 512KB ROMs force 32-bit addressing, which System 6 doesn't support, and synthetic mouse events don't register in its X11 event loop. See `docs/TESTING.md` for details.

### Test Target

- Host: `<telnet-host>` (user: `<username>`, password: `<password>`) — see local memory for real values
- Service: telnetd (standard Linux)

## Repository Conventions

- Git remote: `https://github.com/ecliptik/flynn.git`
- Use feature branches, commits, and worktrees for development
- Do NOT commit: disk images, TELNET-M68K.md, or other non-source artifacts
- Maintain: README.md, CHANGELOG.md, TODO.md
- Maintain a development journal documenting how the project was built with agentic AI

## Reference Code

Two existing classic Mac C applications are in `./code-examples/` for reference:

- **wallops-146**: IRC client by jcs@jcs.org - shows classic Mac event loop, menu handling, MacTCP usage, window management, focusable UI pattern
- **subtext-596**: BBS server by jcs@jcs.org - contains `telnet.c`/`telnet.h` with a complete Telnet IAC negotiation implementation (server-side), MacTCP TCP wrapper API in `tcp.c`/`tcp.h`

Key patterns from reference code:
- MacTCP wrapper functions: `_TCPInit`, `_TCPCreate`, `_TCPActiveOpen`, `_TCPSend`, `_TCPRcv`, `_TCPStatus`, `_TCPClose`, `_TCPRelease`, `DNSResolveName`
- Classic Mac event loop: `WaitNextEvent` with `EventRecord`, handling `mouseDown`, `keyDown`, `updateEvt`, `activateEvt`, `app4Evt`
- Mac Toolbox initialization sequence: `InitGraf`, `InitFonts`, `FlushEvents`, `InitWindows`, `InitMenus`, `TEInit`, `InitDialogs`, `InitCursor`, `MaxApplZone`
- Telnet protocol: IAC command/option negotiation state machine, NAWS (window size), TTYPE, TSPEED, SGA, ECHO, BINARY options
- The subtext telnet code is server-side; this project needs a **client-side** implementation (we send DO/WILL, they respond)

## Architecture Notes

The application needs these core components:
- **Telnet protocol engine**: Client-side IAC negotiation (the inverse of subtext's server-side implementation)
- **Terminal emulator**: VT100/ANSI escape sequence parsing and rendering
- **MacTCP networking**: TCP connection management using the MacTCP API (see `tcp.c`/`tcp.h` in examples)
- **Mac UI layer**: Classic Mac event loop, window management, menus, keyboard input
- **DNS resolution**: Using MacTCP DNR (see `dnr.c`/`dnr.h` in examples)

## Programming Reference Material

- PDF books in `./references/`: Human Interface Guidelines (1992), C Programming Techniques for the Mac (1989), How To Write Macintosh Software (1988)
- Online: https://vintageapple.org/macprogramming/
