# True SGR Attributes — Implementation Plan

## Overview

Replace the current SGR attribute mappings with true rendering for italic,
strikethrough, dim, and blink. Currently:

| SGR Code | Name          | Current Behavior        | Proposed         |
|----------|---------------|-------------------------|------------------|
| SGR 1    | Bold          | True bold (ATTR_BOLD)   | Unchanged        |
| SGR 2    | Dim/Faint     | Clears bold             | True dim         |
| SGR 3    | Italic        | Maps to underline       | True italic      |
| SGR 4    | Underline     | True underline          | Unchanged        |
| SGR 5/6  | Blink         | Maps to bold            | True blink       |
| SGR 7    | Inverse       | True inverse            | Unchanged        |
| SGR 9    | Strikethrough | Silently ignored        | True strikethrough |

---

## The Attr Byte Problem

The `TermCell.attr` byte has 8 bits. Currently 7 are used:

```
Bit 0: ATTR_BOLD           (0x01)  ── visual style
Bit 1: ATTR_UNDERLINE      (0x02)  ── visual style
Bit 2: ATTR_INVERSE        (0x04)  ── visual style
Bit 3: ATTR_DEC_GRAPHICS   (0x08)  ── cell type  ┐
Bit 4: ATTR_GLYPH          (0x10)  ── cell type  ├─ mutually exclusive
Bit 5: ATTR_BRAILLE        (0x20)  ── cell type  ┘
Bit 6: ATTR_HAS_COLOR      (0x40)  ── color flag
Bit 7: (free)              (0x80)  ── 1 free bit
```

We need 4 new bits (italic, strikethrough, dim, blink) but have only 1 free.

### Key Insight: Cell Type Bits Are Mutually Exclusive

A cell is exactly ONE of: normal text, DEC graphics, glyph, or braille. These
three flags (bits 3-5) can never be set simultaneously, yet they occupy 3 bits.
Encoding them as a 2-bit field frees 1 bit:

```
00 = normal text
01 = DEC Special Graphics
10 = glyph (primitive or emoji)
11 = braille dot pattern
```

### Proposed Attr Byte Layout

```
Bit 0: ATTR_BOLD           (0x01)  ── unchanged
Bit 1: ATTR_UNDERLINE      (0x02)  ── unchanged
Bit 2: ATTR_INVERSE        (0x04)  ── unchanged
Bit 3: ATTR_CELL_TYPE_LO   (0x08)  ── cell type low bit  ┐ 2-bit field
Bit 4: ATTR_CELL_TYPE_HI   (0x10)  ── cell type high bit ┘
Bit 5: ATTR_ITALIC         (0x20)  ── NEW: true italic
Bit 6: ATTR_HAS_COLOR      (0x40)  ── unchanged
Bit 7: ATTR_STRIKETHROUGH  (0x80)  ── NEW: true strikethrough
```

This gives us 2 new attribute bits. That covers italic and strikethrough —
the two most visually impactful additions.

**Where do dim and blink go?** See the analysis below — they each have
practical rendering challenges that make dedicated bits less valuable than
alternative approaches.

---

## Cell Type Migration

### Current (3 separate flag bits)

```c
#define ATTR_DEC_GRAPHICS   0x08
#define ATTR_GLYPH          0x10
#define ATTR_BRAILLE        0x20

/* Usage in terminal_ui.c draw_row(): */
if (run_attr & ATTR_DEC_GRAPHICS) { ... }
if (run_attr & ATTR_BRAILLE) { ... }
if (run_attr & ATTR_GLYPH) { ... }
```

### Proposed (2-bit encoded field)

```c
/* Cell type encoding (bits 3-4) */
#define CELL_TYPE_MASK      0x18    /* bits 3-4 */
#define CELL_TYPE_SHIFT     3
#define CELL_TYPE_NORMAL    0x00    /* 00: regular character */
#define CELL_TYPE_DEC       0x08    /* 01: DEC Special Graphics */
#define CELL_TYPE_GLYPH     0x10    /* 10: glyph index */
#define CELL_TYPE_BRAILLE   0x18    /* 11: braille dot pattern */

/* Convenience macros */
#define CELL_IS_NORMAL(a)   (((a) & CELL_TYPE_MASK) == CELL_TYPE_NORMAL)
#define CELL_IS_DEC(a)      (((a) & CELL_TYPE_MASK) == CELL_TYPE_DEC)
#define CELL_IS_GLYPH(a)    (((a) & CELL_TYPE_MASK) == CELL_TYPE_GLYPH)
#define CELL_IS_BRAILLE(a)  (((a) & CELL_TYPE_MASK) == CELL_TYPE_BRAILLE)
```

**Critical compatibility detail**: `CELL_TYPE_DEC` (0x08) equals the old
`ATTR_DEC_GRAPHICS` (0x08), and `CELL_TYPE_GLYPH` (0x10) equals the old
`ATTR_GLYPH` (0x10). This means existing cells stored with these flags
decode correctly under the new scheme without any data migration. Only
`ATTR_BRAILLE` changes from 0x20 to 0x18 — but braille cells only exist
in live terminal buffers (never persisted to disk), so this is safe.

### Files Requiring Cell Type Changes

| File              | Occurrences | Change |
|-------------------|-------------|--------|
| terminal.h        | 3 defines   | Replace with new defines + macros |
| terminal.c        | 5 uses      | `ATTR_DEC_GRAPHICS` → `CELL_TYPE_DEC`, etc. |
| terminal_ui.c     | 3 checks    | `& ATTR_X` → `CELL_IS_X()` macro |
| clipboard.c       | 3 checks    | `& ATTR_X` → `CELL_IS_X()` macro |
| savefile.c        | 3 checks    | `& ATTR_X` → `CELL_IS_X()` macro |
| glyphs.h          | 1 comment   | Update comment |
| **Total**         | **18**      | Mechanical replacement |

---

## Phase 1: Cell Type Merge + Attr Restructure

Refactor the attr byte layout. This is a pure refactor with no behavior change.

1. Replace `ATTR_DEC_GRAPHICS`/`ATTR_GLYPH`/`ATTR_BRAILLE` defines with
   `CELL_TYPE_*` defines and `CELL_IS_*()` macros
2. Update all 18 usage sites across 6 files
3. Add `ATTR_ITALIC` (0x20) and `ATTR_STRIKETHROUGH` (0x80) defines
   (unused until Phase 2)
4. Build and verify — behavior must be identical

**Risk**: Low. Mechanical refactor. The DEC and GLYPH bit positions are
unchanged (0x08 and 0x10), so only BRAILLE (0x20→0x18) changes encoding.
Braille cells are rare and only exist in live buffers.

---

## Phase 2: True Italic (SGR 3)

### Rendering

QuickDraw's `TextFace(italic)` applies a shear transform to characters.
This is straightforward to add — `italic` is a standard `TextFace` constant
alongside `bold` and `underline`.

```c
/* In draw_row(), text face calculation: */
short face = 0;
if (run_attr & ATTR_BOLD)
    face |= bold;
if (run_attr & ATTR_UNDERLINE)
    face |= underline;
if (run_attr & ATTR_ITALIC)       /* NEW */
    face |= italic;               /* NEW */
if (face != last_face) {
    TextFace(face);
    last_face = face;
}
```

QuickDraw handles bold+italic, underline+italic, and all combinations
natively. The `face` parameter is a bitmask — they compose correctly.

### Parser Changes

```c
/* terminal.c, in SGR handler: */
case 3:
    /* Italic — true italic rendering */
    term->cur_attr |= ATTR_ITALIC;
    break;
case 23:
    /* Not italic */
    term->cur_attr &= ~ATTR_ITALIC;
    break;
```

Remove the old mapping of SGR 3 → ATTR_UNDERLINE.

### Italic Cell Width Concern

QuickDraw italic applies a shear (slant) to each character. On fixed-width
fonts like Monaco, the slanted character may extend ~1-2 pixels past its
cell boundary into the adjacent cell.

**Impact**: Minimal. The per-character `MoveTo(x, baseline); DrawChar(ch);`
rendering path already positions each character absolutely, so the next
character overwrites any italic overhang. The rightmost column may clip at
the window edge, which is acceptable.

**On glyph/DEC graphics/braille cells**: Italic flag is silently ignored for
these cell types (they're drawn with LineTo/CopyBits, not TextFace). This
is correct — you wouldn't italicize a box-drawing character.

### Files Modified
- `terminal.h`: Add `ATTR_ITALIC` define
- `terminal.c`: SGR 3/23 handler (~4 lines changed)
- `terminal_ui.c`: TextFace calculation (~2 lines added)

---

## Phase 3: True Strikethrough (SGR 9)

### Rendering

Draw a horizontal line through the vertical center of each cell in the run.
This is done AFTER drawing the text, as a post-pass:

```c
/* In draw_row(), after text rendering for a run: */
if (run_attr & ATTR_STRIKETHROUGH) {
    short strike_y = row_y + g_cell_height / 2;
    short x0 = LEFT_MARGIN + run_start * cell_w;
    short x1 = x0 + run_len * cell_w;
    MoveTo(x0, strike_y);
    LineTo(x1, strike_y);
}
```

That's 2 QuickDraw calls per run (not per cell). If a 40-character run is
all strikethrough, it's 1 MoveTo + 1 LineTo for the entire run.

### Interaction with Other Attributes

| Combination              | Rendering                              |
|--------------------------|----------------------------------------|
| Strikethrough + Bold     | Bold text + line at PenSize(1,1)       |
| Strikethrough + Italic   | Italic text + horizontal line          |
| Strikethrough + Inverse  | Inverse text + line in foreground color |
| Strikethrough + Color    | Line uses same foreground color as text |

On monochrome inverse: the line should use `srcBic` mode to draw white-on-black,
matching the text. On color: line uses current foreground RGBColor.

### Parser Changes

```c
/* terminal.c, in SGR handler: */
case 9:
    /* Strikethrough */
    term->cur_attr |= ATTR_STRIKETHROUGH;
    break;
case 29:
    /* Not strikethrough */
    term->cur_attr &= ~ATTR_STRIKETHROUGH;
    break;
```

### Files Modified
- `terminal.h`: Add `ATTR_STRIKETHROUGH` define
- `terminal.c`: SGR 9/29 handler (~4 lines changed)
- `terminal_ui.c`: Strikethrough rendering (~10 lines added)

---

## Phase 4: Dim (SGR 2) — Via Color, Not Attr Bit

### Why Not a Dedicated Bit

Dim (faint) text is defined as "reduced intensity." On a **monochrome**
display (the Mac Plus), there are only two states: black and white. There is
no way to render "50% black" with individual pixels — QuickDraw text rendering
is either fully on or fully off. Options:

1. **Gray pattern dithering**: Render text, then apply a checkerboard mask to
   erase every other pixel. Result: barely readable at 9pt, ugly, and
   computationally expensive (requires a second pass with PenPat).
2. **Map to "not bold"**: Current behavior. On terminals with only two
   intensities (normal and bold), dim = normal. This is what the VT220 did.

On a **color** system (System 7), dim is meaningful — halve the RGB values of
the foreground color. But this can be done without an attr bit:

### Recommended Approach: Dim via Color Channel

When SGR 2 is received on a color system:
1. If `cur_fg` is a specific color (not default): compute a dimmed version
   by halving the RGB components, find nearest palette entry via
   `color_nearest_256()`, and set `cur_fg` to that dimmed color.
2. If `cur_fg` is default: set `cur_fg` to palette index for medium gray
   (e.g., index 248 = RGB(168,168,168) on dark bg, or index 240 =
   RGB(88,88,88) on light bg).

This approach:
- Requires **zero attr bits** — the dim effect is baked into the color
- Works automatically with the existing color rendering pipeline
- Gives visually correct results on color systems
- Costs nothing on monochrome (continue current "clear bold" behavior)

### Dim + Bold Interaction

SGR 2 followed by SGR 1 should cancel dim (per ECMA-48). SGR 22 cancels both.
Current behavior (SGR 2 clears bold, SGR 22 clears bold) is already correct
for monochrome. On color, the dim color effect would be overridden by a
subsequent SGR 1 setting bright colors.

### Parser Changes

```c
case 2:
    /* Dim/faint */
    term->cur_attr &= ~ATTR_BOLD;       /* monochrome: clear bold */
    if (g_has_color_qd) {               /* color: dim foreground */
        unsigned char dimmed;
        if (term->cur_fg == COLOR_DEFAULT)
            dimmed = g_dark_mode ? 248 : 240;  /* gray */
        else
            dimmed = color_dim(term->cur_fg);   /* halve RGB */
        term->cur_fg = dimmed;
        term->cur_attr |= ATTR_HAS_COLOR;
    }
    break;
```

New function in `color.c`:

```c
/* Return a dimmed version of a palette color (halve RGB components) */
unsigned char color_dim(unsigned char idx);
```

### Files Modified
- `terminal.c`: SGR 2 handler (~8 lines changed)
- `color.c/h`: Add `color_dim()` (~15 lines)

---

## Phase 5: Blink (SGR 5) — Timer-Driven Visibility Toggle

### Why Not a Simple Attr Bit

Blink requires a **time-based rendering state** — cells alternate between
visible and invisible on a ~500ms cycle. A bit in the attr byte tells the
renderer WHICH cells blink, but the actual toggling needs infrastructure:

1. A timer in the event loop (every ~500ms, toggle blink state)
2. Track which rows have blinking cells (avoid full-screen redraw)
3. Redraw only affected cells during blink toggle

This is more complex than italic/strikethrough, which are purely static
visual attributes.

### Approach: ATTR_BLINK Stored in Attr + Timer in Event Loop

**Option A — Use the DIM approach (no bit needed):**
On monochrome, continue mapping blink → bold (current behavior).
On color, map blink → bright background color (the "bright background"
interpretation used by many DOS-era terminals where SGR 5 selects bright
background colors 8-15 instead of actual blinking).

**Option B — True blink with timer:**
Use a free bit... except there are no free bits left after italic +
strikethrough. Options to create one:

1. **Reclaim ATTR_HAS_COLOR (0x40)**: Compute color presence at render time
   by checking `screen_color != NULL && color.fg != COLOR_DEFAULT`. Adds
   ~2 byte-comparisons per cell during rendering. See performance analysis.

2. **Lazy-allocated blink array**: A separate `unsigned char blink_mask[][]`
   (1 bit per cell packed, or 1 byte per cell). Allocated only when SGR 5
   is received. Memory: 6,600 bytes per session (1 byte/cell) or 825 bytes
   (1 bit/cell packed).

3. **Accept blink → bold mapping**: Actual text blinking is widely considered
   a hostile UI pattern. Most modern terminals disable it by default
   (iTerm2, Alacritty, kitty all default to no-blink). The bold mapping is
   reasonable and matches historical VT220 behavior.

### Recommended: Option A (bright background) for Color, Bold for Mono

This requires no attr bit and provides the BBS-authentic experience:

```c
case 5:
case 6:
    /* Blink */
    term->cur_attr |= ATTR_BOLD;        /* monochrome fallback */
    if (g_has_color_qd) {
        /* Bright background interpretation (DOS-style) */
        if (term->cur_bg != COLOR_DEFAULT && term->cur_bg < 8)
            term->cur_bg += 8;          /* 0-7 → 8-15 bright */
        term->cur_attr |= ATTR_HAS_COLOR;
    }
    break;
```

This is what the majority of BBS terminals (SyncTERM, NetRunner, mTCP)
actually do — SGR 5 means "bright background" not "blink."

### If True Blink Is Desired Later

If actual blinking is ever wanted, it can be added without an attr bit by
using a lazy-allocated blink bitfield array:

```c
/* In Terminal struct: */
unsigned char *blink_mask;  /* NULL until SGR 5 received, then 825 bytes */

/* Blink timer in event loop (main.c): */
static unsigned long next_blink = 0;
if (TickCount() >= next_blink) {
    g_blink_visible = !g_blink_visible;
    next_blink = TickCount() + 30;  /* 30 ticks ≈ 500ms */
    /* Mark rows with blink cells dirty */
}
```

Memory: +825 bytes per session (packed bits), allocated on first SGR 5.
CPU: One TickCount() check per event loop iteration (~20 cycles, negligible).

---

## Memory Impact Analysis

### Static (Code)

| Component                   | Size        |
|-----------------------------|-------------|
| Cell type macros (inline)   | 0 bytes     |
| ATTR_ITALIC define          | 0 bytes     |
| ATTR_STRIKETHROUGH define   | 0 bytes     |
| Italic: TextFace change     | ~8 bytes    |
| Strikethrough: LineTo code  | ~40 bytes   |
| Dim: color_dim() function   | ~80 bytes   |
| Blink: bright bg code       | ~30 bytes   |
| SGR parser changes          | ~60 bytes   |
| **Total code increase**     | **~220 bytes** |

### Per-Session (Dynamic)

| Component                     | Size     |
|-------------------------------|----------|
| No new struct fields          | 0 bytes  |
| No new arrays                 | 0 bytes  |
| **Total per-session increase**| **0 bytes** |

### Comparison: SGR Attributes vs CP437

| Metric                  | SGR Attributes | CP437 (ANSI-BBS) |
|-------------------------|----------------|-------------------|
| Static code increase    | ~220 bytes     | ~1,800 bytes      |
| Const data              | 0 bytes        | 512 bytes         |
| Per-session memory      | 0 bytes        | 1 byte            |
| New source files        | 0              | 2 (cp437.c/h)     |
| Modified files          | 6              | 8                 |
| Lines changed           | ~80            | ~600-800          |
| New glyph primitives    | 0              | ~35               |
| Attr bits consumed      | 2 (italic, strikethrough) | 1 (CP437) |
| Remaining free bits     | 0              | 0                 |
| Risk of regression      | Low-Medium     | Low               |

**SGR attributes are ~8× smaller in implementation scope than CP437.**

---

## CPU / Performance Impact

### Italic

**Cost**: Zero additional overhead. `TextFace(face)` is already called per
run. Adding `italic` to the bitmask adds 1 OR instruction (~4 cycles) per
run, not per cell. The `last_face` cache avoids redundant `TextFace()` calls.

QuickDraw italic rendering itself is the same cost as normal text — the
shear transform is applied at `DrawChar()` time with no additional calls.

| Operation              | Cycles | Notes |
|------------------------|--------|-------|
| Check ATTR_ITALIC      | ~8     | AND + branch, per run |
| OR italic into face    | ~4     | Only if italic is set |
| TextFace() call        | ~0     | Already called, just different value |
| DrawChar() with italic | ~200   | Same cost as normal DrawChar |
| **Per-cell overhead**  | **0**  | Cost is per-run, amortized |

### Strikethrough

**Cost**: 2 QuickDraw calls per run (MoveTo + LineTo), only for runs that
have ATTR_STRIKETHROUGH set. For a full row of 80 strikethrough characters
in one run, it's 2 calls. For mixed runs, it's 2 calls per strikethrough
segment.

| Operation                | Cycles | Notes |
|--------------------------|--------|-------|
| Check ATTR_STRIKETHROUGH | ~8     | AND + branch, per run |
| MoveTo()                 | ~100   | Once per run |
| LineTo()                 | ~150   | Once per run, horizontal line |
| **Per-run overhead**     | **~250** | Only for strikethrough runs |
| **Per-cell amortized**   | **~3** | 250 / 80 cells per run |

**Worst case**: Full 80×24 screen of strikethrough text, each cell a
different run (no batching). 1,920 runs × 250 cycles = 480K cycles = **60ms**.
This is pathological and won't occur in practice.

**Typical case**: 1-3 strikethrough runs per row, 1-5 rows per update.
5 runs × 250 = 1,250 cycles = **0.16ms**. Negligible.

### Dim (via color)

**Cost per SGR 2**: One call to `color_dim()` which does:
- `color_get_rgb()`: table lookup or computation (~30 cycles)
- Halve 3 components: 3 shifts (~12 cycles)
- `color_nearest_256()`: iterate 256 palette entries (~5000 cycles)

The 5000-cycle `color_nearest_256()` cost happens once per SGR 2 sequence,
not per cell. Typical occurrence: 1-5 times per screen update. Total:
5-25K cycles = **0.6-3ms**. Acceptable.

**Render cost**: Zero additional — dimmed color is just a different palette
index, same `RGBForeColor()` call path as any other color.

### Blink (bright background)

**Cost per SGR 5**: One comparison + add (~8 cycles). Negligible.

**Render cost**: Zero additional — bright background is just a different
palette index.

### Cell Type Macro Overhead

Old: `if (run_attr & ATTR_GLYPH)` → AND + branch (~8 cycles)
New: `if (CELL_IS_GLYPH(run_attr))` → AND + CMP + branch (~12 cycles)

**Difference**: +4 cycles per cell type check. 3 checks per run in
draw_row(). For a full 80×24 screen with 24 runs: 24 × 3 × 4 = 288 cycles.
**0.04ms**. Completely negligible.

### Summary

| Attribute      | Per-Cell Cost | Per-Row Cost | Full Screen |
|----------------|---------------|--------------|-------------|
| Italic         | 0 cycles      | ~12 cycles   | ~0.04ms     |
| Strikethrough  | ~3 cycles     | ~250 cycles  | ~0.75ms     |
| Dim            | 0 cycles      | 0 cycles     | 0ms (color set at parse time) |
| Blink          | 0 cycles      | 0 cycles     | 0ms (color set at parse time) |
| Cell type macro| +4 cycles     | +12 cycles   | ~0.04ms     |
| **Total**      |               |              | **< 1ms**   |

**When no new attributes are used**: The only cost is the cell type macro
overhead (+0.04ms per full screen). Effectively zero regression.

---

## Rendering Details by Platform

### Monochrome (Mac Plus, System 6)

| Attribute      | Rendering                                       |
|----------------|------------------------------------------------|
| Bold           | TextFace(bold) — thicker strokes (unchanged)    |
| Italic         | TextFace(italic) — sheared characters (NEW)      |
| Underline      | TextFace(underline) — line below baseline (unchanged) |
| Strikethrough  | MoveTo+LineTo at mid-height (NEW)                |
| Inverse        | PaintRect + srcBic text (unchanged)              |
| Dim            | Clear bold (same as current)                     |
| Blink          | Set bold (same as current)                       |

All combinations compose naturally. `TextFace(bold | italic | underline)`
is valid and renders bold-italic-underlined text. Strikethrough is an
independent post-pass line.

### Color (System 7)

| Attribute      | Rendering                                        |
|----------------|--------------------------------------------------|
| Bold           | TextFace(bold) + bright color (unchanged)        |
| Italic         | TextFace(italic) (NEW)                           |
| Underline      | TextFace(underline) (unchanged)                  |
| Strikethrough  | LineTo in foreground color (NEW)                 |
| Inverse        | Swap fg/bg colors (unchanged)                    |
| Dim            | Dimmed foreground color via color_dim() (NEW)    |
| Blink          | Bright background color (NEW, DOS-style)         |

---

## Tradeoffs

### 1. Italic Character Width at Small Fonts

QuickDraw italic shears characters rightward. At Monaco 9 (cell width ~6px),
the rightmost column of the italic character may extend 1-2px into the
adjacent cell. This is:
- **Visually acceptable** — italic text is recognizably different from normal
- **Not functionally broken** — the next character's MoveTo positions it
  correctly, overwriting any overhang
- **Consistent with Mac behavior** — the Mac Plus renders italic Monaco
  this way in every application (SimpleText, TeachText, etc.)

If overhang is visually objectionable at specific font sizes, a future fix
could clip each cell to its bounding rect with `ClipRect()` before drawing.
Cost: 1 additional QuickDraw call per cell (~100 cycles). Only worth doing
if users report issues.

### 2. Strikethrough Line Position

The strikethrough line is drawn at `row_y + g_cell_height / 2`. This is the
vertical center of the cell, not the typographic "strikethrough position"
(which varies by font). For monospaced terminal fonts at small sizes, center
positioning is standard and matches what xterm, Terminal.app, and other
terminal emulators do.

For double-height lines (DECDHL), strikethrough should be drawn at
`row_y + (g_cell_height * 2) / 2` = full cell height. The existing
`cell_h` variable already accounts for double-height, so this works
automatically.

### 3. Dim Is Not Reversible

The color-based dim approach bakes the dim effect into the foreground color
at parse time. If the server sends `SGR 2; SGR 22` (dim then un-dim), the
original color is lost — `SGR 22` clears bold but doesn't know what the
pre-dim color was.

**Mitigation**: Track the "raw" color in `cur_fg` and only apply dimming
at render time instead of parse time. This requires checking a `dim` flag
during rendering, which means... we need a bit. Circular problem.

**Practical impact**: Very low. SGR 2→SGR 22 transitions within a single
line are extremely rare. Servers typically set all attributes at once
(`ESC[2;37m`) and reset with SGR 0. The SGR 0 (reset all) handler already
resets `cur_fg` to `COLOR_DEFAULT`, so full resets work correctly.

**Alternative**: Store the pre-dim color in a small stack/variable:
```c
unsigned char pre_dim_fg;   /* 1 byte in Terminal struct */
```
When SGR 2 is received, save `cur_fg` to `pre_dim_fg`, then dim. When
SGR 22 is received, restore `cur_fg` from `pre_dim_fg`. Cost: 1 byte per
session, ~10 lines of code. Worth doing for correctness.

### 4. No True Blink

The recommended approach maps blink to bright background (color) or bold
(monochrome) rather than implementing actual blinking. Reasons:

- **User experience**: Blinking text is universally considered hostile UI.
  iTerm2, Alacritty, kitty, Windows Terminal all default to no-blink.
- **CPU cost**: True blink requires timer-driven redraws every 500ms,
  consuming CPU cycles even when the user isn't interacting. On a
  68000@8MHz, this is a meaningful cost.
- **Attr bit**: No free bit available without further restructuring.
- **BBS compatibility**: The bright-background interpretation is what
  DOS terminals (MS-DOS ANSI.SYS) and most BBS terminals use. This is
  arguably more authentic for BBS use than actual blinking.

If true blink is ever desired, the lazy blink bitfield approach (Phase 5
"If True Blink Is Desired Later") can be added independently, without
consuming an attr bit.

### 5. Cell Type Encoding Is a Refactor with Touchpoints

The cell type merge touches 18 sites across 6 files. While each change is
mechanical (replace `& ATTR_X` with `CELL_IS_X()`), the risk of missing
one or introducing a typo is non-zero. A thorough grep + build + test cycle
is essential.

The `CELL_IS_X()` macros make the code slightly more verbose but also more
self-documenting. The 2-bit encoding is invisible to callers — they use the
same macro pattern.

### 6. Attr Byte Is Now Full

After this change, all 8 bits are allocated:
```
0x01  BOLD
0x02  UNDERLINE
0x04  INVERSE
0x08  CELL_TYPE_LO
0x10  CELL_TYPE_HI
0x20  ITALIC
0x40  HAS_COLOR
0x80  STRIKETHROUGH
```

Future attributes (hidden/concealed SGR 8, overline SGR 53) would require
either:
- Expanding TermCell to 4 bytes (doubles buffer memory: +13.2KB/buffer)
- Lazy-allocated extended attribute arrays (like screen_color)
- Merging rarely-used combinations

This is documented as a known constraint, not a blocking issue.

---

## Risks

### Low Risk
1. **Build break**: Mechanical refactor, well-defined changes. Caught at
   compile time if any site is missed (macro type mismatch).
2. **Memory**: +220 bytes code, 0 bytes per session. Negligible.
3. **Performance**: < 1ms overhead for full screen with all attributes.

### Medium Risk
4. **Italic overhang at small fonts**: Characters may visually bleed into
   adjacent cells. Acceptable on Mac Plus (matches native Mac behavior).
   Fixable with ClipRect if needed.
5. **Dim color accuracy**: Parse-time dimming loses original color on
   SGR 22 reset. Fixable with 1-byte pre-dim color save.

### Low Risk (Architecture)
6. **Attr byte full**: Documented constraint. No planned features require
   additional bits. Multiple escape hatches available if needed.

---

## Implementation Order

| Phase | Description            | Effort    | Files Changed |
|-------|------------------------|-----------|---------------|
| 1     | Cell type merge        | ~1 hour   | 6 files, 18 sites |
| 2     | True italic            | ~30 min   | 3 files, ~10 lines |
| 3     | True strikethrough     | ~30 min   | 3 files, ~15 lines |
| 4     | Dim via color          | ~1 hour   | 3 files, ~30 lines |
| 5     | Blink → bright bg      | ~15 min   | 1 file, ~8 lines |
| **Total** |                    | **~3 hours** | **~80 lines** |

All phases are independently testable. Phase 1 is a prerequisite for
Phases 2-3. Phases 4-5 are independent of everything.

---

## Interaction with Future CP437 Plan

If CP437 (ANSI-BBS) support is implemented later:
- The cell type field has a free encoding: `11` is braille, but CP437 could
  reuse the DEC graphics slot (DEC Special Graphics is irrelevant in CP437
  mode, so the same `01` encoding could mean "DEC graphics" in VT mode
  and "CP437 special char" in CP437 mode, distinguished by a `cp437_mode`
  flag on the Terminal struct).
- Alternatively, CP437 rendering could use the glyph pathway (CELL_TYPE_GLYPH)
  with CP437-specific glyph indices, requiring no additional attr bits.
- The attr byte being full is NOT a problem for CP437 — CP437 needs a mode
  flag on the Terminal struct, not a per-cell attr bit.

**Bottom line**: This SGR plan does not conflict with or block the CP437 plan.

---

## SGR 0 (Reset All) Completeness

The SGR 0 handler must clear all new attributes:

```c
case 0:
    /* Reset all attributes */
    term->cur_attr = ATTR_NORMAL;   /* clears bold, underline, inverse,
                                       italic, strikethrough, cell type */
    term->cur_fg = COLOR_DEFAULT;
    term->cur_bg = COLOR_DEFAULT;
    break;
```

`ATTR_NORMAL` is 0x00, which already clears all bits including the new ones.
No change needed.
