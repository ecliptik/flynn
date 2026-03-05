# Development Journal

A living document recording the development of telnet-m68k, a Telnet client for classic Macintosh, built with agentic AI (Claude Code).

---

## 2026-03-05 — Project Inception and Phase 0 Complete

### Planning with a Research Team

The project began with a four-agent research team, each focused on a different domain:

1. **Telnet Protocol Researcher** — Deep dive into RFC 854/855/857/858/1091/1073 and the subtext-596 reference code. Mapped out the complete client-side IAC negotiation flow, identified the minimum viable option set (BINARY, ECHO, SGA, TTYPE, NAWS, TSPEED), and documented how to invert subtext's server-side telnet.c for our client implementation. Key insight: modern Linux telnetd sends ~10 option requests immediately on connect; we only need to accept 6 and reject the rest.

2. **Classic Mac Programmer** — Studied wallops-146 (IRC client) and subtext-596 (BBS server) exhaustively. Documented the exact Mac Toolbox initialization sequence (MaxApplZone must be last!), WaitNextEvent patterns with adaptive wait times, MacTCP wrapper API lifecycle, keyboard handling, and memory management for a 4MB Mac Plus. Identified which files can be reused directly (tcp.c, dnr.c, util.c) vs. adapted (telnet.c) vs. built from scratch (VT100 parser, terminal UI).

3. **Build System Researcher** — Researched the Retro68 cross-compilation toolchain: prerequisites, build steps, CMake integration, resource file compilation with Rez, and the `-m68000` flag for Mac Plus compatibility. Found that MacTCP.h is not included in Retro68's Multiversal Interfaces and must be supplied from the wallops reference code.

4. **QA/Testing Researcher** — Planned the complete Basilisk II emulator setup: SLiRP networking (NAT, no root needed), `extfs` shared filesystem for deploying builds, MacTCP manual IP configuration, and a 9-phase progressive test plan from smoke test through full vi/nano interactive testing.

All four agents worked in parallel and delivered comprehensive reports in about 15 minutes.

### Environment Setup

A second team handled Phase 0 execution:

- **Retro68 Toolchain**: Installed prerequisites via apt, cloned the repo, and built with `--no-ppc --no-carbon` (68K only). Produced `m68k-apple-macos-gcc` (GCC 12.2.0), Rez resource compiler, and MakeAPPL bundler.

- **Basilisk II**: Not in Debian apt repos; built from source (github.com/cebix/macemu). Configured with Mac Plus v3 ROM, the existing mac608.hda System 6.0.8 disk image, SLiRP networking, and `extfs` pointing to the build directory.

- **Project Scaffolding**: Created src/, include/, resources/ structure. Copied MacTCP wrapper files (tcp.c/h, dnr.c/h, util.c/h) from wallops-146 with all original ISC license headers preserved. Created main.c skeleton with correct Toolbox init, event loop, and menu handling.

### First Build

The initial build attempt revealed several Retro68-specific differences from classic Mac Toolbox:

- **No `Desk.h` header** — Retro68 uses Multiversal Interfaces. `SystemClick` and `OpenDeskAcc` are in `Multiverse.h`.
- **`qd.thePort` not `thePort`** — QuickDraw globals are accessed through the `qd` struct.
- **`qd.screenBits` not `screenBits`** — Same pattern for screen bounds.
- **`GetMenuHandle` not `GetMHandle`** — Newer API names (old names exist as macros but weren't resolving).
- **`AppendResMenu` not `AddResMenu`** — Same renaming pattern.
- **`GetApplLimit` linker error** — Used `LMGetApplLimit()` low-memory accessor macro instead.

After fixing these, the build succeeded. Output: TelnetM68K.bin (8.5KB MacBinary), TelnetM68K.dsk (800KB floppy image).

### Attribution

Both reference projects (wallops and subtext) are by joshua stein (jcs@jcs.org), released under the ISC license. All copied files retain original copyright headers. A comprehensive LICENSE file documents all derived code and upstream copyrights (including University of Illinois for tcp.c and Apple for dnr.c).

### Current State

Phase 0 is complete. The skeleton app builds successfully with Retro68. Next step: boot it in Basilisk II to verify it actually runs on the emulated Mac.

---

## 2026-03-05 — Switching to Snow Emulator and Installing System 6.0.8

### Why Snow Instead of Basilisk II

Basilisk II hit a dead end: all its 512KB ROMs force 32-bit addressing, which System 6.0.8 doesn't support, and every method of programmatically clicking the resulting dialog failed. Switched to [Snow](https://snowemu.com/) (v1.3.1), a Rust-based emulator with low-level hardware emulation, Mac Plus support, and DaynaPORT SCSI/Link Ethernet for MacTCP networking.

### GUI Automation: Teaching Claude to Use a Mac

Developed a method for Claude to operate the Snow emulator entirely through X11 automation — taking screenshots to see the screen, then using `xdotool` to click and type. This required switching from KDE to WindowMaker (KDE lacks `_NET_ACTIVE_WINDOW`), discovering that `xdotool click` doesn't work with Snow's egui (must use explicit `mousedown`/`mouseup` pairs), and learning the different interaction patterns for Mac menus (press-hold-drag) vs Snow's host menus (regular click).

A comprehensive automation guide was written at `docs/SNOW-GUI-AUTOMATION.md`.

### Automated System 6.0.8 Installation

Using the GUI automation techniques, Claude performed the entire System 6.0.8 installation unattended — launching the Installer from a floppy, clicking through the UI, and swapping four floppy disk images via Snow's Drives menu as the Installer requested them. The result is a bootable 90MB SCSI hard drive image at `diskimages/snow-sys608.img`.

Creating the SCSI drive image itself required trial and error — three failed approaches before finding a pre-formatted blank HFS image with the proper Apple partition map and SCSI driver. See `docs/DEVLOG.md` for the full technical details and `docs/screenshots/` for step-by-step screenshots.

### What's Next

- Boot from HDD without floppy to verify standalone boot
- Install MacTCP 2.1 and configure DaynaPORT networking
- Transfer and test the TelnetM68K skeleton app in the emulator
