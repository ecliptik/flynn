# TODO

## Future (v2.0)
- [ ] Color support (for System 7 / color Macs)
- [ ] Emoji rendering via bitmap resources (CopyBits into terminal cells)
  - Store common emoji as small monochrome bitmaps in custom resources
  - UTF-8 decoder already identifies emoji codepoints (currently renders `??`)
  - Look up bitmap table and CopyBits into cell instead of DrawChar
  - Support 2-cell-wide emoji for better legibility at small sizes
  - Bypasses font system entirely — no 256 glyph limit
- [ ] SUPDUP protocol support (RFC 749)
  - ITS terminal protocol, predecessor to Telnet
  - Connect to PDP-10/ITS systems and modern SUPDUP servers
- [ ] ANSI-BBS emulation with CP437 character set
  - Code Page 437 box-drawing, shading blocks, card suits, etc.
  - Map CP437 glyphs to bitmap resources or custom font
  - Support ANSI color (with color Mac support) and monochrome fallback
  - For connecting to classic BBS systems (Synchronet, Mystic, etc.)
- [ ] Multiple simultaneous sessions in separate windows
  - Each window has its own Connection, TelnetState, Terminal, and TerminalUI
  - Window switching via standard Mac Window menu
  - Independent connect/disconnect per session
- [ ] SSH support

## Completed (v1.0.0)

All phases 0-16 are complete. See git history for details.
