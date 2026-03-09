# TODO

## Future (v2.0)
- [ ] Color support (for System 7 / color Macs)
- [ ] Expanded emoji and glyph coverage
  - Currently: 15 monochrome 10x10 bitmap emoji (2-cell wide via CopyBits)
  - Currently: 51 QuickDraw primitive glyphs (arrows, shapes, blocks, suits, etc.)
  - Currently: Braille pattern rendering (U+2800-U+28FF dot grids)
  - Add more emoji bitmaps as needed (faces, objects, flags, etc.)
  - Consider larger bitmap sizes for better detail at bigger font sizes
- [x] ~~SUPDUP protocol support (RFC 734)~~ — **Won't do.** SUPDUP uses its own display protocol (%TD codes) rather than VT100/ANSI, requiring either a parallel rendering path or lossy translation. Too fundamental a change for a Telnet-focused client, and very few servers exist to connect to.
- [ ] ANSI-BBS emulation with CP437 character set
  - Code Page 437 box-drawing, shading blocks, card suits, etc.
  - Map CP437 glyphs to bitmap resources or custom font
  - Support ANSI color (with color Mac support) and monochrome fallback
  - For connecting to classic BBS systems (Synchronet, Mystic, etc.)
- [ ] Multiple simultaneous sessions in separate windows
  - Each window has its own Connection, TelnetState, Terminal, and TerminalUI
  - Window switching via standard Mac Window menu
  - Independent connect/disconnect per session
- [x] ~~SSH support~~ — **Won't do.** Native SSH requires bigint crypto math (1024-bit DH) on an 8MHz 68000 with no hardware acceleration — handshake alone could take 30+ seconds. No existing SSH library fits 68000/4MB constraints. A proxy approach through a Linux box is more practical but outside Flynn's scope as a standalone client.

## Completed (v1.0.0)

All phases 0-16 are complete. See git history for details.
