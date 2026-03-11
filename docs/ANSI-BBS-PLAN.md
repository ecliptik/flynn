# ANSI-BBS Emulation with CP437 — Implementation Plan

## Overview

Add ANSI-BBS terminal emulation with the CP437 (Code Page 437) character set to Flynn.
This enables connecting to BBS systems, viewing ANSI art, and running DOS-era terminal
applications over Telnet. Now that 256-color support exists (v1.9.0), the visual fidelity
gap is closed — CP437 character rendering is the remaining piece.

## What Is ANSI-BBS / CP437?

**CP437** is the original IBM PC character set. All 256 byte values (0x00-0xFF) map to
visible characters — there are no "invisible" control codes in display context except
ESC (0x1B). The upper half (0x80-0xFF) contains international letters, box-drawing
characters (single AND double line), block elements, and Greek/math symbols. The lower
range (0x01-0x1F) maps to card suits, arrows, smileys, and other symbols.

**ANSI-BBS** uses CP437 as the character set with a subset of ANSI/VT100 escape sequences:
- CSI sequences: cursor movement (CUP, CUU/CUD/CUF/CUB), erase (ED, EL), SGR colors
- 16-color palette (SGR 30-37 / 40-47, bold=bright foreground)
- No DEC private modes, no character set designation (ESC(0), no UTF-8
- Fixed 80-column width, auto-wrap at column 80
- Raw byte stream — no UTF-8 encoding, every byte 0x00-0xFF is a potential character

## Current Architecture (Relevant)

```
Network bytes → telnet_process() → terminal_process() → term_ui_draw()
                  │                      │                    │
                  │                  UTF-8 decode         draw_row()
                  │                  → Unicode CP          │
                  │                  → glyph_lookup()     glyph primitives
                  │                  → Mac Roman           or DrawChar()
                  │                  → TermCell storage
                  │
                  IAC stripping, TTYPE negotiation
```

### Key Constraints
- **TermCell = 2 bytes**: `ch` (unsigned char) + `attr` (unsigned char, 7 bits used)
- **Attr bit 7 (0x80)**: Currently unused — available
- **Glyph indices**: 0x00-0x66 primitives, 0x80-0x8E emoji. Range 0x67-0x7F free
- **68000 @ 8MHz**: Every cycle counts. No floating point. 16-bit operations preferred
- **4 MiB RAM**: ~73KB per session currently, 640KB SIZE resource limit

## The Fundamental Challenge

Flynn's pipeline assumes **UTF-8 multi-byte encoding**. CP437 is **single-byte, raw**.
These are mutually exclusive interpretations of the same byte values:

| Byte  | UTF-8 Interpretation        | CP437 Interpretation     |
|-------|-----------------------------|--------------------------|
| 0x80  | UTF-8 continuation byte     | Ç (C-cedilla)           |
| 0xC9  | UTF-8 lead byte (2-byte)    | ╔ (double box corner)   |
| 0x01  | SOH control character       | ☺ (white smiley face)   |
| 0x0D  | CR (carriage return)        | ♪ (music note)          |

The mode switch must happen at the byte interpretation layer, not later.

---

## CP437 Character Coverage Analysis

### Already Covered by Existing Glyphs (~25 characters)

| CP437 | Char | Existing Glyph          |
|-------|------|-------------------------|
| 0x03  | ♥    | GLYPH_HEART             |
| 0x04  | ♦    | GLYPH_DIAMOND_FILLED    |
| 0x05  | ♣    | GLYPH_CLUB              |
| 0x06  | ♠    | GLYPH_SPADE             |
| 0x07  | •    | GLYPH_CIRCLE_FILLED     |
| 0x09  | ○    | GLYPH_CIRCLE_EMPTY      |
| 0x0D  | ♪    | GLYPH_MUSIC_NOTE        |
| 0x0E  | ♫    | GLYPH_MUSIC_NOTES       |
| 0x10  | ►    | GLYPH_TRI_RIGHT         |
| 0x11  | ◄    | GLYPH_TRI_LEFT          |
| 0x18  | ↑    | GLYPH_ARROW_UP          |
| 0x19  | ↓    | GLYPH_ARROW_DOWN        |
| 0x1A  | →    | GLYPH_ARROW_RIGHT       |
| 0x1B  | ←    | GLYPH_ARROW_LEFT        |
| 0x1E  | ▲    | GLYPH_TRI_UP            |
| 0x1F  | ▼    | GLYPH_TRI_DOWN          |
| 0xB0  | ░    | GLYPH_SHADE_LIGHT       |
| 0xB1  | ▒    | GLYPH_SHADE_MEDIUM      |
| 0xB2  | ▓    | GLYPH_SHADE_DARK        |
| 0xB3  | │    | GLYPH_BOX_V             |
| 0xB4  | ┤    | GLYPH_BOX_VL            |
| 0xBF  | ┐    | GLYPH_BOX_DL            |
| 0xC0  | └    | GLYPH_BOX_UR            |
| 0xC1  | ┴    | GLYPH_BOX_UH            |
| 0xC2  | ┬    | GLYPH_BOX_DH            |
| 0xC3  | ├    | GLYPH_BOX_VR            |
| 0xC4  | ─    | GLYPH_BOX_H             |
| 0xC5  | ┼    | GLYPH_BOX_VH            |
| 0xD9  | ┘    | GLYPH_BOX_UL            |
| 0xDA  | ┌    | GLYPH_BOX_DR            |
| 0xDB  | █    | GLYPH_BLOCK_FULL        |
| 0xDC  | ▄    | GLYPH_BLOCK_LOWER       |
| 0xDD  | ▌    | GLYPH_BLOCK_LEFT        |
| 0xDE  | ▐    | GLYPH_BLOCK_RIGHT       |
| 0xDF  | ▀    | GLYPH_BLOCK_UPPER       |

### Map to Mac Roman (~40 characters)

International letters (0x80-0xA5) and some punctuation map directly to Mac Roman
encoding — these render natively via DrawChar() with no new glyphs needed:

| CP437 | Char | Mac Roman |
|-------|------|-----------|
| 0x80  | Ç    | 0x82      |
| 0x81  | ü    | 0x9F      |
| 0x82  | é    | 0x8E      |
| 0x83  | â    | 0x89      |
| 0x84  | ä    | 0x8A      |
| 0x85  | à    | 0x88      |
| 0x86  | å    | 0x8C      |
| 0x87  | ç    | 0x8D      |
| 0x88  | ê    | 0x90      |
| 0x89  | ë    | 0x91      |
| 0x8A  | è    | 0x8F      |
| 0x8B  | ï    | 0x95      |
| 0x8C  | î    | 0x94      |
| 0x8D  | ì    | 0x93      |
| 0x8E  | Ä    | 0x80      |
| 0x8F  | Å    | 0x81      |
| 0x90  | É    | 0x83      |
| 0x91  | æ    | 0xBE      |
| 0x92  | Æ    | 0xAE      |
| 0x93  | ô    | 0x99      |
| 0x94  | ö    | 0x9A      |
| 0x95  | ò    | 0x98      |
| 0x96  | û    | 0x9E      |
| 0x97  | ù    | 0x9D      |
| 0x98  | ÿ    | 0xD8      |
| 0x99  | Ö    | 0x85      |
| 0x9A  | Ü    | 0x86      |
| 0x9B  | ¢    | 0xA2      |
| 0x9C  | £    | 0xA3      |
| 0x9D  | ¥    | 0xB4      |
| 0x9F  | ƒ    | 0xC4      |
| 0xA0  | á    | 0x87      |
| 0xA1  | í    | 0x92      |
| 0xA2  | ó    | 0x97      |
| 0xA3  | ú    | 0x9C      |
| 0xA4  | ñ    | 0x96      |
| 0xA5  | Ñ    | 0x84      |
| 0xA6  | ª    | 0xBB      |
| 0xA7  | º    | 0xBC      |
| 0xA8  | ¿    | 0xC0      |
| 0xAA  | ¬    | 0xC2      |
| 0xAD  | ¡    | 0xC1      |
| 0xAE  | «    | 0xC7      |
| 0xAF  | »    | 0xC8      |
| 0xE1  | ß    | 0xA7      |
| 0xE3  | π    | 0xB9      |
| 0xE6  | µ    | 0xB5      |
| 0xE8  | Φ    | (none — use glyph) |
| 0xEA  | Ω    | 0xBD      |
| 0xED  | φ    | (none)    |
| 0xF1  | ±    | 0xB1      |
| 0xF3  | ≤    | 0xB2      |
| 0xF4  | ≥    | 0xB3      |
| 0xF6  | ÷    | 0xD6      |
| 0xF8  | °    | 0xA1      |
| 0xF9  | ·    | 0xE1      |
| 0xFA  | ·    | 0xE1      |
| 0xFE  | ■    | (use GLYPH_SQUARE_FILLED) |

### Need New Glyph Primitives (~35 characters)

These have no Mac Roman equivalent and no existing glyph:

**Smileys / Symbols (0x01-0x1F range):**
| CP437 | Char | Description           | Rendering |
|-------|------|-----------------------|-----------|
| 0x01  | ☺    | White smiley          | Circle + dots + arc |
| 0x02  | ☻    | Black smiley          | Filled circle + inverse |
| 0x08  | ◘    | Inverse bullet        | Filled square + white circle |
| 0x0A  | ◙    | Inverse white circle  | Filled circle + white circle |
| 0x0B  | ♂    | Male sign             | Circle + arrow |
| 0x0C  | ♀    | Female sign           | Circle + cross |
| 0x0F  | ☼    | Sun                   | Circle + radiating lines |
| 0x12  | ↕    | Up-down arrow         | Vertical line + arrowheads |
| 0x13  | ‼    | Double exclamation    | Two '!' (can use DrawChar) |
| 0x14  | ¶    | Pilcrow               | Mac Roman 0xA6 ✓ |
| 0x15  | §    | Section               | Mac Roman 0xA4 ✓ |
| 0x16  | ▬    | Black rectangle       | PaintRect (half-height) |
| 0x17  | ↨    | Up-down arrow w/base  | Vertical line + heads + base |
| 0x1C  | ∟    | Right angle           | Two lines |
| 0x1D  | ↔    | Left-right arrow      | Horizontal line + arrowheads |
| 0x7F  | ⌂    | House                 | Triangle roof + rectangle |

**Double-Line Box Drawing (0xB5-0xD8) — 24 characters:**
| CP437 | Char | Description              |
|-------|------|--------------------------|
| 0xB5  | ╡    | Single V + double H left |
| 0xB6  | ╢    | Double V + single H left |
| 0xB7  | ╖    | Double V + single H down+left |
| 0xB8  | ╕    | Single V + double H down+left |
| 0xB9  | ╣    | Double V + double H left |
| 0xBA  | ║    | Double vertical          |
| 0xBB  | ╗    | Double down+left         |
| 0xBC  | ╝    | Double up+left           |
| 0xBD  | ╜    | Double V + single H up+left |
| 0xBE  | ╛    | Single V + double H up+left |
| 0xC6  | ╞    | Single V + double H right |
| 0xC7  | ╟    | Double V + single H right |
| 0xC8  | ╚    | Double up+right          |
| 0xC9  | ╔    | Double down+right        |
| 0xCA  | ╩    | Double up+horiz          |
| 0xCB  | ╦    | Double down+horiz        |
| 0xCC  | ╠    | Double V + double H right |
| 0xCD  | ═    | Double horizontal        |
| 0xCE  | ╬    | Double cross             |
| 0xCF  | ╧    | Double H + single V up   |
| 0xD0  | ╨    | Single H + double V up   |
| 0xD1  | ╤    | Double H + single V down |
| 0xD2  | ╥    | Single H + double V down |
| 0xD3  | ╙    | Double V + single H up+right |
| 0xD4  | ╘    | Single V + double H up+right |
| 0xD5  | ╒    | Single V + double H down+right |
| 0xD6  | ╓    | Double V + single H down+right |
| 0xD7  | ╫    | Double V + single H cross |
| 0xD8  | ╪    | Single V + double H cross |

**Greek/Math (0xE0-0xFE range):**
| CP437 | Char | Description    | Rendering |
|-------|------|----------------|-----------|
| 0xE0  | α    | Alpha          | DrawChar 'a' italic or glyph |
| 0xE2  | Γ    | Gamma          | QuickDraw L-shape |
| 0xE4  | Σ    | Sigma (Mac Roman 0xB7) | ✓ Mac Roman |
| 0xE5  | σ    | Lowercase sigma | Glyph or fallback 'o' |
| 0xE7  | τ    | Tau            | DrawChar or glyph |
| 0xE9  | Θ    | Theta          | Circle + horiz line |
| 0xEB  | δ    | Delta          | Glyph |
| 0xEC  | ∞    | Infinity       | Figure-eight glyph |
| 0xEE  | ε    | Epsilon        | Glyph or fallback 'e' |
| 0xEF  | ∩    | Intersection   | Arc/U-shape |
| 0xF0  | ≡    | Identical      | Three horiz lines |
| 0xF2  | ≥    | Greater-equal  | (Mac Roman 0xB3) ✓ |
| 0xF5  | ⌠    | Top integral   | Bracket glyph |
| 0xF6  | ⌡    | Bottom integral| Bracket glyph |
| 0xF7  | ≈    | Approx equal   | Two tildes |
| 0xFB  | √    | Square root    | Radical glyph |
| 0xFC  | ⁿ    | Superscript n  | Small 'n' raised |
| 0xFD  | ²    | Superscript 2  | HAVE: GLYPH_SUPER_2 ✓ |
| 0xFF  | (nb) | No-break space | Space (no glyph needed) |

**Fractions / Misc:**
| CP437 | Char | Description |
|-------|------|-------------|
| 0x9E  | ₧    | Peseta sign — fallback to 'P' |
| 0xA9  | ⌐    | Reversed not — L-shape glyph |
| 0xAB  | ½    | One-half — glyph or '1/2' |
| 0xAC  | ¼    | One-quarter — glyph or '1/4' |

### Summary

| Category                     | Count | Method              |
|------------------------------|-------|---------------------|
| ASCII (0x20-0x7E)            | 95    | No change           |
| Existing glyph primitives    | ~35   | Reuse via CP437 table |
| Mac Roman direct mapping     | ~45   | DrawChar (native font) |
| New glyph primitives needed  | ~35   | QuickDraw rendering |
| New double-line box drawing  | ~29   | QuickDraw LineTo    |
| Fallback approximations      | ~10   | Nearest ASCII char  |
| Null/space (0x00, 0xFF)      | 2     | Space               |
| **Total**                    | **256** |                   |

---

## Implementation Plan

### Phase 1: CP437 Lookup Table (cp437.c/h)

Create a 256-entry lookup table mapping each CP437 byte value to its rendering method:

```c
/* cp437.h */

/* CP437 cell rendering method */
#define CP437_ASCII     0   /* render as ch directly (0x20-0x7E) */
#define CP437_MACROMAN  1   /* render as Mac Roman byte in .mr field */
#define CP437_GLYPH     2   /* render as glyph index in .glyph field */
#define CP437_SPACE     3   /* render as space (0x00, 0xFF) */

typedef struct {
    unsigned char method;   /* CP437_ASCII/MACROMAN/GLYPH/SPACE */
    unsigned char value;    /* Mac Roman byte, glyph ID, or ASCII char */
} CP437Entry;

extern const CP437Entry cp437_table[256];

/* Convert CP437 byte to TermCell ch + attr */
void cp437_to_cell(unsigned char cp437_byte,
                   unsigned char *out_ch,
                   unsigned char *out_attr_flags);
```

**Memory cost**: 256 × 2 = **512 bytes** (const ROM data, not heap)

**Files**: `src/cp437.c`, `src/cp437.h`

### Phase 2: New Glyph Primitives for CP437

Add ~35 new glyph primitives, primarily:

1. **Double-line box drawing** (~29 glyphs): Use the same QuickDraw LineTo approach
   as existing single-line box drawing, but draw parallel lines 2px apart. This is
   the single largest block and the most important for BBS art fidelity.

2. **Symbol glyphs** (~6 glyphs): Smileys, gender signs, sun, house, arrows.
   Simple QuickDraw shapes.

Current glyph indices used: 0x00-0x66 (primitives), 0x80-0x8E (emoji).
Free primitive slots: 0x67-0x7F = **25 slots**.

**Problem**: 25 free primitive slots, but ~35 new primitives needed.

**Solutions** (pick one):
- **(A) Expand primitive range**: Use 0x67-0x9F (extending into emoji base area).
  Requires moving `GLYPH_EMOJI_BASE` to 0xA0. Emoji indices 0x80-0x8E become 0xA0-0xAE.
  Gives 64 free primitive slots (0x67-0x9F). **Breaking change** to glyph storage.
- **(B) Add CP437 glyph category**: New `GLYPH_CAT_CP437` with its own 0x00-0xFF
  index space, rendered by a new `draw_cp437_prim()` function. Avoids touching
  existing glyph indices. Uses `ATTR_CP437` flag (bit 7, 0x80) to distinguish.
- **(C) Hybrid**: Use remaining 25 primitive slots for the most common CP437 glyphs
  (double box drawing corners/lines), approximate the rest with existing glyphs or
  Mac Roman fallbacks.

**Recommended: Option B** — cleanest separation, zero impact on existing rendering.

**Memory cost**: ~29 new QuickDraw primitive renderers ≈ **800 bytes code** (each
double-line box glyph is ~25 bytes of compiled 68k code — MoveTo + LineTo × 4).
Symbol glyphs add ~200 bytes. Total: **~1KB code**.

**Files**: `src/glyphs.c` (add `draw_cp437_prim()`), `src/glyphs.h` (new defines)

### Phase 3: Terminal Mode Switch

Add ANSI-BBS mode to the Terminal struct and parser:

```c
/* terminal.h additions */
#define ATTR_CP437  0x80    /* bit 7: cell uses CP437 glyph rendering */

/* In Terminal struct: */
unsigned char cp437_mode;   /* 0 = off (UTF-8/VT220), 1 = CP437/ANSI-BBS */
```

**Parser changes in `terminal_process()`:**

When `cp437_mode == 1`:
1. **Bypass UTF-8 decoder entirely** — every byte is a single character
2. **Control character handling changes**:
   - 0x1B (ESC): Still enters escape sequence parser (CSI, SGR, etc.)
   - 0x0D (CR): Still carriage return
   - 0x0A (LF): Still line feed
   - 0x08 (BS): Still backspace
   - 0x07 (BEL): Still bell
   - **All other 0x00-0x1F, 0x7F**: Display as CP437 graphic characters (NOT controls)
   - This means 0x01-0x06, 0x0B-0x0C, 0x0E-0x1F display as symbols
3. **Byte storage**: Look up `cp437_table[byte]` → store `ch` + `attr` with
   appropriate flags (ATTR_GLYPH, ATTR_CP437, or plain char)
4. **No SCS/G0/G1 charset switching** — CP437 is the only charset

**Parser state machine**: No changes needed — CSI/OSC/DCS parsing is identical.
The only difference is how PARSE_NORMAL handles non-escape bytes.

**Memory cost**: 1 byte in Terminal struct + ~200 bytes of branching code in
`terminal_process()`.

**Files**: `src/terminal.h`, `src/terminal.c`

### Phase 4: TTYPE and Settings Integration

1. **Add "ansi" terminal type** to TTYPE negotiation:
   ```c
   /* telnet.c */
   ttype_cycle[] = { "xterm-256color", "xterm", "VT220", "VT100", "ansi" }
   ```
   When TTYPE is "ansi", automatically enable `cp437_mode = 1`.

2. **Settings/Preferences**:
   - Add terminal_type option 4 = "ansi" to settings menu
   - Per-bookmark terminal type already supports custom values
   - FlynnPrefs version bump (v8 → v9) for the new option

3. **Connect dialog**: "ansi" appears in terminal type selector

**Memory cost**: ~20 bytes (string + menu item)

**Files**: `src/telnet.c`, `src/settings.c`, `src/settings.h`, `src/main.c`

### Phase 5: Renderer Integration

Add CP437 glyph rendering path to `draw_row()` in `terminal_ui.c`:

```c
/* In draw_row(), after checking ATTR_GLYPH and ATTR_BRAILLE: */
if (cell->attr & ATTR_CP437) {
    draw_cp437_prim(cell->ch, x, y, cell_w, cell_h, is_bold);
    continue;
}
```

The `draw_cp437_prim()` function handles the ~35 new CP437-specific symbols.
For cells stored as Mac Roman or ASCII, existing DrawChar() path works unchanged.

**Color rendering**: Works automatically — ATTR_HAS_COLOR + CellColor arrays are
orthogonal to the character rendering path. BBS 16-color SGR maps to existing
palette indices 0-15.

**Double-line box drawing rendering detail**:
```
Single line:     Double line:
    │                ║
    │              │   │    (2px gap between parallel lines)
    │              │   │
```
Each double-line glyph is 4-6 LineTo calls vs 2-3 for single-line. The gap between
parallel lines should be 2 pixels (1px line, 2px gap, 1px line) for visual clarity
at Monaco 9 cell dimensions (6×10 or 7×11 pixels).

**Files**: `src/terminal_ui.c`, `src/glyphs.c`

### Phase 6: Testing and Polish

1. Test with known BBS systems over Telnet
2. Verify ANSI art renders correctly (block characters, box drawing, colors)
3. Verify VT100/xterm mode is completely unaffected (regression testing)
4. Test mode switching (connect to BBS with "ansi", then to Unix with "xterm")
5. Verify clipboard copy produces reasonable ASCII approximations

---

## Memory Impact Analysis

### Static (Code + Const Data)

| Component                    | Size        | Notes |
|------------------------------|-------------|-------|
| CP437 lookup table           | 512 bytes   | const, in code segment |
| Double-line box draw code    | ~800 bytes  | 29 glyph renderers |
| Symbol glyph code            | ~200 bytes  | 6 symbol renderers |
| Parser branching (cp437_mode)| ~200 bytes  | Conditional in terminal_process |
| TTYPE string + settings      | ~50 bytes   | "ansi" + menu |
| **Total static increase**    | **~1.8 KB** | |

### Per-Session (Dynamic)

| Component                | Size     | Notes |
|--------------------------|----------|-------|
| `cp437_mode` flag        | 1 byte   | In Terminal struct |
| **Total per-session**    | **1 byte** | |

**No additional dynamic memory allocation.** CP437 mode reuses:
- Existing TermCell screen buffers (ch + attr, same 2-byte cells)
- Existing CellColor arrays (if color enabled on System 7)
- Existing scrollback ring buffer
- No new arrays, no new heap allocations

### Comparison

| Configuration            | Memory/Session | Change |
|--------------------------|----------------|--------|
| Current (VT220/xterm)    | ~73 KB         | —      |
| With CP437 mode (Sys 6)  | ~73 KB         | +1 byte |
| With CP437 mode (Sys 7)  | ~119 KB        | +1 byte |

---

## CPU / Performance Impact Analysis

### Fast Path (VT100/xterm mode — no CP437)

**Zero overhead.** The CP437 check is a single branch at the top of
`terminal_process()`:

```c
if (term->cp437_mode) {
    /* CP437 path */
} else {
    /* existing UTF-8/VT220 path — completely unchanged */
}
```

On 68000, this is **2 instructions** (tst.b + beq) = **~12 cycles** per byte
processed. With incoming data at ~19.2 Kbaud max over Telnet, this adds
< 0.05% overhead to the existing path.

### CP437 Path Performance

| Operation                | Cycles (est.) | Notes |
|--------------------------|---------------|-------|
| CP437 table lookup       | ~20           | Indexed load, 2 bytes |
| Skip UTF-8 decoder       | -80 to -200  | **Savings** (no multi-byte state) |
| Cell storage             | ~30           | Same as VT220 |
| **Net per-byte**         | **~50 faster** | CP437 is simpler than UTF-8 |

**CP437 mode is actually faster than VT220 mode** because:
1. No UTF-8 multi-byte sequence accumulation
2. No `glyph_lookup()` binary search (direct table index instead)
3. No Unicode → Mac Roman translation fallback chain
4. Single-byte = single-cell, always (no wide character handling)

### Rendering Performance

| Glyph Type               | Cycles/Cell | vs Normal Text |
|---------------------------|-------------|----------------|
| ASCII (DrawChar)          | ~200        | Same           |
| Mac Roman (DrawChar)      | ~200        | Same           |
| Single-line box (LineTo)  | ~400        | 2× slower      |
| Double-line box (LineTo)  | ~700        | 3.5× slower    |
| Symbol primitives         | ~500        | 2.5× slower    |

**Worst case**: A full screen of double-line box drawing (80×24 = 1,920 cells)
at ~700 cycles each = 1.34M cycles = **~167ms** at 8MHz. This is within the
17ms×10 = 170ms budget for a full-screen redraw at 1 tick WaitNextEvent.

**Typical BBS screen**: Mixed text + box drawing + blocks. Estimate ~60% ASCII,
~20% box drawing, ~10% blocks, ~10% other. Average ~300 cycles/cell = **57ms**
full-screen redraw. Acceptable.

**Incremental rendering**: BBS content typically arrives line-by-line. With
dirty-row tracking, only changed rows are redrawn. Typical: 1-3 rows at ~3-5ms
each = **3-15ms per update**. Excellent.

---

## Tradeoffs

### 1. Glyph Index Space Pressure

**Current state**: 103 primitive glyphs used (0x00-0x66), 15 emoji (0x80-0x8E).
Adding ~35 CP437 primitives pushes total to ~138 primitives.

**Option A (expand primitives)**: Gives 64 new slots but requires renumbering emoji.
Any saved terminal state with emoji glyph IDs would be invalidated. Risk: clipboard
copy of emoji cells would produce wrong characters until session resets.

**Option B (ATTR_CP437 bit)**: Uses bit 7 of attr byte (currently free). CP437 glyphs
get their own 0x00-0xFF index space. Zero impact on existing glyph system. Downside:
consumes the last available attr bit — no more attribute flags can be added.

**Recommendation**: Option B. The attr byte is full after this, but there are no
planned features that need another attr bit. If one is ever needed, ATTR_BRAILLE
(0x20) could be merged into ATTR_GLYPH with a reserved glyph range.

### 2. Control Character Semantics

In CP437 mode, bytes 0x01-0x06, 0x0E-0x1A, 0x1C-0x1F are display characters,
not control codes. This means:

- **SO/SI (0x0E/0x0F)** don't switch charsets — they display ♫/☼
- **Tab (0x09)** displays ○ instead of advancing to next tab stop
- **Form Feed (0x0C)** displays ♀ instead of clearing screen

Most BBS servers don't send these as controls in ANSI mode. But some BBS software
uses Tab (0x09) for alignment. Risk: misrendered menus on some BBSes.

**Mitigation**: Could make specific bytes configurable (honor Tab in CP437 mode).
Or: keep 0x07-0x0D as controls even in CP437 mode (BEL, BS, Tab, LF, VT, FF, CR)
since these are the most commonly used. This is what most ANSI-BBS terminals do.

**Recommendation**: Hybrid approach — honor BEL(07), BS(08), Tab(09), LF(0A),
CR(0D) as controls even in CP437 mode. Display all others as graphics. This
matches real-world BBS terminal behavior (SyncTERM, NetRunner).

### 3. Bit 7 Consumption (Last Attr Bit)

Using ATTR_CP437 = 0x80 means all 8 bits of the attr byte are consumed:

| Bit | Flag            |
|-----|-----------------|
| 0   | ATTR_BOLD       |
| 1   | ATTR_UNDERLINE  |
| 2   | ATTR_INVERSE    |
| 3   | ATTR_DEC_GRAPHICS |
| 4   | ATTR_GLYPH      |
| 5   | ATTR_BRAILLE    |
| 6   | ATTR_HAS_COLOR  |
| 7   | ATTR_CP437 (new)|

**Future flexibility**: If a new attribute is ever needed, options include:
- Merge ATTR_BRAILLE into ATTR_GLYPH (braille as glyph range 0xC0-0xFF)
- Merge ATTR_DEC_GRAPHICS into ATTR_GLYPH (DEC graphics as glyph range)
- Expand TermCell to 4 bytes (major change, doubles buffer memory)

### 4. Mode Isolation

CP437 mode and UTF-8/VT220 mode are mutually exclusive per-session. A session
is either CP437 or UTF-8, determined at connect time by TTYPE. There is no
in-band switching between modes.

**Risk**: If a BBS sends UTF-8 content while TTYPE is "ansi", it will be
misrendered (each UTF-8 byte treated as a separate CP437 character). This is
correct behavior — the BBS should respect the terminal type it negotiated.

**Risk**: If a user manually selects "ansi" terminal type but connects to a
Unix server, all output will be garbled. Mitigation: clear documentation,
and possibly a menu item to switch modes mid-session (would require terminal
reset).

### 5. Double-Line Box Drawing Fidelity at Small Font Sizes

At Monaco 9 (cell size ~6×10 pixels), double-line box drawing characters have
very limited pixel space. Two parallel lines with a gap require at minimum 3
pixels horizontally (1+1+1). In a 6-pixel-wide cell:
- Margins: 1px each side = 4 pixels available
- Double line: 1px + 1px gap + 1px = 3 pixels
- Remaining: 1px for centering

This is tight but workable. At Monaco 12 or larger fonts, it looks much better.

**Risk**: At very small font sizes (Geneva 9, which has ~5px cells), double lines
may merge into single thick lines. Acceptable degradation — still recognizable.

### 6. No SAUCE Metadata Support

SAUCE (Standard Architecture for Universal Comment Extensions) is a metadata
block appended to ANSI art files. It contains title, author, dimensions, font
preference, etc. Flynn won't parse SAUCE because:
- SAUCE is a file format, not a protocol feature
- Telnet BBS servers strip SAUCE before sending to clients
- Adding SAUCE parsing adds complexity with minimal benefit

If needed later, SAUCE could be a Phase 7 addition (~200 lines of code).

---

## Risks

### Low Risk

1. **Existing functionality regression**: CP437 code paths are entirely
   conditional on `cp437_mode` flag. VT220/xterm paths are untouched. Risk
   of regression is minimal with proper testing.

2. **Memory pressure**: +1.8KB code, +1 byte per session. Negligible on a
   system with 640KB SIZE resource.

3. **Build complexity**: Two new source files (cp437.c/h), one modified
   CMakeLists.txt. Straightforward.

### Medium Risk

4. **Double-line box drawing visual quality**: At small font sizes, parallel
   lines may not render cleanly. Mitigation: test at Monaco 9 (primary font),
   adjust pixel offsets if needed. Worst case: double lines look like bold
   single lines (degraded but functional).

5. **BBS compatibility**: Different BBS software has subtly different ANSI
   implementations. Some use ANSI music (ESC[M...), some use custom escape
   sequences. Flynn would support the common subset. Uncommon sequences would
   be silently ignored (existing parser behavior for unknown CSI).

6. **TTYPE negotiation**: Some BBS software may not recognize "ansi" as a
   terminal type and fall back to raw mode. Mitigation: the TTYPE cycle
   allows the server to request alternatives.

### High Risk

7. **Attr byte exhaustion**: Using the last bit (0x80) closes off future
   attribute expansion without restructuring TermCell. This is the most
   significant long-term architectural risk. However, no planned features
   require additional attr bits, and TermCell can be expanded to 4 bytes
   if ever needed (at the cost of doubling buffer memory).

---

## Effort Estimate

| Phase | Description              | New/Modified Files | Complexity |
|-------|--------------------------|-------------------|------------|
| 1     | CP437 lookup table       | +cp437.c/h        | Low        |
| 2     | New glyph primitives     | glyphs.c/h        | Medium     |
| 3     | Terminal mode switch     | terminal.c/h      | Medium     |
| 4     | TTYPE + settings         | telnet.c, settings.c/h, main.c | Low |
| 5     | Renderer integration     | terminal_ui.c, glyphs.c | Medium |
| 6     | Testing + polish         | tests/             | Medium     |

**Total new code**: ~600-800 lines (cp437.c: ~300, new glyphs: ~200, parser/renderer: ~100-300)

**Files modified**: 8 (terminal.h, terminal.c, terminal_ui.c, glyphs.c, glyphs.h, telnet.c, settings.c/h, CMakeLists.txt)

**Files added**: 2 (cp437.c, cp437.h)

---

## Alternative Approaches Considered

### A. Bitmap Font for CP437
Instead of QuickDraw primitives, embed a complete 256-character 8×16 bitmap font
(the original IBM VGA font). Each character = 16 bytes, total = 4KB.

**Pros**: Perfect visual fidelity to original IBM PC rendering.
**Cons**: Fixed size (doesn't scale with Mac font size selection), 4KB static
memory, doesn't integrate with Mac font system, licensing concerns with IBM
VGA font.

**Verdict**: Rejected. QuickDraw primitives scale with font size and match the
Mac aesthetic. The slight visual difference from original CP437 is acceptable.

### B. UTF-8 Translation Layer
Instead of raw CP437 mode, translate incoming CP437 bytes to UTF-8 Unicode
codepoints and feed them through the existing Unicode pipeline.

**Pros**: No new attr bit needed, leverages existing glyph system.
**Cons**: Still needs double-line box drawing glyphs (same amount of new glyph
work), adds translation overhead, doesn't solve control character semantics
(0x01-0x1F still need special handling), more complex than direct CP437 table.

**Verdict**: Rejected. The translation layer adds complexity without reducing
the core glyph work. Direct CP437 lookup is simpler and faster.

### C. Do Nothing (Status Quo)
BBS usage is niche. Most modern BBS systems support UTF-8 and standard ANSI
escape codes, which Flynn already handles.

**Pros**: Zero development effort, zero risk.
**Cons**: Cannot display classic ANSI art, cannot connect to legacy DOS-era BBS
systems, misses the retro aesthetic that aligns with Flynn's classic Mac identity.

**Verdict**: Valid option if BBS support isn't a priority. But the implementation
cost is modest (~800 lines), the risk is low, and it adds genuine capability.
