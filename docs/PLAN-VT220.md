# VT220 Terminal Emulation Plan

## 1. VT220 vs VT320: Recommendation

**Target: VT220.** Rationale:

- VT220 is the sweet spot for modern server compatibility. Nearly all TUI applications (vim, nano, tmux, htop, Midnight Commander) work correctly with VT220.
- VT320 adds features rarely used by modern software: 132-column mode (requires window resize on 512px Mac Plus screen), rectangular area operations (DECSERA/DECCRA), and sixel graphics.
- xterm defaults to VT220 compatibility level for most operations.
- On a 68000 Mac Plus with 4MB RAM and monochrome 512x342 display, VT220 is the pragmatic ceiling — the additional VT320 features cannot be meaningfully supported.
- Modern servers treat VT220 and VT320 nearly identically for text-mode applications.

## 2. Feature Priority List

### P0 — Critical (enables core functionality that's currently broken)

| # | Feature | Why it matters |
|---|---------|----------------|
| 1 | DEC Special Graphics charset (ESC ( 0) | tmux borders, MC panels, ncurses dialog boxes, htop — ALL render as garbage without this |
| 2 | TTYPE "VT220" + multi-response | Servers use TTYPE to decide what features to enable; current "VT100" limits server behavior |
| 3 | Primary DA update | Change from VT100 `\033[?1;2c` to VT220 `\033[?62;1;6c` so servers detect VT220 |
| 4 | Secondary DA (CSI > c) | vim, tmux, and many apps query this to enable advanced features; no response causes timeouts |
| 5 | DECCKM cursor key mode (CSI ? 1 h/l) | vi/vim switch cursor keys to application mode (ESC O A vs ESC [ A); without this, arrow keys break in vi |
| 6 | DECSTR soft reset (CSI ! p) | Applications reset terminal state without full RIS; avoids screen corruption between programs |

### P1 — Important (needed for good usability)

| # | Feature | Why it matters |
|---|---------|----------------|
| 7 | Function keys F1-F4 (SS3 P/Q/R/S) | Help keys, mc function bar, htop |
| 8 | Function keys F5-F12 (CSI 15~ through 24~) | mc function bar, vim mappings |
| 9 | DECAWM auto-wrap mode (CSI ? 7 h/l) | Some apps disable auto-wrap for status lines; currently always on |
| 10 | DECOM origin mode (CSI ? 6 h/l) | Scroll region cursor positioning in vim, less |
| 11 | IRM insert mode (CSI 4 h/l) | Insert-mode text editing in some line editors |
| 12 | OSC window title (ESC ] 0;...BEL) | tmux, screen, bash set window title; currently causes garbage output |
| 13 | SI/SO charset switching (0x0E/0x0F) | Programs that use G1 charset for temporary line drawing |

### P2 — Nice to have

| # | Feature | Why it matters |
|---|---------|----------------|
| 14 | Function keys F13-F20 | Rarely used; only relevant on extended keyboards |
| 15 | Additional SGR: dim (2), blink (5), hidden (8) | Monochrome: dim→normal, blink→bold, hidden→invisible |
| 16 | DECSCA protected attributes | Almost never used by modern software |
| 17 | National Replacement Character Sets | VT220 feature, superseded by UTF-8; irrelevant for modern servers |

## 3. Detailed Design

### 3.1 DEC Special Graphics Character Set

The DEC Special Graphics set maps ASCII 0x60-0x7E to box-drawing characters. Key mappings:

```
0x6A (j) → ┘   lower-right corner
0x6B (k) → ┐   upper-right corner
0x6C (l) → ┌   upper-left corner
0x6D (m) → └   lower-left corner
0x6E (n) → ┼   crossing lines
0x71 (q) → ─   horizontal line
0x74 (t) → ├   left tee
0x75 (u) → ┤   right tee
0x76 (v) → ┴   bottom tee
0x77 (w) → ┬   top tee
0x78 (x) → │   vertical line
0x61 (a) → ▒   checkerboard
0x60 (`) → ◆   diamond
0x66 (f) → °   degree symbol
0x67 (g) → ±   plus/minus
0x7E (~) → ·   centered dot
0x79 (y) → ≤   less-than-or-equal
0x7A (z) → ≥   greater-than-or-equal
```

**Rendering approach: QuickDraw primitives.** Rather than a custom font, draw each glyph using `MoveTo`/`LineTo`/`FrameRect` within the 6x12 cell. Advantages:
- No additional resources needed
- Pixel-perfect control at any cell size
- Works with bold (thicker lines via `PenSize(2,1)`)
- Respects inverse attribute (draw in white on black background)

**Implementation:**
- Add `ATTR_DEC_GRAPHICS 0x08` flag to attr byte (bits 0x08-0x80 are free)
- In `term_put_char()`, if current charset is DEC Special Graphics and `ch` is in 0x60-0x7E, set the flag on the cell
- In `terminal_ui.c draw_row()`, detect the flag and call `draw_line_char(ch, x, y)` instead of batching into DrawText
- `draw_line_char()` uses a switch on the character to draw the appropriate glyph

### 3.2 Character Set State Machine

```
Terminal struct additions:
    unsigned char g0_charset;     /* 'B' = US ASCII (default), '0' = DEC Special Graphics */
    unsigned char g1_charset;     /* 'B' = US ASCII (default), '0' = DEC Special Graphics */
    unsigned char gl_charset;     /* GL points to: 0 = G0, 1 = G1 */
```

**ESC sequence handling:**
- `ESC ( B` → G0 = US ASCII
- `ESC ( 0` → G0 = DEC Special Graphics
- `ESC ) B` → G1 = US ASCII
- `ESC ) 0` → G1 = DEC Special Graphics

**Control character handling (in PARSE_NORMAL):**
- `SI` (0x0F) → GL = G0 (Shift In, default)
- `SO` (0x0E) → GL = G1 (Shift Out)

**In term_put_char():**
```c
unsigned char active_set = (term->gl_charset == 0) ? term->g0_charset : term->g1_charset;
if (active_set == '0' && ch >= 0x60 && ch <= 0x7E) {
    term->screen[row][col].attr |= ATTR_DEC_GRAPHICS;
}
```

### 3.3 DA Response Changes

**Primary DA** (CSI c or CSI 0 c):
- Current: `\033[?1;2c` (VT100 with AVO)
- New: `\033[?62;1;6c` (VT220, 132-col, selective erase)
- The 132-col claim is conventional even when unsupported; omitting it breaks some apps

**Secondary DA** (CSI > c or CSI > 0 c):
- Currently: not implemented
- New: `\033[>1;10;0c` (VT220, firmware 1.0, no ROM cartridge)
- Implementation: detect `intermediate == '>'` in CSI `c` handler

### 3.4 TTYPE Negotiation

Change from single response to multi-response per RFC 1091:
1. First TTYPE SEND request → respond "VT220"
2. Second request → respond "VT100" (fallback)
3. Third+ request → respond "VT100" (repeat last = end of list)

```c
/* In TelnetState: */
short ttype_count;   /* number of TTYPE responses sent */

/* In handle_sb() TTYPE case: */
const char *ttype = (ts->ttype_count == 0) ? "VT220" : "VT100";
ts->ttype_count++;
```

### 3.5 DECCKM — Cursor Key Mode

CSI ? 1 h → Application mode: cursor keys send ESC O A/B/C/D
CSI ? 1 l → Normal mode: cursor keys send ESC [ A/B/C/D (current behavior)

**Terminal struct addition:**
```c
unsigned char cursor_key_mode;  /* 0 = normal (default), 1 = application */
```

**In main.c handle_key_down():**
```c
case 0x7E: /* Up */
    if (terminal.cursor_key_mode)
        conn_send(&conn, "\033OA", 3);
    else
        conn_send(&conn, "\033[A", 3);
```

This is critical because vi/vim immediately sends `CSI ? 1 h` on startup.

### 3.6 Origin Mode (DECOM)

CSI ? 6 h → Origin mode: cursor positions relative to scroll region top
CSI ? 6 l → Normal mode: cursor positions absolute (default)

**Terminal struct addition:**
```c
unsigned char origin_mode;  /* 0 = absolute (default), 1 = relative to scroll region */
```

Affects CUP (CSI H), cursor clamping, and cursor home position. When enabled:
- Row 1 in CUP means `scroll_top`, not row 0
- Cursor is clamped to scroll region

### 3.7 Auto-Wrap Mode (DECAWM)

CSI ? 7 h → Enable auto-wrap (default, current behavior)
CSI ? 7 l → Disable auto-wrap (characters overwrite at right margin)

**Terminal struct addition:**
```c
unsigned char autowrap;  /* 1 = on (default), 0 = off */
```

In `term_put_char()`, the `wrap_pending` logic only activates when `autowrap` is enabled. When disabled, writing at column 79 just overwrites column 79.

### 3.8 Insert/Replace Mode (IRM)

CSI 4 h → Insert mode: characters shift right on insert
CSI 4 l → Replace mode (default): characters overwrite

**Terminal struct addition:**
```c
unsigned char insert_mode;  /* 0 = replace (default), 1 = insert */
```

In `term_put_char()`, when insert mode is active, shift the line right from cursor before placing the character (reuse ICH logic).

### 3.9 DECSTR — Soft Reset

CSI ! p resets terminal state without clearing the screen:
- SGR → normal
- Character sets → G0=ASCII, G1=ASCII, GL=G0
- DECOM → off
- DECAWM → on
- DECCKM → off
- IRM → replace
- Cursor → visible
- Scroll region → full screen
- Saved cursor → home position

This is different from RIS (ESC c) which also clears the screen and scrollback.

### 3.10 OSC — Window Title

ESC ] Ps ; Pt BEL — Set window/icon title
ESC ] Ps ; Pt ESC \ — Same, with ST terminator

Ps values: 0 = set both, 1 = icon only, 2 = window only

**Implementation:**
- Add `PARSE_OSC` state
- Accumulate bytes until BEL (0x07) or ST (ESC \)
- On Ps=0 or Ps=2, call `SetWTitle()` to update the Flynn window title
- Silently discard other OSC sequences
- Cap accumulation at ~128 bytes to prevent buffer overflow

### 3.11 Function Key Mapping

**Extended keyboards (ADB F-keys):**
Virtual keycodes → escape sequences (added to handle_key_down):

```c
case 0x7A: conn_send(&conn, "\033OP", 3); return;    /* F1 */
case 0x78: conn_send(&conn, "\033OQ", 3); return;    /* F2 */
case 0x63: conn_send(&conn, "\033OR", 3); return;    /* F3 */
case 0x76: conn_send(&conn, "\033OS", 3); return;    /* F4 */
case 0x60: conn_send(&conn, "\033[15~", 5); return;  /* F5 */
case 0x61: conn_send(&conn, "\033[17~", 5); return;  /* F6 */
case 0x62: conn_send(&conn, "\033[18~", 5); return;  /* F7 */
case 0x64: conn_send(&conn, "\033[19~", 5); return;  /* F8 */
case 0x65: conn_send(&conn, "\033[20~", 5); return;  /* F9 */
case 0x6D: conn_send(&conn, "\033[21~", 5); return;  /* F10 */
case 0x67: conn_send(&conn, "\033[23~", 5); return;  /* F11 */
case 0x6F: conn_send(&conn, "\033[24~", 5); return;  /* F12 */
```

**M0110/M0110A keyboards (no F-keys):**
Two approaches, both should be implemented:

1. **Cmd+digit shortcuts** (intercepted before MenuKey):
   - Cmd+1 through Cmd+9 → F1-F9
   - Cmd+0 → F10
   - These don't conflict with existing menu shortcuts (Cmd+N, Cmd+W, Cmd+Q, Cmd+C, Cmd+V)

2. **"Send Key" submenu** under Session menu:
   - F1 through F12 menu items
   - Accessible via mouse for all keyboard types
   - Insert key item (ESC [ 2 ~)

### 3.12 Extended DECSC/DECRC (Save/Restore Cursor)

VT220 DECSC (ESC 7) saves more state than VT100. Extend saved fields:

```c
/* Currently saved: */
short     saved_row, saved_col;
unsigned char saved_attr;

/* Add: */
unsigned char saved_g0_charset;
unsigned char saved_g1_charset;
unsigned char saved_gl_charset;
unsigned char saved_origin_mode;
unsigned char saved_autowrap;
```

## 4. Parser State Machine Changes

Current states: PARSE_NORMAL, PARSE_ESC, PARSE_CSI, PARSE_CSI_PARAM, PARSE_CSI_INTERMEDIATE

**New states:**
- `PARSE_OSC` (6) — Accumulating OSC payload until BEL or ST
- `PARSE_OSC_ESC` (7) — Got ESC inside OSC, looking for `\` (ST)

**Changes to PARSE_NORMAL:**
- Handle SI (0x0F) and SO (0x0E) for charset switching

**Changes to PARSE_ESC (term_process_esc):**
- `ESC ]` → transition to PARSE_OSC
- `ESC ( 0` / `ESC ( B` → set G0 charset (currently consumed but ignored)
- `ESC ) 0` / `ESC ) B` → set G1 charset (currently consumed but ignored)
- `ESC P` → DCS, consume until ST (can reuse OSC state or add PARSE_DCS)

**Changes to CSI parameter parsing:**
- Handle `>` like `?` (store as intermediate for secondary DA)

**Changes to term_execute_csi:**
- `h`/`l` with `intermediate == '?'`: add DECCKM (1), DECOM (6), DECAWM (7)
- `h`/`l` with `intermediate == 0`: add IRM (4)
- `c` with `intermediate == '>'`: secondary DA response
- `p` with `intermediate == '!'`: DECSTR soft reset

## 5. File Change Summary

### terminal.h
- Add `ATTR_DEC_GRAPHICS 0x08` attribute flag
- Add `PARSE_OSC` and `PARSE_OSC_ESC` parser states
- Add Terminal struct fields: `g0_charset`, `g1_charset`, `gl_charset`, `cursor_key_mode`, `origin_mode`, `insert_mode`, `autowrap`
- Add saved state fields: `saved_g0`, `saved_g1`, `saved_gl`, `saved_origin`, `saved_autowrap`
- Add OSC buffer: `char osc_buf[128]`, `short osc_len`, `short osc_param`

### terminal.c
- `terminal_init()`: initialize new fields (charsets='B', autowrap=1, etc.)
- `terminal_process()` PARSE_NORMAL: add SI (0x0F), SO (0x0E) handling
- `term_process_esc()`: process ESC ( / ) designator bytes for charset, add ESC ] → PARSE_OSC
- New: `term_process_osc()` — accumulate and execute OSC sequences
- `term_process_csi()`: handle `>` as intermediate (like `?`)
- `term_execute_csi()`:
  - `h`/`l`: add modes 1 (DECCKM), 6 (DECOM), 7 (DECAWM); add non-private mode 4 (IRM)
  - `c`: update primary DA to VT220; add secondary DA (intermediate `>`)
  - `p`: add DECSTR (intermediate `!`)
  - `H`: account for origin mode in cursor positioning
- `term_put_char()`: charset translation, insert mode support, autowrap control
- `term_process_esc()` ESC 7/8: save/restore charset + mode state

### terminal_ui.c
- New: `draw_line_char(unsigned char ch, short x, short y, unsigned char attr)` — renders DEC Special Graphics glyphs using QuickDraw
- `draw_row()`: detect `ATTR_DEC_GRAPHICS` flag and call `draw_line_char()` for those cells instead of batching into DrawText

### telnet.c
- Change `ttype_str` to "VT220"
- Add `ttype_count` to TelnetState (in telnet.h)
- Multi-response TTYPE: first="VT220", subsequent="VT100"

### telnet.h
- Add `ttype_count` field to TelnetState struct

### main.c (handle_key_down)
- Add F1-F12 virtual keycode mappings (0x7A, 0x78, 0x63, etc.)
- Add Cmd+1 through Cmd+0 → F1-F10 for M0110
- Read `terminal.cursor_key_mode` to send correct cursor key sequences
- OSC window title → SetWTitle callback (or terminal response mechanism)

### resources/telnet.r
- Add "Send Key" submenu items (F1-F12) under Session menu (optional, P1)

### main.h
- Add menu item IDs for Send Key submenu (if implemented)

## 6. Memory Impact

| Addition | Size |
|----------|------|
| Terminal struct new fields | ~15 bytes |
| OSC buffer | 130 bytes |
| draw_line_char() code | ~800 bytes |
| Parser additions code | ~500 bytes |
| Total | ~1.5 KB |

Current memory footprint is ~25KB (0.6% of 4MB). This adds negligible overhead.

## 7. Implementation Order

Recommended order to minimize risk and maximize testability:

1. **TTYPE + DA changes** (telnet.c, terminal.c) — smallest change, immediately testable
2. **DECCKM cursor key mode** (terminal.c, main.c) — fixes vi/vim arrow keys
3. **DEC Special Graphics** (terminal.h, terminal.c, terminal_ui.c) — biggest visual impact
4. **Charset switching SI/SO** (terminal.c) — completes charset support
5. **OSC window title** (terminal.c, terminal.h) — eliminates garbage from tmux/screen
6. **DECOM, DECAWM, IRM modes** (terminal.c) — mode handling batch
7. **DECSTR soft reset** (terminal.c) — clean terminal state management
8. **Function key mappings** (main.c) — keyboard input additions
9. **Send Key menu** (main.c, resources/telnet.r) — UI addition for M0110

## 8. Testing Strategy

Each feature can be tested via telnet to the test server:

- **DEC Special Graphics**: Run `tmux` or `mc` — borders should render as lines, not `aaqqxx`
- **DECCKM**: Open `vi` — arrow keys should navigate correctly
- **TTYPE/DA**: `echo $TERM` should show `VT220`; `tput colors` should work
- **OSC**: `echo -e '\033]0;Test Title\007'` — window title should change
- **Function keys**: Press F1 in `mc` — help should open
- **Origin mode**: Use `less` with scroll regions — cursor should stay in bounds
- **DECSTR**: Run `reset` or exit a program — terminal should recover cleanly
