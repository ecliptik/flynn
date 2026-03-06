# Implementation Plan: Mouse-Based Text Selection

## Overview

Add click-drag text selection to Flynn's terminal window, integrating with the existing Cmd+C copy functionality. Selection renders as inverse video on the monochrome display.

## Design Decisions

1. **Selection state lives in `terminal_ui.c`** as a static struct, with public accessor API in `terminal_ui.h`. Clean separation from terminal emulation logic.
2. **Display coordinates** (row 0-23, col 0-79) — same coordinate space as `terminal_get_display_cell()`. No separate coordinate translation needed.
3. **Stream selection** (not rectangular) — like xterm. First/last rows partial, middle rows full width.
4. **ATTR_INVERSE XOR** — selected cells flip the inverse bit. Already-inverse cells become normal (double inversion). Bold/underline preserved.
5. **Clear selection on new data or keypress** — avoids complex scrollback-selection synchronization. Simple and safe for v1.
6. **No auto-scroll during drag** — can be added later. Primary use case (copy visible text) doesn't need it.
7. **Word selection via double-click** — word = contiguous non-space characters. Drag in word mode extends by word boundaries.

## Data Structure

Add to `terminal_ui.h`:

```c
typedef struct {
    short       active;             /* non-zero if selection visible */
    short       selecting;          /* non-zero during mouse drag */
    short       anchor_row;         /* original click point (display row 0-23) */
    short       anchor_col;         /* original click point (display col 0-79) */
    short       extent_row;         /* current drag endpoint row */
    short       extent_col;         /* current drag endpoint col */
    short       scroll_offset;      /* scroll_offset when selection started */
    short       word_mode;          /* non-zero for word selection (double-click) */
    short       word_anchor_start;  /* word start col at anchor row */
    short       word_anchor_end;    /* word end col at anchor row */
    unsigned long last_click_ticks; /* for double-click detection */
    short       last_click_row;     /* last click row */
    short       last_click_col;     /* last click col */
} Selection;
```

Memory cost: ~28 bytes. Static in `terminal_ui.c`.

## New Public API (`terminal_ui.h`)

```c
void  term_ui_sel_start(short row, short col, short scroll_offset);
void  term_ui_sel_start_word(short row, short col, short scroll_offset, Terminal *term);
void  term_ui_sel_extend(short row, short col, Terminal *term);
void  term_ui_sel_clear(void);
void  term_ui_sel_finalize(void);
short term_ui_sel_active(void);
void  term_ui_sel_get_range(short *start_row, short *start_col, short *end_row, short *end_col);
short term_ui_sel_contains(short row, short col);
short term_ui_sel_check_double_click(unsigned long when, short row, short col);
void  term_ui_sel_dirty_rows(Terminal *term, short old_extent_row, short new_extent_row);
void  term_ui_sel_dirty_all(Terminal *term);
```

## Files to Modify

### 1. `terminal_ui.h` — Selection API

- Add `Selection` struct typedef
- Add 11 new function prototypes (listed above)

### 2. `terminal_ui.c` — Selection logic + rendering

- Add `static Selection sel;`
- Implement all `term_ui_sel_*` functions
- Add `find_word_bounds()` static helper for word boundary detection
- **Modify `draw_row()`**: XOR `ATTR_INVERSE` bit for selected cells when building attribute runs:
  ```c
  cell_attr = cell->attr;
  if (term_ui_sel_contains(row, col))
      cell_attr ^= ATTR_INVERSE;
  ```
- **Modify `term_ui_cursor_blink()`**: suppress cursor blink when `sel.active` is true

### 3. `main.c` — Mouse event handling + copy integration

**New helpers:**
- `pixel_to_row(v)` — clamp `(v - TOP_MARGIN) / CELL_HEIGHT` to 0..23
- `pixel_to_col(h)` — clamp `(h - LEFT_MARGIN) / CELL_WIDTH` to 0..79
- `handle_content_click(event)` — double-click detection, shift-click extend, start new selection
- `track_selection_drag()` — `StillDown()/GetMouse()` loop, update extent, incremental redraw

**Modify `handle_mouse_down()`:**
- `inContent` case calls `handle_content_click(event)` instead of the TODO comment

**Modify `handle_key_down()`:**
- Clear selection on any keypress (except Cmd+C which should copy it first)

**Modify null event handler:**
- Clear selection when new data arrives (`out_len > 0`)

**Modify `do_copy()`:**
- If `term_ui_sel_active()`: copy only selected text (stream selection with partial first/last rows)
- If no selection: keep current whole-screen copy behavior
- Stream copy: first row from `start_col` to end, middle rows full, last row from start to `end_col`
- Trim trailing spaces per row, CR between rows

## Implementation Order

1. **Selection struct + API stubs** in `terminal_ui.h` / `terminal_ui.c`
2. **`term_ui_sel_contains()` + `term_ui_sel_get_range()`** — core selection geometry
3. **Modify `draw_row()`** — XOR inverse for selected cells
4. **`pixel_to_row/col` + `handle_content_click`** — basic click-to-select
5. **`track_selection_drag()`** — drag tracking with `StillDown()/GetMouse()`
6. **Double-click word selection** — `find_word_bounds()` + word-mode drag extension
7. **Modify `do_copy()`** — selection-aware copy
8. **Clear selection on keypress/data** — cleanup triggers
9. **Test**: click-drag, shift-click, double-click, Cmd+C with selection, Cmd+C without selection

## Performance Notes

- `term_ui_sel_contains()` is called per-cell during `draw_row()` — 80x24 = 1920 calls per full redraw. The function is a few comparisons, very fast even on 68000.
- During drag, only changed rows are redrawn (dirty flag optimization already exists).
- `StillDown()/GetMouse()` loop blocks the main event loop during drag — this is the standard Mac pattern and is expected. No TCP polling during drag, but drags are typically brief.

## Risk Assessment

- **Low risk**: Selection rendering (XOR inverse) is a proven pattern, minimal code change in `draw_row()`.
- **Low risk**: Mouse tracking with `StillDown()/GetMouse()` is the standard Mac Toolbox pattern.
- **Medium risk**: Interaction between selection and scrollback — mitigated by clearing selection on scroll/new data.
- **Low risk**: Performance — 1920 comparisons per redraw is negligible. Dirty-row optimization limits redraws during drag.

## Future Enhancements (not in this phase)

- Auto-scroll when dragging past window top/bottom edges
- Triple-click for line selection
- Preserve selection across scrollback navigation (adjust coordinates on scroll)
- Selection persists after Cmd+C (Mac convention) — currently clears; could be changed
