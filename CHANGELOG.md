# Changelog

All notable changes to this project will be documented in this file.

## [0.8.0] - 2026-03-06

### Added
- DEC Special Graphics line-drawing characters via QuickDraw
  - `draw_line_char()` renders box-drawing glyphs using MoveTo/LineTo
  - 18 character mappings: corners (┌┐└┘), tees (├┤┬┴), cross (┼),
    horizontal (─), vertical (│), diamond (◆), checkerboard (▒),
    centered dot (·), degree (°), plus/minus (±)
  - Bold attribute renders thicker lines (PenSize 2,1)
  - Inverse attribute renders white-on-black (PaintRect + patBic)
  - Unknown graphic characters fall back to regular text rendering
- TUI apps now display proper box-drawing borders (tmux, mc, dialog)

### Changed
- Version: 0.7.0 → 0.8.0
- terminal_ui.c draw_row() dispatches ATTR_DEC_GRAPHICS runs to draw_line_char()
- Build size: ~80KB (up from ~78KB)

## [0.7.0] - 2026-03-06

### Added
- Alternate screen buffer (DECSET ?1049/?1047/?47)
  - Save/restore main screen (3.8KB static buffer in Terminal struct)
  - Scrollback frozen during alt screen (vi, nano, less don't pollute history)
  - Clear on switch, restore on switch back
- OSC window title (ESC]0;title BEL / ESC]2;title ST)
  - Window title bar shows "Flynn - <title>" when set by remote host
  - Reverts to "Flynn - <host>" when title is empty
- DCS string consumption (PARSE_DCS state, consumed until ST)
- Application cursor keys (DECCKM, DECSET ?1)
  - Arrow keys send ESC O A/B/C/D when enabled (vi, tmux navigation)
- Auto-wrap toggle (DECAWM, DECSET ?7)
  - No-autowrap mode overwrites at right margin instead of wrapping
- Origin mode (DECOM, DECSET ?6)
  - Cursor positioning relative to scroll region when enabled
- Insert/Replace mode (IRM, CSI 4 h/l)
  - Insert mode shifts line right before placing characters
- Character set designation (ESC ( 0/B, ESC ) 0/B)
  - G0/G1 charset tracking, ATTR_DEC_GRAPHICS bit (0x08) in TermCell.attr
- SI/SO charset switching (0x0E/0x0F)
  - Shift Out activates G1, Shift In activates G0
- DECSC/DECRC extended (ESC 7/8 now save/restore charsets, origin mode, autowrap)
- Secondary DA response (CSI > c → ESC[>1;10;0c, VT220)
- Primary DA upgraded to VT220 (ESC[?62;1;6c, was VT100 ESC[?1;2c)
- Soft reset (DECSTR, CSI ! p)
  - Resets attributes, charsets, modes, scroll region, cursor
- CSI s/u cursor save/restore (ANSI alternative to ESC 7/8)
- Bracketed paste mode (DECSET ?2004)
  - Pasted text wrapped in ESC[200~/ESC[201~ markers when enabled
- F-key support (F1-F12)
  - ADB virtual keycodes for extended keyboards
  - Cmd+1..0 sends F1-F10 for M0110 keyboards without function keys
- SGR extended color fix
  - 256-color (38;5;N) and truecolor (38;2;R;G;B) sub-parameters now skipped
    cleanly instead of misinterpreted as blink/underline
- Additional SGR attribute mappings for monochrome
  - Dim (2) clears bold, italic (3) maps to underline, blink (5/6) maps to bold
  - Bright foreground (90-97) maps to bold
  - Reset codes: not-italic (23), blink-off (25)

### Changed
- Version: 0.6.1 → 0.7.0
- DA response identifies as VT220 (was VT100)
- 8-state parser (added PARSE_OSC, PARSE_OSC_ESC, PARSE_DCS)
- Terminal struct gains ~4KB for alternate screen buffer, charset state,
  DEC modes, OSC buffer, and window title fields
- Build size: ~78KB (up from ~70KB)

## [0.6.1] - 2026-03-06

### Changed
- Reduced WaitNextEvent timeout from 5 ticks (83ms) to 1 tick (17ms) for faster
  character echo and improved interactive responsiveness
- Replaced `memset(pb, 0, sizeof(*pb))` in `_TCPStatus()` with explicit field
  initialization, eliminating ~200 bytes of unnecessary zeroing per event loop
  iteration (matches wallops-146 reference pattern)
- Cached `sel.active` flag per row in `draw_row()`, avoiding 80 function calls
  to `term_ui_sel_contains()` per row when no selection is active
- Cached `TextFace()` value in `draw_row()` to skip redundant Toolbox calls
  for consecutive attribute runs with the same style
- Version: 0.6.0 → 0.6.1

## [0.6.0] - 2026-03-06

### Added
- Mouse-based text selection in terminal window
  - Click-and-drag to select text (stream selection, not rectangular)
  - Double-click to select word (contiguous non-space characters)
  - Shift-click to extend selection from cursor to click point
  - Selection renders as inverse video (XOR ATTR_INVERSE per cell)
  - Already-inverse cells render as normal when selected (double inversion)
  - Cursor blink suppressed during active selection
- Selection-aware copy (Cmd+C)
  - If text is selected: copies only selected text (partial first/last rows)
  - If no selection: copies entire visible 80x24 screen (existing behavior)
  - Trailing spaces trimmed per row, CR between rows
- Selection automatically cleared on keypress or incoming server data
- New functions in terminal_ui: Selection struct, 11 public API functions
- GetDblTime via low-memory global 0x02F0 (Retro68 compatibility)

### Changed
- Version: 0.5.1 → 0.6.0
- Build size: ~70KB (up from ~65KB)
- terminal_ui.c draw_row() now computes effective attributes per cell for selection overlay

## [0.5.1] - 2026-03-06

### Added
- Cmd+. sends Escape to remote host (classic Mac "Cancel" convention)
- Clear/NumLock key (vkey 0x47) sends Escape (available on M0110A keypad)
- charCode 0x1B fallback sends Escape from any source (USB/ADB keyboards)

### Fixed
- Escape key had no effect in Snow emulator — the M0110A keyboard has no
  physical Escape key, so Snow's `akm0110::translate` drops host Escape
  key events entirely. Cmd+. and Clear key now provide Escape functionality.
- Host Ctrl key had no effect in Snow — the M0110 has no physical Ctrl key.
  Option+letter (existing fix from 0.5.0) remains the correct way to send
  control characters on M0110/M0110A keyboards.

## [0.5.0] - 2026-03-05

### Added
- Menu state management (`update_menus()` in main.c)
  - Connect grayed out when connected, Disconnect grayed out when disconnected
  - Copy/Paste enabled only when connected
  - `SystemEdit()` call for desk accessory support
- Scrollback viewing with keyboard navigation
  - Cmd+Up/Down scrolls one line, Cmd+Shift+Up/Down scrolls one page
  - Window title shows "[Scroll: -N]" indicator when scrolled back
  - New incoming data automatically returns to live view
  - `terminal_get_display_cell()` reads from scrollback ring buffer
- Copy/paste support via Mac clipboard (scrap)
  - Cmd+C copies visible 80x24 screen text (trailing spaces trimmed per row)
  - Cmd+V pastes clipboard text to connection in 256-byte chunks
  - Copy is scrollback-aware (uses display cells, not live screen)
- Proper About dialog (DLOG 130)
  - "Flynn", "Version 0.5.0", description, copyright, credits
  - Uses `GetNewDialog()`/`ModalDialog()`/`DisposeDialog()`
- Settings persistence (`settings.c`/`settings.h`)
  - `FlynnPrefs` struct saved to "Flynn Prefs" file (type 'pref', creator 'FLYN')
  - Host and port remembered across launches
  - Connect dialog pre-fills from saved preferences
- Option key as Ctrl modifier for M0110 keyboard
  - `optionKey` modifier bit mapped to control characters (`key & 0x1F`)
  - Fixes ESC and Ctrl+key on real M0110 keyboard (no physical Ctrl key)
- Quit confirmation alert when connected ("Disconnect and quit?")
- Connection lost notification alert when remote host closes connection
- Cursor visibility control (DECTCEM: ESC[?25h / ESC[?25l)
- Device Attribute response (DA: ESC[c → ESC[?1;2c, VT100 with AVO)
- Device Status Report response (DSR: ESC[6n → cursor position report, ESC[5n → OK)
- Generic ALRT 128 resource with OK/Cancel buttons for alerts

### Changed
- Version: 0.4.0 → 0.5.0
- New source files: `settings.c`, `settings.h`
- Resource file expanded with About dialog (DLOG/DITL 130) and alert (ALRT/DITL 128)
- Terminal struct gains `cursor_visible`, `response[]`, `response_len` fields

## [0.4.0] - 2026-03-05

### Added
- Dedicated terminal UI renderer (`terminal_ui.c`/`terminal_ui.h`)
  - Monaco 9pt font, 6×12 pixel character cells with 2px margins
  - Batched `DrawText()` calls scanning for attribute runs per row
  - Bold via `TextFace(bold)`, underline via `TextFace(underline)`
  - Inverse video via `PaintRect` + `srcBic` transfer mode
  - XOR block cursor with ~30-tick blink interval
  - Per-row dirty flags for efficient partial redraw
  - Direct drawing in null event handler (bypasses BeginUpdate/EndUpdate)
- Full keyboard input mapping in `handle_key_down()`
  - Arrow keys → VT100 escape sequences (`ESC[A`/`B`/`C`/`D`)
  - Home, End, Page Up, Page Down, Forward Delete
  - Delete/Backspace → DEL (0x7F), Return → CR (0x0D), Tab → HT (0x09)
  - Escape key (0x1B), Ctrl+key combinations
  - Cmd+key menu shortcuts preserved

### Fixed
- **Screen freeze after row 24 (critical)**: `_TCPStatus()` was the only
  TCP function missing `memset(pb, 0, sizeof(*pb))`. Stale parameter block
  data caused incorrect `amtUnreadData` reporting, which led `_TCPRcv()`
  (30-second blocking timeout) to hang the entire event loop after the
  terminal scrolled. Fixed by adding the memset and reducing the timeout
  to 1 second.
- Switched from `term_ui_invalidate()` (InvalRect → updateEvt) to direct
  `term_ui_draw()` calls in the null event handler for immediate rendering

### Changed
- Build size: 64KB (up from 61KB)
- All 14 test scenarios now pass (echo, arrow keys, Ctrl+C, nano, vi)

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
