# Development Log

## 2026-03-06: v0.11.0 — Additional Fonts, Window Resizing, Auto-Login

### Additional Fonts (Courier 10, Chicago 12, Geneva 9, Geneva 10)

Expanded the Preferences > Fonts menu from 2 items (Monaco 9/12) to 6, adding Courier 10 (font_id=22), Chicago 12 (font_id=0), Geneva 9 (font_id=3), and Geneva 10 (font_id=3). All PREFS_* menu constants renumbered. CheckItem logic compares both font_id and font_size for correct checkmark behavior.

### Proportional Font Rendering Fix

Chicago and Geneva are proportional fonts — `DrawText()` advances the pen by each character's actual width rather than the cell width. This caused text to render compressed while the cursor stayed on the wider widMax grid, creating visible misalignment. Fixed by detecting proportional fonts in `term_ui_set_font()` (comparing widMax to character metrics) and switching to per-character `MoveTo()`/`DrawChar()` rendering that forces each character onto its cell grid position. Slightly slower than batched `DrawText()`, but pixel-accurate.

### Window Resizing with Grow Box

Added drag-to-resize for the terminal window:
- `inGrow` handler in `handle_mouse_down()` calls `GrowWindow()` with min/max size limits
- `do_window_resize()` computes grid from pixel dimensions, snaps to cell boundaries, clamps cursor, redraws, and sends NAWS if connected
- Grow icon drawn clipped in `handle_update()` to avoid scroll bar artifacts
- Refactored `do_font_change()` to delegate to `do_window_resize()` for consistent behavior

Terminal buffer increased from 80x24 to 132x50 max. This adds ~29KB of memory (total ~60KB), still well within the 4MB Mac Plus limit. New constants: `TERM_MAX_COLS=132`, `TERM_MAX_ROWS=50`, `TERM_DEFAULT_COLS=80`, `TERM_DEFAULT_ROWS=24`, `MIN_WIN_COLS=20`, `MIN_WIN_ROWS=5`.

### Username Auto-Login

Added a Username field to the Connect dialog (DLOG 129). When provided, Flynn auto-sends the username after connecting (with a short delay for the login prompt to appear). Username is saved in preferences (v5→v6 migration) and pre-filled on subsequent connections.

### Other Fixes

- charCode-based arrow key fallback for M0110A keyboards (handles keyboard layouts that send arrow charCodes instead of virtual keycodes)
- Clear terminal screen on remote disconnect — resets terminal state and clears window before showing the connection lost alert
- Initial window clamped to 80x24 default (was incorrectly using 132x50 max buffer size)
- Connect dialog labels improved ("Host or IP:", removed redundant info text)

---

## 2026-03-06: v0.10.1 — Custom DNS Resolver, Preferences Reorganization

### Custom DNS Resolver (dns.c)

The dnrp code resource (Apple's DNS resolver loaded via `OpenResolver()`) crashes when DNS lookups fail under Retro68. Root cause: calling convention mismatch between Retro68's flat code model and the dnrp resource's A5-relative pascal callbacks. Rather than debug the code resource loading, replaced it entirely with a custom UDP-based DNS resolver.

`dns_resolve()` sends A-record queries to a configurable DNS server (default 1.1.1.1) over MacTCP UDP, with 15-second timeout and 2 retry attempts. Parses standard DNS response format including CNAME chains. Error-specific alerts: "Host not found" (NXDOMAIN), "DNS lookup timed out", "DNS lookup failed".

### DNS Server Preference

Added DLOG 133 for DNS Server configuration, accessible from Preferences > Networking > DNS Server. User can set any IP address as their DNS server. Validated before saving, persisted in FlynnPrefs.

### Preferences Menu Reorganization

Restructured the Preferences menu with disabled section headers:
- Fonts: Monaco 9, Monaco 12
- Terminal Type: xterm, VT220, VT100
- Networking: DNS Server...
- Misc: Dark Mode

### Hostname Validation

Added validation in the Connect dialog to reject malformed IPs (e.g., doubled addresses like "192.168.7.230192.168.7.230") and invalid DNS labels. Uses `ip2long()` check for all-numeric hostnames.

---

## 2026-03-06: v0.10.0 — Preferences Menu, Dark Mode, Floppy Image

### Preferences Menu

Renamed and expanded the Font menu (MENU 131) into a Preferences menu with font selection, terminal type selection (xterm/VT220/VT100), and dark mode toggle. Terminal type preference controls TTYPE cycling order — user's preferred type is sent first during negotiation. Prefs version 3→4.

### Dark Mode

Implemented as a global XOR of ATTR_INVERSE. On a monochrome Mac, dark mode renders white text on black background. All rendering paths (normal text, DEC Special Graphics, cursor blink) work correctly in both modes. The key insight: patXor-based cursor rendering is mode-invariant since XOR is self-inverting.

### 800K Floppy Image

build.sh now sets the correct FLYN creator code and includes the Read Me file on the Flynn.dsk floppy image, making it ready for use with FloppyEMU or physical disk drives.

### About Dialog Polish

Added ICON resource (32x32 mono, same bitmap as Finder icon), made dialog taller with consistent margins, and added ecliptik.com URL.

---

## 2026-03-06: v0.9.1 — Finder Icon, Read Me, Font Menu, BinHex

### Custom Finder Icon (ICN# 128)

Designed a 32x32 monochrome pixel art icon depicting a Macintosh Plus with a white screen showing a black `>_` prompt. Three iterations: first attempt was too dark (solid CRT), second used dark screen with white prompt, final version matched Flynn's actual white-background aesthetic.

The icon is defined as raw hex `data 'ICN#'` in telnet.r (not as a Rez `resource` — Retro68's Rez doesn't support the `ICN#` type directly). The mask bitmap mirrors the icon outline so the Finder composites it correctly against any desktop pattern.

Associated resources for Finder registration:
- `FREF` (128) — maps file type 'APPL' to icon local ID 0
- `BNDL` (128) — associates creator code 'FLYN' with ICN# and FREF resources
- `FLYN` (0) — signature resource with Pascal string "Flynn - Telnet Client"

Desktop database rebuild (Cmd+Option during boot) is required after adding BNDL resources for the Finder to pick up the new icon.

### TeachText Read Me

Created `docs/Flynn Read Me` — 274 lines of plain ASCII documentation covering usage, features, keyboard shortcuts, M0110 keyboard notes, bookmarks, font selection, troubleshooting, credits, and contact info. Deployed to the HFS image as type 'TEXT', creator 'ttxt' so it opens with TeachText (the System 6 built-in text viewer).

TeachText wasn't installed on the System 6.0.8 HDD image. Mounted the System Tools floppy via hfsutils and copied TeachText to the system volume.

Line endings converted from Unix LF to Mac CR via `tr '\n' '\r'` in build.sh.

### Settings Menu → Font Menu

Renamed MENU 131 from "Settings" to "Font" since it only contains Monaco 9/12 font choices. Updated all constants (`SETTINGS_MENU_ID` → `FONT_MENU_ID`, etc.) and the global variable (`settings_menu` → `font_menu`) in main.h and main.c.

### BinHex Archive Output

Added BinHex (.hqx) generation to build.sh using the `binhex` command from the `macutils` apt package. BinHex 4.0 is a text-safe encoding that preserves both data and resource forks, making it suitable for distribution via email, web, or BBS where binary transfers might corrupt Mac files. The input is Flynn.bin (MacBinary II format, already produced by Retro68's MakeAPPL).

### Folder-Based Deployment

Changed HFS deployment from loose files at the volume root to a `:Flynn:` folder via `hmkdir :Flynn`. The folder contains Flynn (application), Flynn Prefs (created at runtime), and Flynn Read Me (TeachText document).

### About Dialog Update

Added ecliptik.com URL as a new StaticText item in DITL 130. Bumped dialog height and repositioned OK button to accommodate. Version string updated to 0.9.1.

---

## 2026-03-06: Phases 11-14 — Bookmarks, Font, xterm, UTF-8 (v0.9.0)

### Session Bookmarks (Phase 11)

New bookmark manager dialog (DLOG 131) with a UserItem-based list display, Add/Edit/Delete/Connect buttons. Up to 8 bookmarks stored in preferences (version bumped from 1 to 3 with migration). Each bookmark has name, host, port fields.

Key implementation details:
- `bm_list_draw()` — UserItem draw proc renders bookmark list with inverse highlight for selection
- `bm_filter()` — ModalDialog filter handles clicks in the list area
- `bm_edit_dialog()` — Separate DLOG 132 for adding/editing bookmarks
- Dynamic Session menu: bookmarks appear below a separator after Quit, using `AppendMenu()`/`SetMenuItemText()` at startup and after bookmark changes
- `conn_connect()` extracted from `conn_open_dialog()` to allow direct connection from bookmark selection without opening the connect dialog

Preferences migration: `prefs_load()` handles v1 (host/port only), v2 (+ bookmarks), and v3 (+ font) gracefully with field-by-field reads.

### Font Selection (Phase 12)

Added Font menu (MENU 131, initially called "Settings") with Monaco 9pt and Monaco 12pt options:
- `term_ui_set_font()` — Changes font, calls `GetFontInfo()` to measure actual glyph metrics, recomputes cell dimensions and grid size
- `active_cols`/`active_rows` in Terminal struct replace ~40 hardcoded `TERM_COLS`/`TERM_ROWS` references
- `SizeWindow()` resizes the window to fit the computed grid
- NAWS renegotiation sent to server on font change (if connected)
- Font preference saved in FlynnPrefs v3 (font_id, font_size fields)
- Checkmark on active font menu item via `CheckItem()`

### xterm Compatibility (Phase 13)

Extended terminal emulation for better compatibility with modern Linux programs:
- TTYPE cycling: first sub-negotiation returns "xterm", then "VT220", then "VT100" — matches what tmux/vim expect
- Application keypad mode (DECKPAM `ESC =` / DECKPNM `ESC >`) — numpad keys send SS3 sequences in application mode
- Silent consumption of mouse reporting modes (?1000-1006), focus events (?1004), cursor blink (?12)

### UTF-8 Support (Phase 14)

Added a UTF-8 decoder state machine to `terminal_process()`:
- 2/3/4-byte UTF-8 sequences decoded to Unicode codepoints
- Box-drawing (U+2500-U+257F) → DEC Special Graphics characters (33 mappings)
- Latin-1 Supplement (U+0080-U+00FF) → Mac Roman via 128-entry lookup table
- Common symbols → Mac Roman (em/en dash, curly quotes, bullet, ellipsis, euro, pi, delta, trademark, etc.)
- Wide characters and emoji → 2-cell `??` placeholder with modifier/ZWJ/skin-tone sequence absorption
- Unmapped codepoints render as `?` fallback

### Bug Fixes in v0.9.0

- Screen not cleared on disconnect: `do_disconnect()` now calls `terminal_reset()`, `telnet_init()`, redraws
- Overlapping `strncpy` UB in `conn_connect()` when host parameter pointed to same buffer as `conn.host`
- UTF-8 fallback was Mac Roman 0xB7 (Σ sigma) — changed to `?`

---

## 2026-03-06: Phase 10 — DEC Special Graphics (v0.8.0)

### Line-Drawing Characters via QuickDraw

Implemented `draw_line_char()` in terminal_ui.c — renders box-drawing glyphs using QuickDraw `MoveTo()`/`LineTo()` primitives instead of font characters. 18 character mappings: corners (┌┐└┘), tees (├┤┬┴), cross (┼), horizontal (─), vertical (│), diamond (◆), checkerboard (▒), centered dot (·), degree (°), plus/minus (±).

Bold attribute renders thicker lines (`PenSize(2,1)`). Inverse renders white-on-black via `PaintRect` + `patBic`. Unknown DEC graphic characters fall back to regular text. The `draw_row()` function dispatches `ATTR_DEC_GRAPHICS` runs to `draw_line_char()`.

This enables proper box-drawing borders in tmux, Midnight Commander, dialog, and other TUI applications.

### OSC Title Bug Fix

Fixed semicolon appearing in window title (";Test Title" instead of "Test Title"). The `osc_param` variable was modified during digit parsing, causing the semicolon separator check to be skipped.

---

## 2026-03-06: Phase 9 — Alt Screen, DEC Modes, Charsets, F-keys (v0.7.0)

### Alternate Screen Buffer

`DECSET ?1049/?1047/?47` saves main screen to a 3.8KB static buffer, clears the alt screen, and restores on switch back. Scrollback is frozen during alt screen — vi, nano, and less don't pollute history.

### New Terminal Modes

- Application cursor keys (DECCKM ?1): arrows send `ESC O A/B/C/D` for vi/tmux
- Auto-wrap toggle (DECAWM ?7): overwrites at right margin instead of wrapping
- Origin mode (DECOM ?6): cursor positioning relative to scroll region
- Insert/Replace mode (IRM): insert shifts line right before placing characters
- Bracketed paste mode (?2004): wraps pasted text in `ESC[200~`/`ESC[201~`
- Soft reset (DECSTR, `CSI ! p`): resets attributes, charsets, modes, scroll region

### Character Sets

G0/G1 charset tracking with `ESC ( 0/B` and `ESC ) 0/B` designation. SI/SO (0x0E/0x0F) switches between G0 and G1. `ATTR_DEC_GRAPHICS` bit (0x08) in TermCell.attr flags cells for line-drawing rendering.

### F-Key Support

ADB virtual keycodes for extended keyboards (F1-F12 native). Cmd+1..0 sends F1-F10 for M0110 keyboards. Sends standard xterm-style `ESC[11~` through `ESC[24~` sequences.

### Parser and DA Upgrades

- 8-state parser: added PARSE_OSC, PARSE_OSC_ESC, PARSE_DCS states
- OSC window title: `ESC]0;title BEL` / `ESC]2;title ST` sets "Flynn - <title>" in title bar
- DCS string consumption (consumed until ST)
- Primary DA upgraded to VT220 (`ESC[?62;1;6c`)
- Secondary DA response (`CSI > c → ESC[>1;10;0c`)
- Extended DECSC/DECRC saves/restores charsets, origin mode, autowrap
- SGR extended color: 256-color and truecolor sub-parameters now skipped cleanly

---

## 2026-03-06: Phase 8 — Performance Optimizations (v0.6.1)

Reduced WaitNextEvent timeout from 5 ticks (83ms) to 1 tick (17ms) for faster character echo. Replaced `memset` in `_TCPStatus()` with explicit field initialization (~200 bytes saved per event loop iteration). Cached `sel.active` flag per row in `draw_row()` to avoid 80 `term_ui_sel_contains()` calls per row. Cached `TextFace()` value to skip redundant Toolbox calls for consecutive same-style attribute runs.

---

## 2026-03-06: Phase 7 — Mouse Text Selection (v0.6.0)

### Selection Model

Added `Selection` struct to terminal_ui with start/end coordinates, active flag, and anchoring state. Three selection modes:

- **Click-drag**: Stream selection from mouseDown through mouseDrag to mouseUp. Tracks `sel_start` and extends to current mouse position.
- **Double-click**: Word selection — expands both directions from click point through contiguous non-space characters. Uses `GetDblTime` via low-memory global 0x02F0 (Retro68 compat).
- **Shift-click**: Extends existing selection from original anchor to new click point.

### Rendering

Selection renders as inverse video — XORs `ATTR_INVERSE` per cell in `draw_row()`. Already-inverse cells (e.g., status bars) render as normal when selected (double inversion). Cursor blink suppressed during active selection.

### Copy Integration

Cmd+C now checks for active selection first: if present, copies only selected text (partial first/last rows); if none, copies entire visible 80x24 screen (existing behavior). Selection cleared on keypress or incoming server data.

### Session Menu Rename

Renamed "File" menu to "Session" to better reflect its purpose (Connect, Disconnect, Bookmarks, Quit).

---

## 2026-03-05: Phase 6 — Polish, Preferences, Keyboard Fixes

### Menu State Management (main.c)

`update_menus()` enables/disables Connect, Disconnect, Copy, Paste based on `conn.state`. Uses `EnableItem()`/`DisableItem()`. Called after connect, disconnect, and state changes. `SystemEdit(item - 1)` added for DA support.

### Scrollback Viewing (terminal.c, main.c)

Added `scroll_offset` to Terminal struct. `terminal_get_display_cell()` maps display rows to scrollback ring buffer or live screen. Cmd+Up/Down scrolls 1 line, Cmd+Shift+Up/Down scrolls 24 lines. New data resets offset to 0. Window title shows "Flynn [-N]" when scrolled. Cursor hidden while scrolled back.

### Copy/Paste (main.c)

Copy: iterates 80x24 via `terminal_get_display_cell()`, trims trailing spaces, joins with CR, `ZeroScrap()`/`PutScrap('TEXT')`. Paste: `GetScrap('TEXT')`, sends to connection in 256-byte chunks. Both gated by menu state (connected only).

### About Dialog (telnet.r, main.c)

DLOG 130 with DITL 130: OK button + 5 static text items (name, version 0.5.0, description, copyright, credits). `do_about()` uses `GetNewDialog()`/`ModalDialog()`/`DisposeDialog()`.

### Settings Persistence (settings.c/settings.h)

`FlynnPrefs` struct (version, host[256], port). Saved to "Flynn Prefs" file via `FSOpen`/`FSWrite`/`FSClose`. Type 'pref', creator 'FLYN'. `prefs_load()` at startup, `prefs_save()` after successful connect. Pre-fills connect dialog.

### Keyboard Fix: Option as Ctrl (main.c)

M0110 has no Ctrl key. Added `optionKey` modifier check alongside `ControlKey` — `key & 0x1F` for both. Fixes ESC, Ctrl+C/D/X in vi, nano, bash.

### Polish

- Quit confirmation: `CautionAlert` "Disconnect and quit?" when connected
- Connection lost: detect CONNECTED→IDLE transition, show alert
- DECTCEM: `cursor_visible` flag, ESC[?25h/l support in terminal.c and terminal_ui.c
- DA response: ESC[c → ESC[?1;2c (VT100 with AVO)
- DSR response: ESC[6n → cursor position report, ESC[5n → OK

### Automated Testing

`tests/snow_automation.py` — reusable X11 XTEST library for Snow. Auto-calibrates framebuffer coordinates via PIL screenshot analysis. `tests/test_phase6.py` — 20 test cases: menu states, scrollback, copy/paste, About dialog, settings persistence, keyboard fixes, regression (echo, arrows, nano, vi). Screenshot-based verification. Run: `python3 tests/test_phase6.py` (needs `FLYNN_TEST_HOST`, `FLYNN_TEST_USER`, `FLYNN_TEST_PASS`).

### New Files

- `src/settings.c`, `src/settings.h` — preferences persistence
- `tests/snow_automation.py` — X11 XTEST automation library for Snow
- `tests/test_phase6.py` — 20 automated Phase 6 test cases
- ALRT 128 resource — generic alert with OK/Cancel

---

## 2026-03-05: Phase 5 — Terminal UI and TCP Fix

### Terminal UI Renderer (terminal_ui.c/terminal_ui.h)

New dedicated rendering module. Monaco 9pt font, 6×12 character cells, 2px margins. Per-row dirty flags for efficient partial redraw. Attribute runs scanned per row — one `DrawText()` per contiguous span. Bold/underline via `TextFace()`, inverse via `PaintRect` + `srcBic`. XOR block cursor blinks every ~30 ticks.

### Keyboard Mapping (main.c handle_key_down)

Full VT100 keyboard mapping: arrows → `ESC[A`–`ESC[D`, Home/End/PgUp/PgDn/FwdDel, Delete→DEL (0x7F), Ctrl+key→control chars (`& 0x1F`), Cmd+key preserved for menus. Retro68 header uses `ControlKey` not `controlKey`.

### TCP Blocking Bug Fix (tcp.c)

`_TCPStatus()` was missing `memset(pb, 0, sizeof(*pb))` — the only TCP function without it. Stale parameter block data caused bad `amtUnreadData` values. `_TCPRcv()` then blocked for 30 seconds (`commandTimeoutValue`), hanging the entire event loop. Fixed by adding memset and reducing timeout to 1 second.

### Drawing Strategy

Switched from `InvalRect()` → `updateEvt` to direct `term_ui_draw()` in the null event handler. The update event path had timing issues with dirty flags being cleared before the handler read them.

### Test Results

All 14 scenarios pass: launch, connect, login, echo, arrows, Ctrl+C, ls --color, nano, vi, disconnect, quit. Build size: 64KB.

---

## 2026-03-05: Telnet Protocol Engine and VT100 Terminal

### Retro68 Toolchain Rebuild

Rebuilt Retro68 from source in-repo (`Retro68-build/`) with `--no-ppc --no-carbon` for 68K-only. GCC 12.2.0 with newer Multiverse.h headers required extensive compatibility fixes to the wallops-146 code (MacTCP.h enum guards, API renames, missing headers). See `memory/retro68-compat.md` for full details.

### Telnet Protocol Engine (telnet.c/telnet.h)

Client-side IAC negotiation engine adapted from subtext-596's server-side implementation. 9-state parser handles: ECHO, SGA, BINARY, TTYPE ("VT100"), NAWS (80x24), TSPEED ("19200,19200"). Transport-independent — processes byte arrays, no MacTCP coupling.

### VT100 Terminal Emulator (terminal.c/terminal.h)

Full escape sequence parser with 5-state CSI machine. 80x24 screen buffer (2 bytes/cell) + 96-line scrollback ring buffer. Supports cursor movement, screen/line clear, insert/delete, scroll regions, text attributes. ~19KB total struct size.

### Integration

Data pipeline wired in main.c: `conn_idle() → telnet_process() → terminal_process() → InvalRect`. IAC responses sent back immediately. Basic Monaco 9pt text rendering for display. Build size: 61KB.

---

## 2026-03-05: System 6.0.8 Installation via GUI Automation

### SCSI Hard Drive Setup

Created a SCSI hard drive image for the Snow emulator. Snow expects full drive images with Apple partition maps, not just raw HFS volumes.

**Failed approaches:**
1. `hformat` creates raw HFS without Apple partition map — Mac Plus ROM can't mount
2. Custom Python script with DDM + partition map but no driver — Mac can see partitions but not mount
3. Extracting SCSI driver from Apple HD SC Setup — the resource fork contains app init code, not the standalone disk driver

**Working approach:** Downloaded a pre-formatted blank HFS disk image from savagetaylor.com that includes proper DDM (Driver Descriptor Map), Apple_Driver43 partition with SCSI driver binary, and Apple_HFS partition. The image is 90MB.

- Source: `https://www.savagetaylor.com/wp-content/uploads/68k_Macintosh/Bootdisks/blank_100MB_HFS.zip`
- File: `diskimages/snow-sys608.img` (94,371,840 bytes)
- Contains: DDM (sig 0x4552), Apple_Driver43 at blocks 64-95, Apple_HFS at blocks 96+
- Volume name: "Blank 100MB"

### Snow Workspace Configuration

Snow workspace files use the `.snoww` extension (two w's — discovered by reading source at `app.rs:437`). The workspace is a JSON file with paths relative to its parent directory.

Final workspace (`diskimages/flynn.snoww`):
```json
{
  "viewport_scale": 1.5,
  "rom_path": "../roms/68k/128k/Macintosh Plus/1986-03 - 4D1F8172 - MacPlus v3.ROM",
  "scsi_targets": [{"Disk": "snow-sys608.img"}, "None", "None", "None", "None", "None", "None"],
  "model": "Plus",
  "map_cmd_ralt": true,
  "scaling_algorithm": "NearestNeighbor",
  "framebuffer_mode": "Centered",
  "shader_configs": []
}
```

### GUI Automation Discovery

Automated the entire System 6.0.8 installation using X11 tools (`xdotool`, `import`) to interact with the Snow emulator GUI programmatically.

**Key findings:**
- **Window Manager matters:** KDE Plasma doesn't support `_NET_ACTIVE_WINDOW`, breaking `xdotool windowactivate`. Switched to **WindowMaker** (`wmaker`) which works perfectly.
- **`xdotool click` doesn't work with Snow/egui.** Must use explicit `xdotool mousedown 1` + `xdotool mouseup 1` as separate commands.
- **Double-click works** with two mousedown/mouseup pairs separated by ~100ms.
- **Screenshot coordinates = X11 screen coordinates** (1:1 mapping at 1280x800 display).
- **Mac menus** use press-hold-release (mousedown, wait, mouseup).
- **Snow host menus (egui)** use regular clicks.

### Installation Process

1. Launched Snow with workspace + System Tools floppy
2. Mac booted from floppy, showing Finder with System Tools and Blank 100MB drives
3. Double-clicked System Tools floppy icon to open it
4. Double-clicked Installer app inside
5. Apple Installer launched — "Welcome to the Apple Installer" screen
6. Clicked OK → Easy Install screen showing "Version 6.0.8, Macintosh Plus System Software"
7. Target: "Blank 100MB" hard drive
8. Clicked Install → installation began
9. Installer read from System Tools, then requested additional disks:
   - Utilities 2 → loaded via Drives > Floppy #1 > Load image...
   - Utilities 1 → loaded same way
   - Printing Tools → loaded same way
   - System Tools → reloaded for final "Updating disk..."
10. "Installation on 'Blank 100MB' was successful!"
11. Quit Installer, verified hard drive now contains System Folder

### Screenshots

| Screenshot | Description |
|-----------|-------------|
| `03-installer-welcome.png` | Apple Installer welcome screen |
| `04-easy-install.png` | Easy Install — Version 6.0.8, target Blank 100MB |
| `05-installer-reading-system.png` | Reading files from System Tools disk |
| `06-install-successful.png` | Installation successful dialog |
| `07-system608-installed.png` | Blank 100MB drive contents — Desktop Folder, System Folder, Trash |

### Next Steps

- Boot from HDD without floppy (verify standalone boot)
- Install MacTCP 2.1 for networking
- Attach DaynaPORT SCSI/Link Ethernet adapter
- Test Flynn skeleton app in the emulator
