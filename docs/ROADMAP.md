# Flynn Feature Roadmap

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
- [ ] draw_line_char() in terminal_ui.c
  - QuickDraw MoveTo/LineTo for box-drawing characters
  - 15+ character mappings (corners, tees, crosses, horizontal, vertical)
  - Files: terminal_ui.c
- [x] Update DA response to VT220 (ESC[?62;1;6c) *(done in Phase 9)*
  - Files: terminal.c
- [ ] Change TTYPE from "VT100" to "VT220"
  - Files: telnet.c

## Phase 11: Session Bookmarks (~250 lines, +1.3KB prefs)

Independent of terminal changes — pure UI/UX feature. Can be done in parallel.

- [ ] Bookmark struct (name[32], host[128], port), max 8
  - Files: settings.h
- [ ] Extended FlynnPrefs with v1→v2 migration
  - Files: settings.h, settings.c
- [ ] Dynamic Session menu bookmark items
  - AppendMenu + SetMenuItemText below Quit
  - Files: main.c, main.h
- [ ] Bookmark manager dialog (DLOG 131)
  - List Manager list + Add/Edit/Delete buttons
  - Files: main.c, flynn.r
- [ ] Add/Edit bookmark dialog (DLOG 132)
  - Name, Host, Port fields
  - Files: main.c, flynn.r
- [ ] One-click connect from bookmark menu item
  - Refactor conn_open_dialog() → extract conn_connect()
  - Files: connection.c, connection.h, main.c

## Phase 12: Font Selection (~300 lines)

Most invasive change — touches terminal.c extensively (~40 TERM_COLS/TERM_ROWS refs).

- [ ] Replace CELL_WIDTH/CELL_HEIGHT with runtime variables
  - Files: terminal_ui.h, terminal_ui.c
- [ ] term_ui_set_font() with GetFontInfo measurement
  - Files: terminal_ui.c
- [ ] Add active_cols/active_rows to Terminal struct
  - Static arrays stay at max 80×24, use subset
  - Files: terminal.h, terminal.c (~40 occurrences)
- [ ] Settings menu (MENU 131) with font items
  - Monaco 9 (6×12, 80×24), Monaco 12 (7×15, ~70×20)
  - Optional Courier 10 if installed
  - Files: main.c, main.h, flynn.r
- [ ] Font fields in FlynnPrefs (font_id, font_size)
  - Files: settings.h, settings.c
- [ ] NAWS renegotiation on font/grid change
  - Files: main.c, telnet.c
- [ ] Window resizing for new grid dimensions
  - Files: main.c

## Phase 13: xterm Compatibility (~150 lines, incremental)

After VT220 is solid — adds xterm-specific features.

- [x] Bracketed paste mode (DECSET ?2004) *(done in Phase 9)*
  - Wrap pasted text in ESC[200~/ESC[201~ markers
  - Files: terminal.h, terminal.c, main.c
- [x] OSC window title → SetWTitle *(done in Phase 9)*
  - Parse OSC 0/1/2, extract title, update Mac window
  - Files: terminal.c, main.c
- [ ] Application keypad mode (ESC = / ESC >)
  - Files: terminal.c, main.c
- [ ] Change TTYPE from "VT220" to "XTERM"
  - Files: telnet.c
- [x] F-key support *(done in Phase 9)*
  - ADB virtual keycodes for extended keyboards
  - Cmd+1..0 for M0110 keyboards
  - Files: main.c
- [ ] Silently consume remaining modes
  - Mouse reporting (?1000-1006), focus events (?1004), cursor blink (?12)
  - Files: terminal.c

## Summary

| Phase | Feature | New Code | Memory | Dependencies | Status |
|-------|---------|----------|--------|-------------|--------|
| 9 | Terminal Foundations | ~600 lines | +4KB | None | **Done** |
| 10 | Line Drawing + VT220 | ~100 lines | +0 bytes | Phase 9 | Remaining: draw_line_char, TTYPE |
| 11 | Bookmarks | ~250 lines | +1.3KB | None (parallel) | Planned |
| 12 | Font Selection | ~300 lines | +20 bytes | None (parallel) | Planned |
| 13 | xterm Compat | ~50 lines | +0 bytes | Phase 10 | Remaining: keypad mode, TTYPE, mouse consume |

Phase 9 was larger than estimated (~600 vs ~200 lines) because it pulled in charset
designation, SI/SO, ATTR_DEC_GRAPHICS, F-keys, bracketed paste, and OSC window title
from Phases 10 and 13. This reduces the remaining work for those phases.

Phases 11 and 12 are independent and can be developed in parallel with 10.
