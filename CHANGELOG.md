# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- Multiple simultaneous sessions (up to 4) in separate windows
  - Session struct bundles Window, Connection, TelnetState, Terminal per session
  - UIState save/load pattern for per-session cursor blink and text selection
  - Per-connection idle polling skip counter (moved from file-scope static)
- Window menu (MENU 133, between Edit and Preferences)
  - Close Window (Cmd+W) closes active session window
  - Dynamic window list with checkmark on active session
  - Clicking between windows switches active session
- File menu (renamed from "Session")
  - "New Session..." (Cmd+N, was "Connect...") — auto-creates new window if
    current session is already connected
  - "Close Session" (was "Disconnect") — disconnects active session
  - Bookmarks open in new session if current is connected
- Per-session event routing via window refCon
  - Mouse events routed via FindWindow → session_from_window
  - Key events routed to front window's session
  - Update/activate events routed via WindowPtr
- Font changes and dark mode toggle apply to all session windows
- Quit confirms if any session is connected, destroys all on exit
- Close box on individual windows closes just that session

### Changed
- SIZE resource increased from 512KB/384KB to 640KB/512KB
- Memory per session: ~73KB (Terminal 52KB + Connection 12.5KB + TCP 8KB + misc)
- New source files: `session.c`, `session.h`
- Single-session performance overhead: ~100µs per idle tick (negligible)

## [1.1.1] - 2026-03-09

### Added
- 11 box-drawing primitive glyphs (U+2500-U+253C): ─│┌┐└┘├┤┬┴┼, drawn with edge-to-edge QuickDraw LineTo for seamless tiling
- 3 shade characters (U+2591-U+2593): ░▒▓, rendered with QuickDraw ltGray/gray/dkGray fill patterns
- Glyph count: 51 → 65 primitives (15 emoji unchanged)

### Fixed
- MacBinary II CRC-16 recalculated after creator code patch, fixing "application is busy or damaged" deployment errors

### Changed
- Move build.sh and release.sh into scripts/ directory
- Dev builds use SHA-only naming (Flynn-d724f2b), tagged releases use version-only (Flynn-1.1.1)
- Replace CRT-filtered screenshots with clean cropped captures
- Clean up documentation: remove redundant DEVLOG, trim README, consolidate screenshots
- Mark ROADMAP and AUDIT docs as historical

## [1.1.0] - 2026-03-08

### Added
- Unicode glyph rendering system (`glyphs.c`/`glyphs.h`)
  - 51 QuickDraw primitive glyphs: arrows, geometric shapes, block elements,
    card suits, musical notes, mathematical symbols, and more — drawn natively
    with LineTo, PaintRect, PaintOval, PaintPoly, and PaintArc
  - 15 monochrome 10x10 bitmap emoji rendered via CopyBits (2-cell wide):
    grinning face, heart eyes, thumbs up, fire, star, checkmark, crossmark,
    warning, lightning, sun, moon, skull, rocket, party popper, sparkles
  - Braille pattern rendering (U+2800-U+28FF) — 256 dot-grid patterns drawn
    as filled circles in a 2x4 grid per cell
  - Binary search codepoint-to-glyph lookup table for fast mapping
  - Bold variants: thicker lines (PenSize 2,1) and larger fills for primitives
  - U+00B7 middle dot rendered as centered PaintOval (was misrouted through
    Latin-1 Mac Roman table producing wrong character)

### Fixed
- Bold text spacing drift: DrawText() with bold TextFace increases advance
  width by 1px per character, causing cumulative rightward drift that consumed
  adjacent spaces. Replaced all DrawText calls with per-character
  MoveTo+DrawChar for absolute positioning (draw_row, draw_line_char fallback,
  draw_glyph_prim fallback)
- DCS parser content leak: ESC inside DCS string shared PARSE_OSC_ESC state,
  causing DCS content to leak as visible text. Added dedicated PARSE_DCS_ESC
  state
- CSI parameter overflow: TERM_MAX_PARAMS was 8, but truecolor SGR sequences
  (38;2;R;G;B foreground + 48;2;R;G;B background) need 12+ params. Increased
  to 16 with bounds checking
- CSI colon sub-parameter parsing for SGR 38:2:R:G:B truecolor sequences
  (colon-separated variant)
- UTF-8 error recovery: new start byte mid-sequence now resets decoder and
  reprocesses (was producing garbage output)
- Invisible Unicode characters (ZWSP, ZWJ, variation selectors, combining
  marks, BOM) now absorbed silently instead of rendering as '?'
- Unicode spaces (en space, em space, thin space, hair space) mapped to ASCII
  space instead of '?'

### Changed
- Version: 1.0.1 → 1.1.0
- Terminal parser expanded to 9 states (added PARSE_DCS_ESC)
- TERM_MAX_PARAMS: 8 → 16
- New source files: `glyphs.c`, `glyphs.h`
- Build size: ~110KB (up from ~98KB, glyph tables and bitmap data added)

## [1.0.1] - 2026-03-07

### Fixed
- TCP connect timeout enabled (commandTimeoutValue = 30 seconds)
  - Previously commented out, causing ~5 minute UI freeze when
    networking was unavailable (no command timeout = wait forever)

### Changed
- Verified compatibility with System 7.5.5 and Open Transport
- Updated README, About Flynn, and CHANGELOG to document
  System 7 / Open Transport support

## [1.0.0] - 2026-03-07

### Added
- Control menu with 6 items for sending control sequences without a
  physical Ctrl key:
  - Send Ctrl-C, Send Ctrl-D, Send Ctrl-Z, Send Escape, Send Ctrl-L,
    Send Break (Telnet IAC BRK)
  - Menu items enabled only when connected
  - MENU resource 132 added, MBAR updated to include it
- Keystroke buffering to prevent character loss during fast typing
  - Drains all pending key events from the Mac event queue before sending
  - Batches multiple keystrokes into a single TCP send (reduces per-key
    blocking from synchronous _TCPSend)
  - Single keystrokes still send immediately for interactive feel
  - key_send_buf[256] with buffer_key_send() and flush_key_send()

### Changed
- README: Added Claude Code attribution, navigation TOC, and expanded
  Acknowledgments (Retro68, Snow emulator as individual entries)
- About dialog version updated to 1.0.0 (updated to 1.0.1 in next release)
- Version: 0.11.1 → 1.0.0

## [0.11.0] - 2026-03-06

### Added
- Additional font options: Courier 10, Chicago 12, Geneva 9, Geneva 10
  - Preferences > Fonts menu expanded from 2 to 6 items
  - CheckItem logic compares both font_id and font_size
- Window resizing with grow box
  - Drag-to-resize terminal window, snaps to cell boundaries
  - Terminal buffer increased to 132x50 max (from 80x24)
  - Min window size: 20x5 cells
  - Grow icon drawn clipped (avoids scroll bar lines)
  - NAWS renegotiation sent on resize while connected
- Proportional font rendering
  - Detects proportional fonts (Chicago, Geneva) in `term_ui_set_font()`
  - Uses per-character MoveTo+DrawChar to keep text aligned with cell grid
- Username auto-login field in Connect dialog
  - Sends username automatically on connect (with short delay)
  - Username saved in preferences (v5→v6 migration) and pre-filled
- charCode-based arrow key fallback for M0110A keyboards
- Clear terminal screen on remote disconnect
  - Resets terminal and clears window before showing alert

### Fixed
- Cursor misalignment with proportional fonts (Chicago, Geneva)
  - DrawText advanced pen by actual glyph width, not cell width
- Initial window too large on launch (clamped to 80x24 default, not 132x50 max)
- Connect dialog labels improved ("Host or IP:", info text removed)

### Changed
- Version: 0.10.1 → 0.11.0
- FlynnPrefs bumped to v6 (adds username field)
- TERM_MAX_COLS/TERM_MAX_ROWS: 132x50, TERM_DEFAULT_COLS/TERM_DEFAULT_ROWS: 80x24
- Memory impact: buffer increase adds ~29KB (total ~60KB, within 4MB limit)

## [0.10.1] - 2026-03-06

### Added
- Custom DNS resolver using MacTCP UDP (`dns.c`/`dns.h`)
  - Bypasses broken `dnrp` code resource (crashes on failed lookups due to
    Retro68 calling convention mismatch)
  - Sends A-record queries to configurable DNS server (default: 1.1.1.1)
  - 15-second timeout per attempt, 2 retry attempts
  - Parses DNS responses including CNAME chains
  - Error-specific alerts: "Host not found", "DNS lookup timed out",
    "DNS lookup failed"
- DNS Server preference dialog (DLOG 133)
  - Accessible from Preferences > Networking > DNS Server...
  - User can set any IP address as their DNS server
  - Validates IP format before saving
  - Persisted in FlynnPrefs (dns_server field)
- Hostname validation in Connect dialog
  - Rejects malformed IPs (e.g., doubled addresses like "1.2.3.41.2.3.4")
  - Validates DNS label length and character rules
- `_UDPRcv()` and `_UDPBfrReturn()` wrappers in tcp.c

### Changed
- Version: 0.10.0 → 0.10.1
- Preferences menu reorganized into labeled sections:
  Fonts, Terminal Type, Networking, Misc (disabled section headers)
- FlynnPrefs bumped to v5 (adds dns_server[16] field)
- Connection struct gains dns_server field for per-connection DNS config
- tcp.h converted from CR to LF line endings

## [0.10.0] - 2026-03-06

### Added
- Preferences menu (replacing Font menu, MENU 131)
  - Terminal type selection: xterm, VT220, VT100 with checkmark
  - Dark mode toggle for white-on-black rendering
  - Font selection (Monaco 9/12) retained from Font menu
- Dark/light mode rendering
  - XOR-based inverse on monochrome display (black bg, white text)
  - DEC Special Graphics line-drawing works in both modes
  - Cursor blink (patXor) works identically in both modes
- ICON resource in About dialog (32x32 mono, same as Finder icon)
- Preferred terminal type in TTYPE negotiation
  - User-selected type sent first, then cycles through remaining types
- Preferences v3→v4 migration (terminal_type, dark_mode fields)

### Changed
- Version: 0.9.1 → 0.10.0
- Font menu renamed to Preferences menu with expanded items
- About dialog layout: taller (215px), consistent margins, icon added
- 800K floppy image (Flynn.dsk) now includes Read Me and correct FLYN creator code
- FlynnPrefs struct gains terminal_type and dark_mode fields
- TelnetState gains preferred_ttype field
- Build size: ~90KB (estimated)

## [0.9.1] - 2026-03-06

### Added
- Custom Finder icon (ICN#/BNDL/FREF resources)
  - 32x32 monochrome CRT terminal with >_ prompt
  - BNDL and FREF associate icon with creator code 'FLYN'
  - Signature resource ('FLYN' 0) for Finder identification
- "Flynn Read Me" TeachText documentation
  - Usage guide, features, keyboard shortcuts, M0110 key mappings
  - Troubleshooting, bookmarks, font selection, credits
  - Deployed as TEXT/ttxt to HFS image alongside Flynn
- BinHex (.hqx) archive output in build
  - build.sh generates Flynn.hqx for cross-platform distribution
  - Text-safe encoding preserves resource fork via email/web/BBS
- About dialog now shows https://www.ecliptik.com
- Flynn deployed in its own folder on HFS disk (:Flynn:)

### Changed
- Version: 0.9.0 → 0.9.1
- Settings menu renamed to "Font" (only contains font options)
  - MENU 131 title, all constants and variables renamed
- build.sh now converts Read Me to Mac CR line endings and prints
  folder-based deployment instructions
- Build size: ~88KB (up from ~87KB, icon resources added)

## [0.9.0] - 2026-03-06

### Added
- Session bookmarks (Phase 11)
  - Bookmark struct (name, host, port) stored in preferences, max 8
  - Bookmark manager dialog (DLOG 131) with Add/Edit/Delete/Connect buttons
  - UserItem-based list display with click selection and inverse highlight
  - Add/Edit bookmark dialog (DLOG 132) with Name/Host/Port fields
  - Dynamic Session menu: bookmarks appear below Quit separator
  - One-click connect from Session menu bookmark items
  - Preferences v1→v2→v3 migration for bookmark and font fields
  - Extracted `conn_connect()` from `conn_open_dialog()` for direct connect
- Font selection (Phase 12)
  - Settings menu (MENU 131) with Monaco 9 and Monaco 12 options
  - Runtime cell dimensions via `GetFontInfo()` measurement
  - `term_ui_set_font()` changes font and recomputes grid
  - `active_cols`/`active_rows` in Terminal struct for dynamic grid sizing
  - Window resized with `SizeWindow()` to fit computed grid
  - NAWS renegotiation sent on font change while connected
  - Font preference saved/restored across launches (font_id, font_size)
  - Checkmark on active font in Settings menu
- xterm compatibility (Phase 13)
  - Application keypad mode (DECKPAM/DECKPNM, ESC =/ESC >)
  - Numpad keys send SS3 sequences in application mode
  - TTYPE cycling: xterm → VT220 → VT100 (first response is "xterm")
  - Silent consumption of mouse reporting modes (?1000-1006)
  - Silent consumption of focus events (?1004) and cursor blink (?12)
- UTF-8 support (Phase 14)
  - UTF-8 decoder state machine (2/3/4-byte sequences)
  - Unicode box-drawing (U+2500-U+257F) → DEC Special Graphics (33 mappings)
  - Unicode Latin-1 Supplement (U+0080-U+00FF) → Mac Roman (128-entry table)
  - Unicode symbols → Mac Roman (em/en dash, curly quotes, bullet, ellipsis,
    euro, pi, delta, sqrt, infinity, trademark, not-equal, etc.)
  - Wide character/emoji → 2-cell `??` placeholder
  - Emoji modifier/ZWJ/skin-tone/variation-selector sequence absorption
  - Unmapped codepoints render as `?` fallback

### Fixed
- Screen not cleared on disconnect: `do_disconnect()` now calls
  `terminal_reset()`, `telnet_init()`, and redraws window
- Overlapping `strncpy` UB in `conn_connect()` when called from
  `conn_open_dialog()` — host parameter pointed to same buffer
- UTF-8 fallback character was Mac Roman 0xB7 (Σ sigma) instead of `?`

### Changed
- Version: 0.8.0 → 0.9.0
- Session menu expanded: Connect, Disconnect, Bookmarks, Quit + dynamic bookmarks
- MBAR resource updated to include Settings menu (128, 129, 130, 131)
- Prefs version bumped to 3 (bookmark_count, bookmarks[8], font_id, font_size)
- ~40 TERM_COLS/TERM_ROWS references in terminal.c changed to active_cols/active_rows
- Build size: ~87KB (up from ~77KB)

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

### Fixed
- OSC title parsing: semicolon separator was included in title string
  (e.g. ";Test Title" instead of "Test Title") due to osc_param being
  modified during digit parsing, causing the semicolon check to be skipped

### Changed
- Version: 0.7.0 → 0.8.0
- terminal_ui.c draw_row() dispatches ATTR_DEC_GRAPHICS runs to draw_line_char()
- Build size: ~77KB (up from ~76KB)

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
