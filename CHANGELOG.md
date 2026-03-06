# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0] - 2026-03-05

### Added
- Client-side Telnet protocol engine (`telnet.c`/`telnet.h`)
  - 9-state IAC parser with full option negotiation
  - ECHO, SGA, BINARY, TTYPE (VT100), NAWS (80x24), TSPEED (19200)
  - Adapted from subtext-596 server-side implementation (inverted for client)
- VT100 terminal emulation engine (`terminal.c`/`terminal.h`)
  - 80x24 screen buffer with 96-line scrollback ring buffer
  - Full cursor movement, screen/line clear, insert/delete, scroll regions
  - Text attributes: bold, underline, inverse
  - CSI parameter parser supporting up to 8 parameters
- Wired telnet and terminal into main event loop
  - Data flow: TCP → telnet_process → terminal_process → display
  - IAC responses automatically sent back to server
  - Basic Monaco 9pt text rendering in window

### Fixed
- Retro68 GCC 12.2.0 compatibility (MacTCP.h, API renames, missing headers)
- In-repo toolchain build (`build.sh`, Retro68-build/ gitignored)
- DLOG resource missing `noAutoCenter` field for Rez

## [0.2.0] - 2026-03-05

### Changed
- Renamed project from telnet-m68k to Flynn
- App creator code changed from `TELN` to `FLYN`
- Window title, about dialog, and menu items now say "Flynn"
- Snow workspace renamed to `flynn.snoww`
- Updated all documentation, paths, and git remote references
- Copyright year updated to 2026

## [0.1.0] - 2026-03-05

### Added
- Project scaffolding: directory structure, CMakeLists.txt for Retro68
- Minimal Mac application skeleton (main.c) with Toolbox init, event loop, menus
- Resource file (telnet.r) with Apple/File/Edit menus and SIZE resource
- MacTCP wrapper (tcp.c/h) from wallops-146 by jcs
- DNS resolution (dnr.c/h) from wallops-146 by jcs
- Utility functions (util.c/h) from wallops-146 by jcs
- MacTCP.h header from wallops-146
- README.md with build instructions and acknowledgments
- LICENSE (ISC) with full attribution for derived code
- Development journal (JOURNAL.md)
- .gitignore for build artifacts, disk images, ROMs, reference materials
- Snow emulator setup with Mac Plus workspace (`diskimages/flynn.snoww`)
- SCSI hard drive image with System 6.0.8 installed via automated GUI
- X11 GUI automation guide (`docs/SNOW-GUI-AUTOMATION.md`)
- Testing guide (`docs/TESTING.md`) with Snow and Basilisk II documentation
- Development log (`docs/DEVLOG.md`) with System 6.0.8 installation walkthrough
- Installation screenshots (`docs/screenshots/`)
- Disk creation and driver extraction scripts (`tools/scripts/`)
