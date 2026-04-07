# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository. Use Claude Teams (https://code.claude.com/docs/en/agent-teams) when appropriate to parallelize work.

Spawn a team with members:
- Team Lead (claude-opus-4-6) - Extensive Classic Macintosh System 6 Application experience, make decisions and delegate work
- Software Architect (claude-opus-4-6) - System 6 and Telnet Expert, reviews code for completeness, functionality, bug fixes, overall design
- Build Engineer (claude-sonnet-4-6) - build releases with Retro68, copy to HFS disks. Does NOT launch or interact with Snow emulator — only builds and deploys to HFS image
- Technical Writer (claude-sonnet-4-6) - create and update documentation, take screenshots, record any learnings for future use
- UI/UX QA Engineer (claude-sonnet-4-6) - SOLE operator of Snow emulator. Boots Snow, runs all GUI automation and testing with python scripts. No other agent should launch or interact with Snow

**Snow emulator ownership**: Only the QA Engineer may launch, interact with, or take screenshots of Snow. The Build Engineer deploys to the HFS disk image (`hmount`/`hcopy`/`humount`) but must NOT run `snowemu` or use xdotool/scrot. This prevents conflicts from multiple agents controlling the same emulator instance.

## Project Overview

Flynn is a Telnet client application for classic Macintosh (68000/Macintosh Plus) written in C, targeting System 6.0.8 with MacTCP 2.1. The primary use case is connecting to modern Linux telnetd servers for interactive terminal sessions (vi, nano, tmux, etc.).

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
- Toolchain built from source, installed at `Retro68-build/toolchain/` (in-repo, gitignored)
- Source cloned at `Retro68/` (in-repo, gitignored)
- Build: `./scripts/build.sh` (configures + makes), or manually: `mkdir build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=../Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && make`
- CMake flag: `-m68000` for Mac Plus compatibility
- MacTCP.h is NOT in Retro68's Multiversal Interfaces — copied from wallops-146
- Retro68 API quirks vs classic Toolbox: `qd.thePort` not `thePort`, `GetMenuHandle` not `GetMHandle`, `AppendResMenu` not `AddResMenu`, `LMGetApplLimit()` not `GetApplLimit`

## Testing

Emulator infrastructure lives in `/home/claude/emulators/`. See its docs for full details:
- `docs/TESTING.md` — build-deploy-test workflows for both emulators
- `docs/SNOW.md` — Snow emulator config, networking, keyboard
- `docs/SNOW-GUI-AUTOMATION.md` — X11/XTEST coordinate system and automation techniques

**IMPORTANT: Do NOT launch Snow, deploy to disk images, or run any automated testing unless the user explicitly asks. All testing and QA is done by the human driving the session. Only build and deploy when asked and ready to test.**

### Emulator: Snow (System 6 — Primary)

- **Launch**: `DISPLAY=:0 /home/claude/emulators/snow/snowemu /home/claude/emulators/snow/flynn.snoww &`
- **Keyboard**: Right ALT = Command key (`map_cmd_ralt: true`)
- **Networking**: DaynaPORT SCSI/Link, NAT mode. Host services reachable at `192.168.7.167` (NOT the 10.0.0.1 gateway)
- **Deploy**: `./scripts/deploy.sh --target snow --auto-open` handles kill → build → copy → relaunch

### Emulator: Basilisk II (System 7 — Secondary)

- **Launch**: `/home/claude/emulators/basilisk/launch.sh`
- **Deploy**: `./scripts/deploy.sh --target basilisk` (copies to ExtFS shared dir — no Basilisk restart needed, just reopen the app inside the emulator)
- **System 6 incompatible** — Basilisk II cannot run System 6. All 512KB ROMs force 32-bit addressing. Use Snow for System 6 testing.

### Emulator Rules (Critical — Violations Cause Data Corruption)

- **Never update a disk image while Snow is running** — the image is memory-mapped; corruption will result. Always `pkill -f snowemu; sleep 1` first.
- **Never run multiple Snow instances** or Snow and Basilisk II simultaneously — they share the X11 display.
- **Never hard-kill Basilisk II repeatedly** — corrupts the disk image and `sheep_net` kernel module. Quit from inside the emulator (Special > Shut Down) when possible. If you must kill externally, do it once and reset before relaunching:
  ```bash
  sudo rmmod sheep_net && sudo modprobe sheep_net
  cp ~/GlobalTalk\ System\ 761\ HD.hda /home/claude/emulators/disks/system7.hda
  ```
- **Use `scrot` for screenshots, NEVER ImageMagick `import`** — `import` grabs the X pointer and permanently breaks Snow mouse input.
  ```bash
  DISPLAY=:0 scrot -u /tmp/screenshot.png    # Focused window only
  ```
- **GUI automation requires XTEST incremental motion** — `xdotool mousemove` and `XWarpPointer` do NOT work with Snow. Use the canonical library: `/home/claude/emulators/scripts/snow_automation.py`. For clicks: `xdotool mousedown 1 && sleep 0.05 && xdotool mouseup 1` (NOT `xdotool click`).
- **Mac Plus M0110 keyboard has no Escape or Control keys.** Snow faithfully emulates this — X11 Escape/Control keysyms are ignored.
  - Escape: Cmd+. (Right Alt + period)
  - Ctrl+letter: Option+letter (Left Alt + letter)
- **Window manager**: Must be WindowMaker (`wmaker`) — KDE Plasma lacks `_NET_ACTIVE_WINDOW` support.
- Always set `DISPLAY=:0` when launching Snow or Basilisk II

### Test Target

- Host: `<telnet-host>` (user: `<username>`, password: `<password>`) — see local memory for real values
- Service: telnetd (standard Linux)

## Repository Conventions

- Git remote (primary): `ssh://git@forgejo.ecliptik.com/ecliptik/flynn.git`
- Codeberg mirror: `https://codeberg.org/ecliptik/flynn` (read-only, auto-mirrored from Forgejo)
- GitHub mirror: `https://github.com/ecliptik/flynn` (read-only, auto-mirrored from Forgejo)
- Never push directly to Codeberg or GitHub — only push to Forgejo. Releases are created on all three via `release.sh`.
- Use feature branches, commits, and worktrees for development
- Do NOT commit: disk images, FLYNN.md, or other non-source artifacts
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
