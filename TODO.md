# TODO

## Future
- [x] ~SSH support~ — removed from scope
- [x] ANSI-BBS emulation with CP437 character set (v1.9.2)
- [x] Multiple simultaneous sessions in separate windows (v1.5.0)
- [x] 256-color support (System 7 / Color QuickDraw) — v1.9.0
- [x] Expanded emoji and glyph coverage (202 primitives + 17 emoji) — v1.9.3
- [x] Finger protocol support (RFC 1288) with forwarding

## Bugs
- [ ] Connecting dialog not centered on window/desktop (visible on Basilisk II / System 7)
- [ ] Multi-session window drag leaves white ghost: dragging front window over dark-mode back window leaves unredrawn white rectangle where front window previously sat. Repro: session 1 ANSI-BBS dark mode, session 2 xterm light mode (resized smaller), drag session 2 — exposed region of session 1 stays white (PaintBehind fill never overwritten). Likely updateEvt not firing or offscreen/BeginUpdate interaction. Fix: try explicit InvalRect on background windows after DragWindow.
