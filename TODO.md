# TODO

## Phase 0: Environment Setup
- [x] Create project scaffolding and CMakeLists.txt
- [x] Install Retro68 toolchain (m68k-apple-macos-gcc 12.2.0)
- [x] Set up Snow emulator (v1.3.1, Mac Plus, DaynaPORT networking)
- [x] Create SCSI hard drive image with System 6.0.8 installed
- [x] Develop X11 GUI automation for Snow (xdotool + WindowMaker)
- [x] First successful build of skeleton app
- [x] Boot from HDD without floppy (verified — boots to Finder from SCSI HDD)
- [x] Install MacTCP 2.1 on HDD
- [x] Install DaynaPORT SCSI/Link drivers (v1.2.5, via Installer app)
- [x] Configure MacTCP: Ethernet Built-In, IP 10.0.0.2, gateway 10.0.0.1, DNS 8.8.8.8
- [x] Test networking (MacTCP Ping to 10.0.0.1 — works, ~10ms RTT)
- [x] Verify skeleton app runs in Snow emulator (window, menus, quit all work)

## Phase 1: Minimal Mac Application
- [x] Verify app skeleton runs in emulator (window, menus, quit)
- [x] Connection dialog (host, port input)
- [x] Status bar / connection indicator

## Phase 2: MacTCP Networking
- [x] TCP connection management (connect, send, receive, close)
- [x] DNS resolution
- [x] Connection error handling
- [x] Retro68 GCC 12.2.0 compatibility fixes (MacTCP.h, API renames)
- [x] In-repo toolchain build (Retro68-build/, gitignored)

## Phase 3: Telnet Protocol Engine
- [x] IAC command parser (client-side, adapted from subtext-596)
- [x] Option negotiation (BINARY, ECHO, SGA, TTYPE, NAWS, TSPEED)
- [x] IAC doubling for data transparency
- [x] Subnegotiation handling (TTYPE IS VT100, NAWS 80x24, TSPEED 19200)

## Phase 4: VT100 Terminal Emulator
- [x] ANSI escape sequence parser (5-state CSI machine)
- [x] Character cell grid (80x24)
- [x] Cursor movement and positioning
- [x] Text attributes (bold, reverse, underline)
- [x] Scrolling and scroll region support
- [x] Scrollback buffer (4 pages, 96 lines)
- [x] Wired into main event loop (telnet→terminal→display pipeline)
- [x] Basic inline text rendering (Monaco 9pt)
- [x] Test with actual telnet server (192.168.7.230 — login, neofetch, shell)

## Phase 5: Terminal UI
- [x] Dedicated terminal_ui.c rendering module (Monaco 9pt, batched DrawText)
- [x] Efficient dirty-region redraw (per-row dirty flags, direct drawing)
- [x] Keyboard input mapping (arrow keys, function keys, ESC, Ctrl, Delete)
- [x] Bold/inverse/underline text rendering (TextFace + PaintRect/srcBic)
- [x] Cursor blink (XOR block cursor, ~30 tick interval)
- [x] Fix TCP blocking bug (_TCPStatus memset, _TCPRcv timeout 30→1)
- [x] Copy/paste support
- [x] Scroll with keyboard (Cmd+Up/Down)

## Phase 6: Polish
- [x] Menu state management (Connect/Disconnect enable/disable)
- [x] Scrollback viewing (Cmd+Up/Down, Cmd+Shift for page)
- [x] Copy/paste support (Cmd+C/V, clipboard integration)
- [x] About dialog (DLOG 130, version, credits)
- [x] Settings persistence ("Flynn Prefs" file, host/port saved)
- [x] Option key as Ctrl modifier (M0110 keyboard fix)
- [x] Quit confirmation when connected
- [x] Connection lost notification alert
- [x] Cursor hide/show (DECTCEM ESC[?25h/l)
- [x] Device Attribute response (DA ESC[c → VT100 ID)
- [x] Device Status Report response (DSR ESC[6n → cursor position)

## Future
- [ ] Performance optimization
- [ ] Mouse-based text selection
- [ ] Color support (for System 7 / color Macs)
- [ ] SSH support
