# TODO

## Phase 0: Environment Setup
- [x] Create project scaffolding and CMakeLists.txt
- [x] Install Retro68 toolchain (m68k-apple-macos-gcc 12.2.0)
- [x] Set up Basilisk II emulator (built from source, SLiRP networking)
- [x] First successful build of skeleton app
- [ ] Verify app boots in Basilisk II emulator

## Phase 1: Minimal Mac Application
- [ ] Verify app skeleton runs in emulator (window, menus, quit)
- [ ] Connection dialog (host, port input)
- [ ] Status bar / connection indicator

## Phase 2: MacTCP Networking
- [ ] TCP connection management (connect, send, receive, close)
- [ ] DNS resolution
- [ ] Connection error handling

## Phase 3: Telnet Protocol Engine
- [ ] IAC command parser (client-side, adapted from subtext-596)
- [ ] Option negotiation (BINARY, ECHO, SGA, TTYPE, NAWS, TSPEED)
- [ ] IAC doubling for data transparency
- [ ] Subnegotiation handling

## Phase 4: VT100 Terminal Emulator
- [ ] ANSI escape sequence parser
- [ ] Character cell grid (80x24)
- [ ] Cursor movement and positioning
- [ ] Text attributes (bold, reverse, underline)
- [ ] Scrolling and scroll region support
- [ ] Scrollback buffer (4 pages)

## Phase 5: Terminal UI
- [ ] Keyboard input mapping (arrow keys, function keys, ESC, Ctrl)
- [ ] Window content rendering from character grid
- [ ] Copy/paste support
- [ ] Scroll with mouse

## Phase 6: Polish
- [ ] Settings persistence
- [ ] Performance optimization
- [ ] Comprehensive testing with vi, nano, etc.
