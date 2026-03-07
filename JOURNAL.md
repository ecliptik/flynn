# Development Journal

A living document recording the development of Flynn, a Telnet client for classic Macintosh, built with agentic AI (Claude Code).

---

## 2026-03-07 — v1.0.0: Control Menu, Keystroke Buffering, and 1.0

### The Control Menu

On a real Macintosh Plus with its M0110 keyboard, there's no physical Ctrl key. Flynn already maps Option+letter to Ctrl+letter, but testing on a Linux laptop through the Snow emulator revealed that neither Ctrl nor Command keys reliably pass through. The solution: a dedicated Control menu with the six most useful control sequences — Ctrl-C (interrupt), Ctrl-D (EOF), Ctrl-Z (suspend), Escape, Ctrl-L (clear screen), and Break (Telnet IAC BRK). Menu items are only enabled when connected.

### Keystroke Buffering

Fast typing — whether human or automated — was losing characters. The root cause was three compounding bottlenecks: synchronous `_TCPSend()` blocking for 2-10ms per keystroke, only one event processed per main loop iteration (with 50-200ms of null event processing for drawing between iterations), and the System 6 event queue overflowing at ~20 events.

The fix drains all pending key events from the Mac event queue into a 256-byte buffer, then flushes them in a single TCP send. This eliminates per-keystroke TCP blocking while keeping single keystrokes interactive — when you type one character, it sends immediately because there's only one event to drain.

The first implementation buffered until the null event handler, which was reliable but felt laggy — characters didn't appear relative to when they were typed. The final approach flushes immediately after draining all pending key events in the keyDown handler, giving the best of both worlds: no character loss and responsive echo.

### Version 1.0.0

After 16 phases of development — from an 8.5KB skeleton to a ~98KB full-featured terminal client — Flynn reaches 1.0. It connects to modern Linux servers over Telnet, emulates a VT220/xterm terminal well enough to run vi, nano, tmux, and Midnight Commander, handles UTF-8, renders box-drawing characters via QuickDraw, supports 6 fonts, resizable windows up to 132x50, session bookmarks, dark mode, custom DNS, username auto-login, keystroke buffering, and a Control menu — all running on a 1986 Macintosh Plus with 4MB of RAM.

---

## 2026-03-06 — v0.11.0: Fonts, Resizable Windows, and Polish

### Expanding Beyond Monaco

Flynn started with two font choices: Monaco 9pt (the workhorse, giving 80x24) and Monaco 12pt (larger, fewer cells). This release adds four more: Courier 10, Chicago 12, Geneva 9, and Geneva 10. The interesting challenge was that Chicago and Geneva are proportional fonts — characters have different widths.

The initial implementation used `DrawText()` for batched rendering, which works beautifully for monospace fonts. But with proportional fonts, `DrawText()` advances the pen by each glyph's actual width, while the cursor stays on the fixed-width cell grid. The result: text progressively drifts left of where the cursor thinks it is.

The fix detects proportional fonts at `term_ui_set_font()` time (comparing `widMax` to individual character metrics) and switches to per-character `MoveTo()`/`DrawChar()` rendering. Each character is explicitly positioned at its cell's x-coordinate, keeping text aligned with the grid regardless of glyph width. It's slower than batched `DrawText()`, but on a terminal that refreshes at most a few dozen rows per frame, the difference is imperceptible.

### Draggable Window Resizing

The terminal window now has a grow box in the lower-right corner. Drag to resize, and Flynn snaps to the nearest cell boundary, recomputes the grid dimensions, and — if connected — sends a NAWS update so the remote server knows the new terminal size. This means you can resize during a tmux or vi session and have everything reflow correctly.

To support this, the terminal buffer was increased from the fixed 80x24 to a maximum of 132x50. The default is still 80x24, but you can expand all the way to 132 columns — the classic DEC VT100 wide mode column count. This adds about 29KB of memory, bringing the total to ~60KB, still just 1.5% of the Mac Plus's 4MB.

The grow box is drawn clipped to avoid the characteristic scroll bar lines that appear when you use `DrawGrowIcon()` without clipping — a classic Mac UI detail.

### Username Auto-Login

A small but useful convenience: the Connect dialog now has a Username field. If you fill it in, Flynn automatically sends the username after connecting (with a short delay for the login prompt to appear). Combined with saved bookmarks, this means you can get from launch to a shell prompt in two clicks.

### The M0110A Arrow Key Fix

Discovered that M0110A keyboards (the Mac Plus keyboard with numeric keypad) send arrow key events with charCode values rather than virtual keycodes in some configurations. Added a charCode fallback path: if the virtual keycode doesn't match known arrow keys, check the charCode (0x1C-0x1F) before falling through to normal character handling.

### Current State

Version 0.11.0 — 15 phases complete. Flynn has grown from an 8.5KB skeleton to a ~90KB full-featured terminal client with 6 font choices, resizable windows up to 132x50, session bookmarks, dark mode, custom DNS resolver, username auto-login, UTF-8 support, and comprehensive VT220/xterm terminal emulation. All running on a 1986 Macintosh Plus with 4MB of RAM.

---

## 2026-03-06 — v0.10.0/v0.10.1: Preferences, Dark Mode, DNS

### Preferences Menu and Dark Mode

Expanded the simple Font menu into a full Preferences menu with font selection, terminal type (xterm/VT220/VT100), and a dark mode toggle. Dark mode on a monochrome Mac is elegantly simple: globally XOR the ATTR_INVERSE flag. White text on black. The cursor blink (patXor) is self-inverting, so it works identically in both modes.

### Custom DNS Resolver

The dnrp code resource — Apple's DNS resolver loaded as a CODE resource via `OpenResolver()` — crashes under Retro68 when lookups fail. The root cause is a calling convention mismatch: dnrp uses A5-relative pascal callbacks that don't work with Retro68's flat code model. Rather than debug legacy code resource loading, replaced it entirely with `dns.c` — a clean UDP-based DNS resolver that sends A-record queries to a configurable server (default 1.1.1.1).

---

## 2026-03-06 — v0.9.1: The Finishing Touches

### What Changed

Four small but meaningful improvements that make Flynn feel like a real shipping application:

1. **Custom Finder icon** — A 32x32 monochrome pixel art Macintosh Plus with a white screen and black `>_` prompt. This took three iterations to get right. The first attempt was too dark (a solid CRT monitor shape). The second used a dark screen with white text, but the user pointed out that Flynn actually uses a white background — so the final version shows a Mac Plus with a white screen, exactly as Flynn looks when running. The icon is registered via BNDL/FREF resources so the Finder displays it instead of the generic application diamond.

2. **TeachText Read Me** — A 274-line documentation file covering usage, features, all keyboard shortcuts (including M0110 mappings), bookmarks, font selection, troubleshooting, and credits. Deployed as a TEXT/ttxt file so it opens with TeachText. Had to install TeachText itself onto the System 6 image first (it wasn't included in the base install — copied from the System Tools floppy via hfsutils).

3. **Font menu rename** — The "Settings" menu was renamed to "Font" since it only contains Monaco 9 and Monaco 12 options. Simple but accurate.

4. **BinHex archive** — build.sh now generates Flynn.hqx alongside Flynn.bin. BinHex 4.0 is a text-safe encoding that preserves resource forks, useful for distributing Mac files via email or web.

### Deployment in a Folder

Moved from loose files at the HFS volume root to a `:Flynn:` folder containing the application, Read Me, and preferences file. Updated build.sh with the new deployment instructions.

### Icon Design Process

Designing a 32x32 monochrome icon as hex data was an interesting exercise. Used Python to generate the bitmap data and ASCII preview, then hand-tuned pixels for recognizability. The icon needed to read as "Macintosh Plus" at the tiny Finder grid size — the key elements are the CRT outline, white screen area, `>_` cursor prompt, floppy disk slot, and two small feet at the bottom.

### Team Structure

Used a five-agent team: Team Lead, Software Architect, Build Engineer, Technical Writer, and QA Engineer. The strict Snow emulator ownership rule (only QA Engineer touches the emulator) continued to prevent conflicts. One near-miss when the Build Engineer almost took a screenshot — caught and redirected.

### Git Signing Disabled

Discovered that git commit signing was configured but the RSA hardware token wasn't in the ssh-agent. Permanently disabled signing per user preference (`git config --global commit.gpgsign false`).

---

## 2026-03-06 — Phases 11-14: Bookmarks, Font, xterm, UTF-8 (v0.9.0)

### The Big Features

Four phases implemented together to bring Flynn to feature-complete status:

**Session Bookmarks (Phase 11)** — The most complex UI work in the project. A bookmark manager dialog with a UserItem-based list (the classic Mac way to do custom list rendering), Add/Edit/Delete/Connect buttons, and up to 8 stored bookmarks. Bookmarks also appear directly in the Session menu for one-click connection. This required extracting `conn_connect()` from the dialog-based connection flow so bookmarks could connect without showing the connect dialog.

The preferences file format migrated through three versions: v1 (host/port), v2 (+ bookmarks), v3 (+ font). The migration code handles reading older formats gracefully.

**Font Selection (Phase 12)** — Monaco 9pt (the default, giving 80x24) and Monaco 12pt (larger text, fewer columns). The Font menu shows a checkmark on the active choice. Changing fonts calls `GetFontInfo()` to measure actual glyph metrics, recomputes cell dimensions, resizes the window with `SizeWindow()`, and if connected, sends a NAWS update to the server so it knows the new terminal size.

**xterm Compatibility (Phase 13)** — TTYPE cycling (xterm → VT220 → VT100), application keypad mode for numpad keys, and silent consumption of mouse reporting and focus event modes that modern shells enable but a 1-bit Mac can't use.

**UTF-8 Support (Phase 14)** — A decode-and-translate approach. The UTF-8 decoder state machine feeds Unicode codepoints through three lookup paths: box-drawing → DEC Special Graphics (for tmux/mc borders), Latin-1 → Mac Roman (for accented characters), and a symbol table (em dash, curly quotes, bullet, etc.). Wide characters and emoji get a 2-cell `??` placeholder with modifier sequence absorption. This means Flynn can display most real-world UTF-8 terminal output meaningfully, even on a 1986 Mac Plus.

### Bug Fixes

- **Screen not cleared on disconnect** — `do_disconnect()` wasn't resetting terminal state, leaving the last screen contents visible after disconnecting.
- **Overlapping strncpy** — `conn_connect()` was called with `host` pointing into the same `conn.host` buffer it was copying to. Undefined behavior that happened to work on some calls but not others.
- **UTF-8 fallback character** — Was using Mac Roman 0xB7 which displays as Σ (sigma) instead of a sensible fallback. Changed to `?`.

### Memory Budget

The complete Flynn application with all Phase 11-14 features uses ~30KB of RAM — 0.7% of the Mac Plus's 4MB. The largest consumers are the scrollback ring buffer (15.4KB), alternate screen buffer (3.8KB), and TCP buffer (12.3KB).

---

## 2026-03-06 — Phase 10: DEC Special Graphics (v0.8.0)

### Box-Drawing via QuickDraw

The most visually impactful single feature: rendering box-drawing characters using QuickDraw line primitives instead of trying to find them in a font. `draw_line_char()` handles 18 glyphs — corners, tees, cross, horizontal/vertical lines, diamond, checkerboard, and a few symbols. Bold lines render thicker via `PenSize(2,1)`. Inverse renders white-on-black.

After this phase, tmux borders, Midnight Commander panels, and dialog boxes all render with proper line-drawing instead of question marks or garbled characters. This was the single biggest improvement in visual fidelity.

### OSC Title Bug

A subtle parsing bug: the semicolon separator in OSC sequences (e.g., `ESC]0;Window Title BEL`) was being included in the accumulated title string because `osc_param` was modified during digit parsing before the semicolon check ran. Fixed by restructuring the parse order.

---

## 2026-03-06 — Phase 9: Alt Screen, DEC Modes, and F-Keys (v0.7.0)

### Terminal Foundations

The largest single commit in the project — Phase 9 added all the terminal modes and features needed for full-screen applications:

- **Alternate screen buffer**: vi, nano, less, and tmux use `DECSET ?1049` to switch to a clean screen and restore the original on exit. A 3.8KB static buffer in the Terminal struct stores the main screen contents during alt-screen mode. Scrollback is frozen so alt-screen programs don't pollute history.

- **Application cursor keys (DECCKM)**: When enabled, arrow keys send `ESC O A/B/C/D` instead of `ESC [ A/B/C/D`. This is how vi and tmux distinguish cursor movement from typed escape sequences.

- **Character sets**: G0/G1 charset designation and SI/SO switching, with `ATTR_DEC_GRAPHICS` bit in TermCell.attr flagging cells for line-drawing rendering. This is the protocol-level mechanism that Phase 10's `draw_line_char()` renders.

- **Bracketed paste**: Wraps pasted text in `ESC[200~`/`ESC[201~` when the mode is enabled, so shells can distinguish pasted text from typed input.

- **F-keys**: ADB virtual keycodes for extended keyboards (native F1-F12) plus Cmd+1..0 for M0110 keyboards that lack function keys entirely.

- **Soft reset (DECSTR)**: `CSI ! p` resets terminal to a sane default state — used by programs that want a clean slate without a full reset.

The parser grew from 5 to 8 states (adding OSC, OSC_ESC, and DCS), and the DA response was upgraded from VT100 to VT220 identity.

---

## 2026-03-06 — Phase 8: Performance (v0.6.1)

A focused optimization pass. The key changes:

- **WaitNextEvent timeout**: 5 ticks → 1 tick (83ms → 17ms). Characters now echo almost instantly instead of having a perceptible delay.
- **_TCPStatus() field init**: Replaced `memset` with explicit field initialization, saving ~200 bytes of zeroing per event loop iteration.
- **draw_row() caching**: `sel.active` flag cached once per row (avoids 80 function calls when no selection exists), and `TextFace()` value cached to skip redundant Toolbox calls for consecutive same-style runs.

Small changes, but they add up when the event loop runs 60 times per second.

---

## 2026-03-06 — Phase 7: Mouse Text Selection (v0.6.0)

### Teaching a 1986 Mac to Select Text

Added three selection modes to the terminal window:

1. **Click-drag**: Stream selection following the text flow (not rectangular block selection). Tracks mouse position during `mouseDrag` events and continuously redraws the selection highlight.

2. **Double-click word selection**: Expands both directions from the click point through contiguous non-space characters. Uses the Mac Toolbox's `GetDblTime` interval for double-click detection — accessed via a low-memory global at 0x02F0 since Retro68 doesn't expose it through the normal API.

3. **Shift-click extend**: Extends an existing selection to the new click point, keeping the original anchor fixed.

Selection renders as inverse video — each selected cell gets its `ATTR_INVERSE` flag XORed, so already-inverse cells (like status bars) appear normal when selected. Cmd+C copies only the selected text when a selection exists, or the full screen otherwise. Selection auto-clears on keypress or new server data.

Also renamed the "File" menu to "Session" — better reflecting its actual contents (Connect, Disconnect, Bookmarks, Quit).

---

## 2026-03-05 — Phase 6: Polish, Preferences, and the M0110 Keyboard

### Six-Agent Team

Used Claude Teams with six specialized agents, including stricter role separation for emulator access:

- **Team Lead** — Coordinated work, managed task dependencies, made architectural decisions
- **Software Architect** — Implemented all six Phase 6 sub-tasks (menu states, scrollback, copy/paste, About dialog, settings, polish)
- **Build Engineer** — Built Flynn with Retro68, deployed to HFS disk image via hfsutils (no emulator access)
- **QA Engineer** — Sole operator of the Snow emulator, ran all GUI automation and testing
- **Technical Writer** — Updated all project documentation
- **UI/UX QA Engineer** — Wrote automated test scripts

A key lesson from Phase 5 was that multiple agents should never control the same emulator instance. The CLAUDE.md was updated to enforce Snow emulator ownership: only the QA Engineer may launch, interact with, or take screenshots of Snow.

### Menu State Management (6A)

Added `update_menus()` to main.c — a simple but important UI polish:

- When connected: Connect grayed out, Disconnect enabled, Copy/Paste enabled
- When disconnected: Connect enabled, Disconnect grayed out, Copy/Paste grayed out
- `SystemEdit()` call added for desk accessory support in the Edit menu
- Called after `do_connect()`, `do_disconnect()`, and connection state changes

Uses `EnableItem()`/`DisableItem()` (Retro68's names for the Toolbox calls).

### Scrollback Viewing (6B)

Extended the Terminal struct with `scroll_offset` and added `terminal_get_display_cell()`:

- `scroll_offset = 0` means live view; `scroll_offset > 0` means scrolled back N lines
- `terminal_get_display_cell()` maps display rows to either the scrollback ring buffer or the live screen buffer, depending on the offset
- Cmd+Up/Down scrolls one line, Cmd+Shift+Up/Down scrolls a full page (24 lines)
- New incoming data resets `scroll_offset` to 0 (returns to live)
- Window title shows "Flynn [-N]" when scrolled back
- Cursor is only drawn when `scroll_offset == 0`

The scrollback navigation is intercepted in `handle_key_down()` before `MenuKey()` processing, since Cmd+arrow keys would otherwise be consumed by the menu system.

### Copy/Paste (6C)

Implemented via the classic Mac scrap (clipboard) API:

- **Copy (Cmd+C)**: Iterates the 80x24 display grid using `terminal_get_display_cell()` (scrollback-aware), trims trailing spaces per row, joins with CR, and puts the result on the scrap via `ZeroScrap()`/`PutScrap()`.
- **Paste (Cmd+V)**: `GetScrap()` retrieves TEXT data, sends it to the connection in 256-byte chunks via `conn_send()`. The chunk size prevents overwhelming the TCP send buffer.

Both operations are wired into the Edit menu handler and only available when connected (enforced by menu state management).

### About Dialog (6D)

Replaced the simple `ParamText()`/`Alert()` with a proper DLOG resource:

- DLOG 130 at `{80, 100, 260, 400}` — centered on screen
- DITL 130 with OK button and five static text fields: "Flynn", "Version 0.5.0", description, copyright ("2026 Micheal Waltz"), and credits ("Built with Claude Code + Retro68")
- `do_about()` uses `GetNewDialog()`/`ModalDialog()`/`DisposeDialog()`

### Settings Persistence (6E)

New `settings.c`/`settings.h` module:

- `FlynnPrefs` struct: `version` (short), `host[256]` (char), `port` (short)
- `prefs_load()`: Opens "Flynn Prefs" file from the current volume, reads the struct, validates the version number, falls back to defaults (empty host, port 23)
- `prefs_save()`: Deletes and recreates the file with type 'pref', creator 'FLYN'
- In `do_connect()`: pre-fills `conn.host`/`conn.port` from saved prefs, and saves after a successful connection
- Uses classic Mac File Manager calls: `GetVol()`, `FSOpen()`, `FSRead()`, `FSWrite()`, `FSClose()`, `Create()`, `FSDelete()`

### Keyboard and Polish (6F)

The most impactful fix addresses a real usability issue reported via BUGS.md:

- **M0110 keyboard fix**: The Macintosh Plus M0110 keyboard has no physical Ctrl key. Added `optionKey` as a Ctrl modifier — `Option+key` now computes `key & 0x1F`, making ESC (via backtick area), Ctrl+C, Ctrl+D, and all other control characters accessible. This was critical for using vi, nano, and shell job control.
- **Quit confirmation**: When connected, File > Quit shows a `CautionAlert` asking "Disconnect and quit?" with OK/Cancel
- **Connection lost detection**: `main_event_loop()` tracks `prev_state` and detects when `conn.state` transitions from CONNECTED to IDLE, showing an alert
- **DECTCEM cursor visibility**: Terminal now tracks `cursor_visible` flag, responding to ESC[?25l (hide) and ESC[?25h (show). The UI layer checks this before drawing the cursor.
- **DA/DSR responses**: Terminal generates response bytes for Device Attributes (ESC[c → ESC[?1;2c) and Device Status Report (ESC[6n → cursor position, ESC[5n → OK status). These are stored in `response[]`/`response_len` fields on the Terminal struct.

### Automated Testing

The QA engineer created two new test modules:

- **`tests/snow_automation.py`** — Reusable X11 XTEST automation library for Snow emulator. Auto-calibrates framebuffer coordinates via PIL screenshot analysis (coordinates vary per Snow session). Supports XTEST incremental mouse motion, click-hold-drag for Mac menus, Cmd+key and Option+key (Ctrl mapping), and text typing. Uses `scrot` for screenshots (never `import -window root`, which breaks Snow mouse input).

- **`tests/test_phase6.py`** — 20 automated test cases covering all Phase 6 features: menu state management, scrollback navigation, copy/paste, About dialog, settings persistence, keyboard fixes (ESC, Ctrl+C via Option+C), window title changes, and regression tests (echo, arrows, nano, vi). Tests are screenshot-based since Mac screen memory can't be read directly from the host. Run with `python3 tests/test_phase6.py` (requires `FLYNN_TEST_HOST`, `FLYNN_TEST_USER`, `FLYNN_TEST_PASS` env vars).

### Emulator Ownership

A key process improvement: the CLAUDE.md was updated to enforce strict Snow emulator ownership. Only the QA Engineer may launch, interact with, or take screenshots of Snow. The Build Engineer deploys to the HFS image but never runs `snowemu`. This prevents the conflicts that occurred in earlier phases when multiple agents tried to control the same emulator instance.

### Current State

Phases 0–6 complete. Flynn is a fully featured telnet client for classic Macintosh: connects to real servers, negotiates telnet options, renders VT100 terminal output with text attributes, handles full keyboard input (including M0110 via Option-as-Ctrl), provides scrollback viewing and copy/paste, saves preferences, and includes proper dialogs and alerts. Version 0.5.0.

---

## 2026-03-05 — Phase 5: Terminal UI, Keyboard Mapping, and the TCP Bug

### Five-Agent Team

Used Claude Teams with five specialized agents working in parallel:

- **Team Lead** — Coordinated work, made architectural decisions, managed git
- **Software Architect** — Designed and implemented `terminal_ui.c`/`terminal_ui.h` (rendering module, text attributes, cursor blink, dirty-region invalidation)
- **Build Engineer** — Built Flynn, deployed to HDD image via hfsutils, launched Snow emulator, ran automated GUI tests via X11 XTEST
- **Documentation Writer** — Updated TESTING.md, README.md, and other docs
- **QA Engineer** — Wrote `test_launch.py` automated test script using python-xlib for X11 XTEST mouse/keyboard simulation

### Terminal UI Renderer (terminal_ui.c)

The architect created a dedicated rendering module separating display logic from the terminal emulator:

- **Font**: Monaco 9pt (the classic Mac monospace font), 6×12 pixel cells with 2px margins
- **Dirty tracking**: Per-row dirty flags — only redraws rows that changed
- **Attribute runs**: Scans each row for contiguous spans of identical attributes, issues one `DrawText()` call per span. Bold and underline use `TextFace()`. Inverse video uses `PaintRect()` + `srcBic` transfer mode (paints the cell black, then XORs the text).
- **Cursor**: XOR block cursor that blinks every ~30 ticks (~0.5 seconds)
- **Drawing strategy**: Direct `term_ui_draw()` calls from the null event handler, bypassing the `InvalRect` → `BeginUpdate`/`EndUpdate` mechanism. This was a key fix — the update event approach had timing issues with dirty flags being cleared before the update handler could read them.

### Keyboard Mapping

The build engineer implemented full VT100 keyboard mapping in `handle_key_down()`:

- Mac virtual keycodes (from `event->message`) mapped to VT100 escape sequences
- Arrow keys send `ESC[A`/`B`/`C`/`D`, Home/End send `ESC[H`/`ESC[F`
- Page Up/Down and Forward Delete send extended sequences (`ESC[5~`, `ESC[6~`, `ESC[3~`)
- Delete/Backspace sends DEL (0x7F), which is what Unix expects
- Ctrl+key computes the control character (`key & 0x1F`)
- Cmd+key is intercepted before any of this for menu shortcuts
- Retro68 uses `ControlKey` (capital C) — discovered during compilation

### The TCP Blocking Bug

After the initial deployment, Flynn connected to the telnet server and rendered the login banner and neofetch output perfectly — all 24 rows of ASCII art. But then the screen froze. Typing worked (characters were sent to the server), but no new output appeared on screen.

**Initial hypothesis (wrong)**: The `InvalRect()` → `updateEvt` invalidation path wasn't generating repaints after scrolling past row 24. Switched to direct drawing — this helped with responsiveness but didn't fix the freeze.

**Root cause**: The build engineer discovered it was a complete event loop hang, not a rendering issue. `_TCPStatus()` in `tcp.c` was the **only** TCP function that didn't `memset(pb, 0, sizeof(*pb))` before use. Stale data in the parameter block caused Snow's MacTCP emulation to return incorrect `amtUnreadData` values. When `conn_idle()` then called `_TCPRcv()` with its 30-second `commandTimeoutValue`, the entire application blocked — no events processed, no screen updates, nothing.

**Fix**: Two lines:
1. Added `memset(pb, 0, sizeof(*pb))` to `_TCPStatus()` (matching every other TCP function)
2. Reduced `_TCPRcv()` `commandTimeoutValue` from 30 to 1 second (safety net)

After the fix, all 14 test scenarios passed: echo commands, arrow keys for shell history, Ctrl+C to interrupt processes, `ls --color` with bold text, and full-screen TUI applications (nano with inverse status bars, vi with insert/normal mode switching).

### Automated Testing

The QA engineer created `test_launch.py`, a python-xlib script that drives the Snow emulator through X11 XTEST:

- Clicks on Flynn in the Finder list, opens it with Cmd+O
- Opens File > Connect, types the server IP, presses Return
- Types username/password at the login prompt
- Runs through 12 test scenarios (echo, arrows, Ctrl+C, nano, vi, disconnect)
- Takes screenshots at each step for verification
- Uses environment variables (`FLYNN_TEST_HOST`, `FLYNN_TEST_USER`, `FLYNN_TEST_PASS`) to avoid hardcoded credentials

### Repository Made Public

Before making the repo public on GitHub (as a read-only mirror of the Forgejo origin):
- Scrubbed all hardcoded credentials and private URLs from source and git history using `git-filter-repo`
- Replaced with placeholders in docs, env vars in test scripts
- Stored real credentials in local Claude memory (not committed)

### Current State

Phases 0–5 complete. Flynn is a functional telnet client: connects to real servers, negotiates telnet options, renders VT100 terminal output with text attributes, handles keyboard input with full arrow key / Ctrl / escape support, and runs full-screen TUI applications. Build size is 64KB.

---

## 2026-03-05 — Telnet Protocol and VT100 Terminal Engine

### Rebuilding Retro68 and Fixing Compatibility

The Retro68 toolchain at `/opt` had been lost, so it was rebuilt from source directly in-repo (`Retro68-build/`, gitignored). Building with `--no-ppc --no-carbon` for 68K-only took about 40 minutes and produced GCC 12.2.0.

The newer GCC + Multiverse.h headers introduced several incompatibilities with the wallops-146 code:

- **MacTCP.h**: The `kPascalStackBased` enum conflicted with `Multiverse.h`. Fix: guard with `#ifndef STACK_ROUTINE_PARAMETER` (can't use `#ifndef` on enum values — only works on `#define` macros).
- **API renames**: Retro68 uses newer Toolbox names (`GetDialogItem` not `GetDItem`, `DisposePtr` not `DisposPtr`, `PBControlSync` not `PBControl`). The PB functions also changed from 2-arg `(pb, async)` to 1-arg `Sync`/`Async` variants — fixed with dispatch macros.
- **Missing headers**: `Folders.h` and `GestaltEqu.h` don't exist as separate files; their contents are in `Multiverse.h`.

All of this was documented in `memory/retro68-compat.md` for future reference.

### Three-Agent Team for Phase 3+4

Used a team of three parallel agents to implement the telnet protocol engine, VT100 terminal emulator, and emulator testing simultaneously:

1. **Telnet Protocol Agent** — Studied subtext-596's server-side `telnet.c` and wrote a client-side implementation. The key insight is inverting the negotiation: where the server sends WILL, the client responds DO; where the server sends DO, the client responds WILL. The engine processes raw TCP bytes and produces clean terminal output + IAC responses as separate byte arrays, making it completely transport-independent.

2. **Terminal Display Agent** — Wrote a full VT100 escape sequence parser with a 5-state machine (NORMAL, ESC, CSI, CSI_PARAM, CSI_INTERMEDIATE). The 80×24 screen buffer uses 2 bytes per cell (character + attributes), totaling ~4KB. A 96-line scrollback ring buffer adds ~15KB. The entire `Terminal` struct is ~19KB — less than 0.5% of the Mac Plus's 4MB RAM.

3. **Emulator Test Agent** — Deployed Flynn.bin to the Mac HDD via `hfsutils`, booted Snow, and verified the application appears in the Finder. Testing revealed the previous 51KB build (pre-telnet) ran correctly.

### Integration

Wired the three modules into the main event loop:
```
TCP receive → telnet_process() → terminal_process() → InvalRect → redraw
```

IAC responses from `telnet_process()` are immediately sent back via `conn_send()`. Terminal output is fed to the VT100 parser, which updates the screen buffer and marks dirty rows. A basic Monaco 9pt renderer draws the buffer contents on update events.

The full build is now 61KB — up from 51KB with connection dialog, up from 8.5KB for the original skeleton.

### Current State

Phases 0–4 core modules are complete. The data pipeline works end-to-end in code: connect → IAC negotiate → parse VT100 → render. Next: test with an actual telnet server to verify the pipeline works in practice, then build the proper terminal UI rendering module.

---

## 2026-03-05 — Project Inception and Phase 0 Complete

### Planning with a Research Team

The project began with a four-agent research team, each focused on a different domain:

1. **Telnet Protocol Researcher** — Deep dive into RFC 854/855/857/858/1091/1073 and the subtext-596 reference code. Mapped out the complete client-side IAC negotiation flow, identified the minimum viable option set (BINARY, ECHO, SGA, TTYPE, NAWS, TSPEED), and documented how to invert subtext's server-side telnet.c for our client implementation. Key insight: modern Linux telnetd sends ~10 option requests immediately on connect; we only need to accept 6 and reject the rest.

2. **Classic Mac Programmer** — Studied wallops-146 (IRC client) and subtext-596 (BBS server) exhaustively. Documented the exact Mac Toolbox initialization sequence (MaxApplZone must be last!), WaitNextEvent patterns with adaptive wait times, MacTCP wrapper API lifecycle, keyboard handling, and memory management for a 4MB Mac Plus. Identified which files can be reused directly (tcp.c, dnr.c, util.c) vs. adapted (telnet.c) vs. built from scratch (VT100 parser, terminal UI).

3. **Build System Researcher** — Researched the Retro68 cross-compilation toolchain: prerequisites, build steps, CMake integration, resource file compilation with Rez, and the `-m68000` flag for Mac Plus compatibility. Found that MacTCP.h is not included in Retro68's Multiversal Interfaces and must be supplied from the wallops reference code.

4. **QA/Testing Researcher** — Planned the complete Basilisk II emulator setup: SLiRP networking (NAT, no root needed), `extfs` shared filesystem for deploying builds, MacTCP manual IP configuration, and a 9-phase progressive test plan from smoke test through full vi/nano interactive testing.

All four agents worked in parallel and delivered comprehensive reports in about 15 minutes.

### Environment Setup

A second team handled Phase 0 execution:

- **Retro68 Toolchain**: Installed prerequisites via apt, cloned the repo, and built with `--no-ppc --no-carbon` (68K only). Produced `m68k-apple-macos-gcc` (GCC 12.2.0), Rez resource compiler, and MakeAPPL bundler.

- **Basilisk II**: Not in Debian apt repos; built from source (github.com/cebix/macemu). Configured with Mac Plus v3 ROM, the existing mac608.hda System 6.0.8 disk image, SLiRP networking, and `extfs` pointing to the build directory.

- **Project Scaffolding**: Created src/, include/, resources/ structure. Copied MacTCP wrapper files (tcp.c/h, dnr.c/h, util.c/h) from wallops-146 with all original ISC license headers preserved. Created main.c skeleton with correct Toolbox init, event loop, and menu handling.

### First Build

The initial build attempt revealed several Retro68-specific differences from classic Mac Toolbox:

- **No `Desk.h` header** — Retro68 uses Multiversal Interfaces. `SystemClick` and `OpenDeskAcc` are in `Multiverse.h`.
- **`qd.thePort` not `thePort`** — QuickDraw globals are accessed through the `qd` struct.
- **`qd.screenBits` not `screenBits`** — Same pattern for screen bounds.
- **`GetMenuHandle` not `GetMHandle`** — Newer API names (old names exist as macros but weren't resolving).
- **`AppendResMenu` not `AddResMenu`** — Same renaming pattern.
- **`GetApplLimit` linker error** — Used `LMGetApplLimit()` low-memory accessor macro instead.

After fixing these, the build succeeded. Output: Flynn.bin (8.5KB MacBinary), Flynn.dsk (800KB floppy image).

### Attribution

Both reference projects (wallops and subtext) are by joshua stein (jcs@jcs.org), released under the ISC license. All copied files retain original copyright headers. A comprehensive LICENSE file documents all derived code and upstream copyrights (including University of Illinois for tcp.c and Apple for dnr.c).

### Current State

Phase 0 is complete. The skeleton app builds successfully with Retro68. Next step: boot it in Basilisk II to verify it actually runs on the emulated Mac.

---

## 2026-03-05 — Switching to Snow Emulator and Installing System 6.0.8

### Why Snow Instead of Basilisk II

Basilisk II hit a dead end: all its 512KB ROMs force 32-bit addressing, which System 6.0.8 doesn't support, and every method of programmatically clicking the resulting dialog failed. Switched to [Snow](https://snowemu.com/) (v1.3.1), a Rust-based emulator with low-level hardware emulation, Mac Plus support, and DaynaPORT SCSI/Link Ethernet for MacTCP networking.

### GUI Automation: Teaching Claude to Use a Mac

Developed a method for Claude to operate the Snow emulator entirely through X11 automation — taking screenshots to see the screen, then using `xdotool` to click and type. This required switching from KDE to WindowMaker (KDE lacks `_NET_ACTIVE_WINDOW`), discovering that `xdotool click` doesn't work with Snow's egui (must use explicit `mousedown`/`mouseup` pairs), and learning the different interaction patterns for Mac menus (press-hold-drag) vs Snow's host menus (regular click).

A comprehensive automation guide was written at `docs/SNOW-GUI-AUTOMATION.md`.

### Automated System 6.0.8 Installation

Using the GUI automation techniques, Claude performed the entire System 6.0.8 installation unattended — launching the Installer from a floppy, clicking through the UI, and swapping four floppy disk images via Snow's Drives menu as the Installer requested them. The result is a bootable 90MB SCSI hard drive image at `diskimages/snow-sys608.img`.

Creating the SCSI drive image itself required trial and error — three failed approaches before finding a pre-formatted blank HFS image with the proper Apple partition map and SCSI driver. See `docs/DEVLOG.md` for the full technical details and `docs/screenshots/` for step-by-step screenshots.

### What's Next

- Boot from HDD without floppy to verify standalone boot
- Install MacTCP 2.1 and configure DaynaPORT networking
- Transfer and test the Flynn skeleton app in the emulator
