# Development Journal

A living document recording the development of Flynn, a Telnet client for classic Macintosh, built with agentic AI (Claude Code).

---

## 2026-03-05 — Phase 5: Terminal UI, Keyboard Mapping, and the TCP Bug

### Five-Agent Team

Used Claude Teams with five specialized agents working in parallel:

- **Team Lead** — Coordinated work, made architectural decisions, managed git
- **Software Architect** — Designed and implemented `terminal_ui.c`/`terminal_ui.h` (rendering module, text attributes, cursor blink, dirty-region invalidation)
- **Build Engineer** — Built Flynn, deployed to HDD image via hfsutils, launched Snow emulator, ran automated GUI tests via X11 XTEST
- **Documentation Writer** — Updated TESTING.md, README.md, and other docs
- **QA Engineer** — Wrote `test_launch.py` automated test script using python-xlib for X11 XTEST mouse/keyboard simulation

### Terminal UI Renderer (terminal_ui.c)

The architect created a dedicated rendering module separating display logic from the terminal emulator:

- **Font**: Monaco 9pt (the classic Mac monospace font), 6×12 pixel cells with 2px margins
- **Dirty tracking**: Per-row dirty flags — only redraws rows that changed
- **Attribute runs**: Scans each row for contiguous spans of identical attributes, issues one `DrawText()` call per span. Bold and underline use `TextFace()`. Inverse video uses `PaintRect()` + `srcBic` transfer mode (paints the cell black, then XORs the text).
- **Cursor**: XOR block cursor that blinks every ~30 ticks (~0.5 seconds)
- **Drawing strategy**: Direct `term_ui_draw()` calls from the null event handler, bypassing the `InvalRect` → `BeginUpdate`/`EndUpdate` mechanism. This was a key fix — the update event approach had timing issues with dirty flags being cleared before the update handler could read them.

### Keyboard Mapping

The build engineer implemented full VT100 keyboard mapping in `handle_key_down()`:

- Mac virtual keycodes (from `event->message`) mapped to VT100 escape sequences
- Arrow keys send `ESC[A`/`B`/`C`/`D`, Home/End send `ESC[H`/`ESC[F`
- Page Up/Down and Forward Delete send extended sequences (`ESC[5~`, `ESC[6~`, `ESC[3~`)
- Delete/Backspace sends DEL (0x7F), which is what Unix expects
- Ctrl+key computes the control character (`key & 0x1F`)
- Cmd+key is intercepted before any of this for menu shortcuts
- Retro68 uses `ControlKey` (capital C) — discovered during compilation

### The TCP Blocking Bug

After the initial deployment, Flynn connected to the telnet server and rendered the login banner and neofetch output perfectly — all 24 rows of ASCII art. But then the screen froze. Typing worked (characters were sent to the server), but no new output appeared on screen.

**Initial hypothesis (wrong)**: The `InvalRect()` → `updateEvt` invalidation path wasn't generating repaints after scrolling past row 24. Switched to direct drawing — this helped with responsiveness but didn't fix the freeze.

**Root cause**: The build engineer discovered it was a complete event loop hang, not a rendering issue. `_TCPStatus()` in `tcp.c` was the **only** TCP function that didn't `memset(pb, 0, sizeof(*pb))` before use. Stale data in the parameter block caused Snow's MacTCP emulation to return incorrect `amtUnreadData` values. When `conn_idle()` then called `_TCPRcv()` with its 30-second `commandTimeoutValue`, the entire application blocked — no events processed, no screen updates, nothing.

**Fix**: Two lines:
1. Added `memset(pb, 0, sizeof(*pb))` to `_TCPStatus()` (matching every other TCP function)
2. Reduced `_TCPRcv()` `commandTimeoutValue` from 30 to 1 second (safety net)

After the fix, all 14 test scenarios passed: echo commands, arrow keys for shell history, Ctrl+C to interrupt processes, `ls --color` with bold text, and full-screen TUI applications (nano with inverse status bars, vi with insert/normal mode switching).

### Automated Testing

The QA engineer created `test_launch.py`, a python-xlib script that drives the Snow emulator through X11 XTEST:

- Clicks on Flynn in the Finder list, opens it with Cmd+O
- Opens File > Connect, types the server IP, presses Return
- Types username/password at the login prompt
- Runs through 12 test scenarios (echo, arrows, Ctrl+C, nano, vi, disconnect)
- Takes screenshots at each step for verification
- Uses environment variables (`FLYNN_TEST_HOST`, `FLYNN_TEST_USER`, `FLYNN_TEST_PASS`) to avoid hardcoded credentials

### Repository Made Public

Before making the repo public on GitHub (as a read-only mirror of the Forgejo origin):
- Scrubbed all hardcoded credentials and private URLs from source and git history using `git-filter-repo`
- Replaced with placeholders in docs, env vars in test scripts
- Stored real credentials in local Claude memory (not committed)

### Current State

Phases 0–5 complete. Flynn is a functional telnet client: connects to real servers, negotiates telnet options, renders VT100 terminal output with text attributes, handles keyboard input with full arrow key / Ctrl / escape support, and runs full-screen TUI applications. Build size is 64KB.

---

## 2026-03-05 — Telnet Protocol and VT100 Terminal Engine

### Rebuilding Retro68 and Fixing Compatibility

The Retro68 toolchain at `/opt` had been lost, so it was rebuilt from source directly in-repo (`Retro68-build/`, gitignored). Building with `--no-ppc --no-carbon` for 68K-only took about 40 minutes and produced GCC 12.2.0.

The newer GCC + Multiverse.h headers introduced several incompatibilities with the wallops-146 code:

- **MacTCP.h**: The `kPascalStackBased` enum conflicted with `Multiverse.h`. Fix: guard with `#ifndef STACK_ROUTINE_PARAMETER` (can't use `#ifndef` on enum values — only works on `#define` macros).
- **API renames**: Retro68 uses newer Toolbox names (`GetDialogItem` not `GetDItem`, `DisposePtr` not `DisposPtr`, `PBControlSync` not `PBControl`). The PB functions also changed from 2-arg `(pb, async)` to 1-arg `Sync`/`Async` variants — fixed with dispatch macros.
- **Missing headers**: `Folders.h` and `GestaltEqu.h` don't exist as separate files; their contents are in `Multiverse.h`.

All of this was documented in `memory/retro68-compat.md` for future reference.

### Three-Agent Team for Phase 3+4

Used a team of three parallel agents to implement the telnet protocol engine, VT100 terminal emulator, and emulator testing simultaneously:

1. **Telnet Protocol Agent** — Studied subtext-596's server-side `telnet.c` and wrote a client-side implementation. The key insight is inverting the negotiation: where the server sends WILL, the client responds DO; where the server sends DO, the client responds WILL. The engine processes raw TCP bytes and produces clean terminal output + IAC responses as separate byte arrays, making it completely transport-independent.

2. **Terminal Display Agent** — Wrote a full VT100 escape sequence parser with a 5-state machine (NORMAL, ESC, CSI, CSI_PARAM, CSI_INTERMEDIATE). The 80×24 screen buffer uses 2 bytes per cell (character + attributes), totaling ~4KB. A 96-line scrollback ring buffer adds ~15KB. The entire `Terminal` struct is ~19KB — less than 0.5% of the Mac Plus's 4MB RAM.

3. **Emulator Test Agent** — Deployed Flynn.bin to the Mac HDD via `hfsutils`, booted Snow, and verified the application appears in the Finder. Testing revealed the previous 51KB build (pre-telnet) ran correctly.

### Integration

Wired the three modules into the main event loop:
```
TCP receive → telnet_process() → terminal_process() → InvalRect → redraw
```

IAC responses from `telnet_process()` are immediately sent back via `conn_send()`. Terminal output is fed to the VT100 parser, which updates the screen buffer and marks dirty rows. A basic Monaco 9pt renderer draws the buffer contents on update events.

The full build is now 61KB — up from 51KB with connection dialog, up from 8.5KB for the original skeleton.

### Current State

Phases 0–4 core modules are complete. The data pipeline works end-to-end in code: connect → IAC negotiate → parse VT100 → render. Next: test with an actual telnet server to verify the pipeline works in practice, then build the proper terminal UI rendering module.

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

After fixing these, the build succeeded. Output: Flynn.bin (8.5KB MacBinary), Flynn.dsk (800KB floppy image).

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
- Transfer and test the Flynn skeleton app in the emulator
