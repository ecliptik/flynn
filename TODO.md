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
- [x] Cmd+. sends Escape (classic Mac convention for Cancel)
- [x] Clear/NumLock key sends Escape (M0110A keypad)
- [x] charCode 0x1B fallback (USB/ADB keyboards with physical Escape key)
- [x] Quit confirmation when connected
- [x] Connection lost notification alert
- [x] Cursor hide/show (DECTCEM ESC[?25h/l)
- [x] Device Attribute response (DA ESC[c → VT100 ID)
- [x] Device Status Report response (DSR ESC[6n → cursor position)

## Phase 7: Mouse Selection
- [x] Selection data model (Selection struct in terminal_ui.c)
- [x] Click-drag text selection (StillDown/GetMouse tracking)
- [x] Double-click word selection
- [x] Shift-click extend selection
- [x] Inverse video rendering (ATTR_INVERSE XOR per cell)
- [x] Selection-aware Cmd+C copy (stream selection)
- [x] Selection cleared on keypress or incoming data

## Phase 8: Performance Optimization
- [x] Reduce WaitNextEvent timeout (5 ticks → 1 tick, 83ms → 17ms)
- [x] Optimize _TCPStatus() hot-path memset (explicit field init)
- [x] Cache selection active flag per row in draw_row()
- [x] Cache TextFace() value to avoid redundant Toolbox calls

## Phase 9: Terminal Foundations
- [x] Alternate screen buffer (DECSET ?1049/?1047/?47)
- [x] OSC window title (ESC]0;title BEL, ESC]2;title ST)
- [x] DCS string consumption (PARSE_DCS state)
- [x] Application cursor keys (DECCKM, DECSET ?1)
- [x] Auto-wrap toggle (DECAWM, DECSET ?7)
- [x] Origin mode (DECOM, DECSET ?6)
- [x] Insert/Replace mode (IRM, CSI 4 h/l)
- [x] Character set designation (ESC ( 0/B, ESC ) 0/B)
- [x] SI/SO charset switching (0x0E/0x0F)
- [x] ATTR_DEC_GRAPHICS bit (0x08) in TermCell.attr
- [x] Secondary DA response (CSI > c → VT220)
- [x] Primary DA upgraded to VT220 (ESC[?62;1;6c)
- [x] Soft reset (DECSTR, CSI ! p)
- [x] CSI s/u cursor save/restore
- [x] Bracketed paste mode (DECSET ?2004)
- [x] F-key support (F1-F12, ADB keycodes + Cmd+1..0 for M0110)
- [x] SGR extended color fix (38;5;N, 38;2;R;G;B skipped cleanly)
- [x] Additional SGR mappings (dim, italic, blink, bright fg for monochrome)

## Phase 10: DEC Special Graphics + VT220 Identity
- [x] draw_line_char() in terminal_ui.c (QuickDraw box-drawing)
- [x] TTYPE changed to VT220 (done in Phase 9, with VT100 fallback cycling)

## Future
- [ ] Session bookmarks (named host/port list in prefs)
- [ ] Font/size settings (Monaco 9/12, Chicago 12, Geneva 9/10)
- [ ] Application keypad mode (ESC = / ESC >)
- [ ] Color support (for System 7 / color Macs)
- [ ] SSH support
