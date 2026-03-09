# Flynn Feature Roadmap (Historical)

> **Note:** All phases below were completed for v1.0.0. See `TODO.md` for the current roadmap.

Generated 2026-03-06 from team-based planning analysis.

## Phase 9: Terminal Foundations (~200 lines, +4KB memory)

Combines alternate screen buffer, core DECSET/DECRST modes, and OSC parsing.
These are tightly coupled — all needed before changing TTYPE.

- [x] Alternate screen buffer (DECSET ?1049/?1047/?47)
  - Save/restore main screen (3.8KB static buffer in Terminal struct)
  - Freeze scrollback during alt screen
  - Clear on switch, restore on switch back
  - Files: terminal.h, terminal.c
- [x] OSC/DCS string consumption (3 new parser states)
  - PARSE_OSC, PARSE_OSC_ESC, PARSE_DCS
  - Prevents garbage from title sequences appearing on screen
  - Files: terminal.h, terminal.c
- [x] SGR 38/48 extended color fix
  - Fix latent misparse of `38;5;N` and `38;2;R;G;B` sub-parameters
  - Files: terminal.c (SGR 'm' handler)
- [x] Application cursor keys (DECSET ?1 / DECCKM)
  - Arrow keys send ESC O A/B/C/D when enabled (needed for vi/tmux)
  - Files: terminal.h, terminal.c, main.c
- [x] Auto-wrap toggle (DECSET ?7 / DECAWM)
  - Files: terminal.h, terminal.c
- [x] Origin mode (DECSET ?6 / DECOM)
  - Files: terminal.c
- [x] Secondary DA response (CSI > c)
  - Respond with ESC[>1;10;0c (VT220)
  - Files: terminal.c
- [x] Soft reset (DECSTR, CSI ! p)
  - Files: terminal.c
- [x] CSI s/u cursor save/restore (ANSI alternative to ESC 7/8)
  - Files: terminal.c

## Phase 10: DEC Special Graphics + VT220 Identity (~180 lines)

Biggest visual impact for TUI apps (tmux borders, mc panels, dialog boxes).

- [x] Character set designation (ESC ( 0/B, ESC ) 0/B) *(done in Phase 9)*
  - G0/G1 charset tracking in Terminal struct
  - Files: terminal.h, terminal.c
- [x] SI/SO charset switching (0x0E/0x0F) *(done in Phase 9)*
  - Files: terminal.c
- [x] ATTR_LINEDRAW bit (0x08) in TermCell.attr *(done in Phase 9, as ATTR_DEC_GRAPHICS)*
  - Files: terminal.h
- [x] draw_line_char() in terminal_ui.c
  - QuickDraw MoveTo/LineTo for box-drawing characters
  - 18 character mappings (corners, tees, crosses, horizontal, vertical, etc.)
  - Files: terminal_ui.c
- [x] Update DA response to VT220 (ESC[?62;1;6c) *(done in Phase 9)*
  - Files: terminal.c
- [x] Change TTYPE from "VT100" to "VT220" *(done in Phase 9, with cycling)*
  - Files: telnet.c

## Phase 11: Session Bookmarks (~250 lines, +1.3KB prefs)

Independent of terminal changes — pure UI/UX feature. Can be done in parallel.

- [x] Bookmark struct (name[32], host[128], port), max 8
  - Files: settings.h
- [x] Extended FlynnPrefs with v1→v2→v3 migration
  - Files: settings.h, settings.c
- [x] Dynamic Session menu bookmark items
  - AppendMenu + SetMenuItemText below Quit
  - Files: main.c, main.h
- [x] Bookmark manager dialog (DLOG 131)
  - UserItem-based list with click selection (no List Manager needed)
  - Add/Edit/Delete/Connect buttons
  - Files: main.c, telnet.r
- [x] Add/Edit bookmark dialog (DLOG 132)
  - Name, Host, Port fields
  - Files: main.c, telnet.r
- [x] One-click connect from bookmark menu item
  - Extracted conn_connect() from conn_open_dialog()
  - Files: connection.c, connection.h, main.c

## Phase 12: Font Selection (~300 lines)

Most invasive change — touches terminal.c extensively (~40 TERM_COLS/TERM_ROWS refs).

- [x] Replace CELL_WIDTH/CELL_HEIGHT with runtime variables (g_cell_width/g_cell_height)
  - Files: terminal_ui.h, terminal_ui.c
- [x] term_ui_set_font() with GetFontInfo measurement
  - Files: terminal_ui.c
- [x] Add active_cols/active_rows to Terminal struct
  - Static arrays stay at max 80x24, use subset
  - Files: terminal.h, terminal.c (~40 occurrences)
- [x] Settings menu (MENU 131) with font items
  - Monaco 9 (6x12, 80x24), Monaco 12 (7x15, ~70x20)
  - Files: main.c, main.h, telnet.r
- [x] Font fields in FlynnPrefs (font_id, font_size)
  - Files: settings.h, settings.c
- [x] NAWS renegotiation on font/grid change
  - Files: main.c
- [x] Window resizing for new grid dimensions
  - Files: main.c

## Phase 13: xterm Compatibility (~150 lines, incremental)

After VT220 is solid — adds xterm-specific features.

- [x] Bracketed paste mode (DECSET ?2004) *(done in Phase 9)*
  - Wrap pasted text in ESC[200~/ESC[201~ markers
  - Files: terminal.h, terminal.c, main.c
- [x] OSC window title → SetWTitle *(done in Phase 9)*
  - Parse OSC 0/1/2, extract title, update Mac window
  - Files: terminal.c, main.c
- [x] Application keypad mode (DECKPAM ESC =, DECKPNM ESC >)
  - Numpad sends SS3 sequences in application mode
  - Files: terminal.c, main.c
- [x] TTYPE cycling: xterm → VT220 → VT100 (first response is "xterm")
  - Files: telnet.c
- [x] F-key support *(done in Phase 9)*
  - ADB virtual keycodes for extended keyboards
  - Cmd+1..0 for M0110 keyboards
  - Files: main.c
- [x] Silently consume remaining modes
  - Mouse reporting (?1000-1006), focus events (?1004), cursor blink (?12)
  - Files: terminal.c

## Phase 14: UTF-8 Support (~150 lines, +~1KB code)

Decode UTF-8 multi-byte sequences and translate to the best single-byte
representation. Keeps TermCell at 2 bytes (no memory increase for buffers).
See `docs/PLAN-UTF8.md` for complete design.

- [x] UTF-8 decoder state machine in terminal.c (2/3/4-byte sequences)
  - utf8_buf[4], utf8_len, utf8_expect in Terminal struct
  - Intercept bytes >= 0x80 in PARSE_NORMAL before term_put_char
  - Files: terminal.h, terminal.c
- [x] Unicode box-drawing → DEC Special Graphics mapping
  - U+2500-U+257F → existing draw_line_char() glyphs (33 mappings)
  - Light, heavy, and double-line variants all map to same glyphs
  - Fixes tmux/htop/mc borders on UTF-8 servers
  - Files: terminal.c
- [x] Unicode → Mac Roman translation (Latin-1 Supplement)
  - 128-entry lookup table for U+0080-U+00FF
  - Accented characters, currency symbols, common punctuation
  - Files: terminal.c
- [x] Unicode symbol → Mac Roman mapping
  - Em/en dash, curly quotes, bullet, ellipsis, euro, math symbols
  - ~18 common codepoints in U+2000-U+20FF range
  - Files: terminal.c
- [x] Wide character handling (CJK, emoji)
  - Render as 2-cell placeholder `??`
  - Emoji modifier/ZWJ sequence absorption (don't render extra placeholders)
  - Files: terminal.c
- [x] Fallback for unmapped codepoints
  - `?` as replacement character (was Mac Roman sigma, fixed)
  - Files: terminal.c

## Phase 15: Additional Fonts + Window Resizing (~250 lines, +29KB memory)

Expands font options and adds drag-to-resize window support.

- [x] Additional font options (Courier 10, Chicago 12, Geneva 9, Geneva 10)
  - Preferences > Fonts menu expanded from 2 to 6 items
  - CheckItem compares both font_id and font_size
  - Files: main.c, main.h, telnet.r
- [x] Proportional font rendering
  - Detect proportional fonts in term_ui_set_font()
  - Per-character MoveTo+DrawChar for Chicago/Geneva alignment
  - Files: terminal_ui.c
- [x] Window resizing with grow box
  - inGrow handler in handle_mouse_down() with GrowWindow()
  - do_window_resize() computes grid, snaps to cell boundaries, clamps cursor
  - Grow icon drawn clipped in handle_update()
  - Files: main.c, main.h
- [x] Terminal buffer increase (132x50 max)
  - TERM_MAX_COLS=132, TERM_MAX_ROWS=50
  - TERM_DEFAULT_COLS=80, TERM_DEFAULT_ROWS=24
  - MIN_WIN_COLS=20, MIN_WIN_ROWS=5
  - Files: terminal.h, terminal.c
- [x] NAWS renegotiation on resize
  - Sends updated window size when resizing while connected
  - Files: main.c
- [x] Username auto-login in Connect dialog
  - Auto-sends username on connect with delay
  - Saved in prefs v6
  - Files: connection.c, connection.h, settings.c, settings.h, telnet.r
- [x] charCode-based arrow key fallback for M0110A keyboards
  - Files: main.c
- [x] Clear terminal screen on remote disconnect
  - Files: main.c

## Phase 16: Control Menu + Keystroke Buffering (~100 lines)

Adds a dedicated menu for sending control sequences and fixes character loss
during fast typing.

- [x] Control menu (MENU 132, "Control")
  - 6 items: Send Ctrl-C, Ctrl-D, Ctrl-Z, Escape, Ctrl-L, Send Break
  - Break sends Telnet IAC BRK (0xFF 0xF3)
  - Items enabled only when connected (update_menus)
  - Files: main.c, main.h, telnet.r
- [x] Keystroke buffering
  - key_send_buf[256] with buffer_key_send() and flush_key_send()
  - All conn_send() calls in handle_key_down() replaced with buffer_key_send()
  - Event queue draining: GetNextEvent loop collects all pending keyDown events
  - Single flush after draining — one TCP send for N keystrokes
  - Eliminates per-keystroke _TCPSend blocking (~2-10ms each)
  - Files: main.c

## Summary

| Phase | Feature | New Code | Memory | Dependencies | Status |
|-------|---------|----------|--------|-------------|--------|
| 9 | Terminal Foundations | ~600 lines | +4KB | None | **Done** |
| 10 | Line Drawing + VT220 | ~194 lines | +0 bytes | Phase 9 | **Done** |
| 11 | Bookmarks | ~350 lines | +1.3KB | None (parallel) | **Done** |
| 12 | Font Selection | ~200 lines | +20 bytes | None (parallel) | **Done** |
| 13 | xterm Compat | ~100 lines | +0 bytes | Phase 10 | **Done** |
| 14 | UTF-8 Support | ~250 lines | +~1KB code | Phase 10 | **Done** |
| 15 | Fonts + Resizing | ~250 lines | +29KB | Phase 12 | **Done** |
| 16 | Control + Buffering | ~100 lines | +256 bytes | None | **Done** |

All phases 9-16 are complete as of v1.0.0. Phase 16 added a Control menu
for sending control sequences and keystroke buffering to eliminate character
loss during fast typing.
