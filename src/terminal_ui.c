/*
 * terminal_ui.c - Terminal rendering for classic Macintosh
 *
 * Renders the VT100 terminal screen buffer to a Mac window using
 * Monaco 9pt font.  Supports bold, underline, and inverse attributes.
 * Uses dirty-row tracking to minimise redraws and DrawText() for
 * batched output.
 *
 * Copyright (c) 2024-2026 Flynn project
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <ToolUtils.h>
#include <OSUtils.h>
#include "terminal_ui.h"

/* Cursor state */
static unsigned long	cursor_last_tick;
static short		cursor_visible;
static short		cursor_prev_row;
static short		cursor_prev_col;
static short		cursor_initialized;

/* Selection state */
static Selection	sel;

/* Row pixel helpers */
static short row_top(short row)    { return TOP_MARGIN + row * CELL_HEIGHT; }
static short row_bottom(short row) { return TOP_MARGIN + (row + 1) * CELL_HEIGHT; }
static short col_left(short col)   { return LEFT_MARGIN + col * CELL_WIDTH; }
static short col_right(short col)  { return LEFT_MARGIN + (col + 1) * CELL_WIDTH; }

static void draw_row(Terminal *term, short row);
static void draw_cursor(Terminal *term, short on);
static void sel_normalize(short *sr, short *sc, short *er, short *ec);
static void find_word_bounds(Terminal *term, short row, short col,
	    short *start, short *end);

/*
 * term_ui_init - set up font and cursor state
 */
void
term_ui_init(WindowPtr win, Terminal *term)
{
	GrafPtr old_port;

	GetPort(&old_port);
	SetPort(win);

	TextFont(4);		/* Monaco */
	TextSize(9);
	TextFace(0);		/* normal */
	TextMode(srcOr);

	cursor_last_tick = TickCount();
	cursor_visible = 0;
	cursor_prev_row = 0;
	cursor_prev_col = 0;
	cursor_initialized = 1;

	SetPort(old_port);
}

/*
 * term_ui_draw - render terminal contents, only dirty rows
 *
 * Called from BeginUpdate/EndUpdate in the update handler.
 * If force_all is needed (e.g. window reveal), mark all rows dirty
 * in the Terminal before calling.
 */
void
term_ui_draw(WindowPtr win, Terminal *term)
{
	short row;
	Rect r;

	TextFont(4);
	TextSize(9);

	for (row = 0; row < TERM_ROWS; row++) {
		if (!terminal_is_row_dirty(term, row))
			continue;

		/* Erase the row background */
		SetRect(&r, LEFT_MARGIN, row_top(row),
		    LEFT_MARGIN + TERM_COLS * CELL_WIDTH, row_bottom(row));
		EraseRect(&r);

		draw_row(term, row);
	}

	terminal_clear_dirty(term);

	/* Draw cursor only when viewing live terminal and cursor enabled */
	if (term->scroll_offset == 0 && term->cursor_visible)
		draw_cursor(term, 1);
}

/*
 * draw_row - render one row of terminal cells
 *
 * Scans left-to-right, batching consecutive cells with the same
 * attribute into single DrawText() calls for efficiency.
 */
static void
draw_row(Terminal *term, short row)
{
	char buf[TERM_COLS];
	short col, run_start, run_len;
	unsigned char run_attr;
	TermCell *cell;
	short baseline;
	short sel_active;
	short last_face = -1;

	baseline = row_top(row) + CELL_HEIGHT - 2;	/* baseline ~2px from bottom */
	sel_active = sel.active;

	col = 0;
	while (col < TERM_COLS) {
		unsigned char cell_attr;

		cell = terminal_get_display_cell(term, row, col);
		cell_attr = cell->attr;
		if (sel_active && term_ui_sel_contains(row, col))
			cell_attr ^= ATTR_INVERSE;
		run_attr = cell_attr;
		run_start = col;
		run_len = 0;

		/* Collect run of cells with same effective attributes */
		while (col < TERM_COLS) {
			cell = terminal_get_display_cell(term, row, col);
			cell_attr = cell->attr;
			if (sel_active && term_ui_sel_contains(row, col))
				cell_attr ^= ATTR_INVERSE;
			if (cell_attr != run_attr)
				break;
			buf[run_len] = cell->ch;
			run_len++;
			col++;
		}

		/* Skip runs of plain spaces (already erased) */
		if (run_attr == ATTR_NORMAL) {
			short all_space, i;

			all_space = 1;
			for (i = 0; i < run_len; i++) {
				if (buf[i] != ' ') {
					all_space = 0;
					break;
				}
			}
			if (all_space)
				continue;
		}

		/* Set text face for this run (cached to avoid redundant calls) */
		{
			short face = 0;

			if (run_attr & ATTR_BOLD)
				face |= bold;
			if (run_attr & ATTR_UNDERLINE)
				face |= underline;
			if (face != last_face) {
				TextFace(face);
				last_face = face;
			}
		}

		if (run_attr & ATTR_INVERSE) {
			Rect inv_r;

			/* Paint background black for inverse region */
			SetRect(&inv_r,
			    col_left(run_start), row_top(row),
			    col_left(run_start) + run_len * CELL_WIDTH,
			    row_bottom(row));
			PaintRect(&inv_r);

			/* Draw text in white (srcBic: clears bits where text is) */
			TextMode(srcBic);
			MoveTo(col_left(run_start), baseline);
			DrawText(buf, 0, run_len);
			TextMode(srcOr);
		} else {
			/* Normal or bold/underline text */
			MoveTo(col_left(run_start), baseline);
			DrawText(buf, 0, run_len);
		}
	}

	/* Restore normal face */
	TextFace(0);
}

/*
 * draw_cursor - draw or erase the block cursor at current position
 *
 * Uses XOR (patXor) so drawing twice erases it, giving us blink.
 */
static void
draw_cursor(Terminal *term, short on)
{
	short crow, ccol;
	Rect cur_r;

	terminal_get_cursor(term, &crow, &ccol);

	if (on) {
		SetRect(&cur_r,
		    col_left(ccol), row_top(crow),
		    col_right(ccol), row_bottom(crow));
		PenMode(patXor);
		PaintRect(&cur_r);
		PenNormal();

		cursor_visible = 1;
		cursor_prev_row = crow;
		cursor_prev_col = ccol;
	}
}

/*
 * term_ui_invalidate - invalidate only dirty rows
 *
 * Called after terminal_process() in the event loop.  Instead of
 * invalidating the entire window, only mark the rows that changed.
 */
void
term_ui_invalidate(WindowPtr win, Terminal *term)
{
	GrafPtr old_port;
	short row;
	short crow, ccol;
	Rect r;
	short any_dirty;

	GetPort(&old_port);
	SetPort(win);

	any_dirty = 0;
	for (row = 0; row < TERM_ROWS; row++) {
		if (terminal_is_row_dirty(term, row)) {
			SetRect(&r, LEFT_MARGIN, row_top(row),
			    LEFT_MARGIN + TERM_COLS * CELL_WIDTH,
			    row_bottom(row));
			InvalRect(&r);
			any_dirty = 1;
		}
	}

	/* If cursor moved, invalidate old and new cursor rows */
	terminal_get_cursor(term, &crow, &ccol);
	if (cursor_initialized &&
	    (crow != cursor_prev_row || ccol != cursor_prev_col)) {
		/* Invalidate old cursor position row */
		SetRect(&r, LEFT_MARGIN, row_top(cursor_prev_row),
		    LEFT_MARGIN + TERM_COLS * CELL_WIDTH,
		    row_bottom(cursor_prev_row));
		InvalRect(&r);

		/* Invalidate new cursor position row */
		SetRect(&r, LEFT_MARGIN, row_top(crow),
		    LEFT_MARGIN + TERM_COLS * CELL_WIDTH,
		    row_bottom(crow));
		InvalRect(&r);

		/* Mark both rows dirty so they get redrawn */
		term->dirty[cursor_prev_row] = 1;
		term->dirty[crow] = 1;
	}

	SetPort(old_port);
}

/*
 * term_ui_invalidate_all - invalidate entire terminal area
 *
 * Used for window reveals or full repaints.
 */
void
term_ui_invalidate_all(WindowPtr win)
{
	GrafPtr old_port;

	GetPort(&old_port);
	SetPort(win);
	InvalRect(&win->portRect);
	SetPort(old_port);
}

/*
 * term_ui_cursor_blink - toggle cursor blink on timer
 *
 * Call from event loop idle handler.  Toggles the cursor every
 * CURSOR_BLINK_TICKS ticks using XOR so it's self-inverting.
 */
void
term_ui_cursor_blink(WindowPtr win, Terminal *term)
{
	unsigned long now;
	GrafPtr old_port;
	short crow, ccol;
	Rect cur_r;

	if (!cursor_initialized || !term->cursor_visible || sel.active)
		return;

	now = TickCount();
	if (now - cursor_last_tick < CURSOR_BLINK_TICKS)
		return;

	cursor_last_tick = now;

	terminal_get_cursor(term, &crow, &ccol);

	GetPort(&old_port);
	SetPort(win);

	/* XOR the cursor rect to toggle it */
	SetRect(&cur_r,
	    col_left(ccol), row_top(crow),
	    col_right(ccol), row_bottom(crow));
	PenMode(patXor);
	PaintRect(&cur_r);
	PenNormal();

	cursor_visible = !cursor_visible;

	SetPort(old_port);
}

/*
 * sel_normalize - ensure start <= end in reading order
 */
static void
sel_normalize(short *sr, short *sc, short *er, short *ec)
{
	*sr = sel.anchor_row;
	*sc = sel.anchor_col;
	*er = sel.extent_row;
	*ec = sel.extent_col;

	if (*sr > *er || (*sr == *er && *sc > *ec)) {
		short tmp;

		tmp = *sr; *sr = *er; *er = tmp;
		tmp = *sc; *sc = *ec; *ec = tmp;
	}
}

/*
 * find_word_bounds - find start and end column of word at (row, col)
 *
 * A word is a contiguous run of non-space characters.
 * If the click is on a space, the "word" is just that single column.
 */
static void
find_word_bounds(Terminal *term, short row, short col,
    short *start, short *end)
{
	TermCell *cell;
	short s, e;

	cell = terminal_get_display_cell(term, row, col);
	if (cell->ch == ' ') {
		*start = col;
		*end = col;
		return;
	}

	s = col;
	while (s > 0) {
		cell = terminal_get_display_cell(term, row, s - 1);
		if (cell->ch == ' ')
			break;
		s--;
	}

	e = col;
	while (e < TERM_COLS - 1) {
		cell = terminal_get_display_cell(term, row, e + 1);
		if (cell->ch == ' ')
			break;
		e++;
	}

	*start = s;
	*end = e;
}

/*
 * term_ui_sel_start - begin a new character-mode selection
 */
void
term_ui_sel_start(short row, short col, short scroll_offset)
{
	sel.active = 1;
	sel.selecting = 1;
	sel.anchor_row = row;
	sel.anchor_col = col;
	sel.extent_row = row;
	sel.extent_col = col;
	sel.scroll_offset = scroll_offset;
	sel.word_mode = 0;
}

/*
 * term_ui_sel_start_word - begin a word-mode selection (double-click)
 */
void
term_ui_sel_start_word(short row, short col, short scroll_offset,
    Terminal *term)
{
	short ws, we;

	find_word_bounds(term, row, col, &ws, &we);

	sel.active = 1;
	sel.selecting = 1;
	sel.anchor_row = row;
	sel.anchor_col = ws;
	sel.extent_row = row;
	sel.extent_col = we;
	sel.scroll_offset = scroll_offset;
	sel.word_mode = 1;
	sel.word_anchor_start = ws;
	sel.word_anchor_end = we;
}

/*
 * term_ui_sel_extend - update extent during drag
 *
 * In word mode, extends to word boundaries.
 */
void
term_ui_sel_extend(short row, short col, Terminal *term)
{
	if (sel.word_mode) {
		short ws, we;
		short before_anchor;

		find_word_bounds(term, row, col, &ws, &we);

		/* Determine if dragging before or after anchor word */
		before_anchor = (row < sel.anchor_row ||
		    (row == sel.anchor_row && col < sel.word_anchor_start));

		if (before_anchor) {
			sel.anchor_col = sel.word_anchor_end;
			sel.extent_row = row;
			sel.extent_col = ws;
		} else {
			sel.anchor_col = sel.word_anchor_start;
			sel.extent_row = row;
			sel.extent_col = we;
		}
	} else {
		sel.extent_row = row;
		sel.extent_col = col;
	}
}

/*
 * term_ui_sel_clear - clear selection state
 */
void
term_ui_sel_clear(void)
{
	sel.active = 0;
	sel.selecting = 0;
	sel.word_mode = 0;
}

/*
 * term_ui_sel_finalize - end drag; clear if no actual selection made
 */
void
term_ui_sel_finalize(void)
{
	sel.selecting = 0;

	if (sel.anchor_row == sel.extent_row &&
	    sel.anchor_col == sel.extent_col) {
		sel.active = 0;
	}
}

/*
 * term_ui_sel_active - return non-zero if selection is visible
 */
short
term_ui_sel_active(void)
{
	return sel.active;
}

/*
 * term_ui_sel_get_range - get normalized selection range
 */
void
term_ui_sel_get_range(short *start_row, short *start_col,
    short *end_row, short *end_col)
{
	sel_normalize(start_row, start_col, end_row, end_col);
}

/*
 * term_ui_sel_contains - check if (row, col) is within selection
 *
 * Stream selection: not rectangular.
 */
short
term_ui_sel_contains(short row, short col)
{
	short sr, sc, er, ec;

	if (!sel.active)
		return 0;

	sel_normalize(&sr, &sc, &er, &ec);

	if (row < sr || row > er)
		return 0;

	if (sr == er)
		return (col >= sc && col <= ec);

	if (row == sr)
		return (col >= sc);

	if (row == er)
		return (col <= ec);

	return 1;
}

/*
 * term_ui_sel_check_double_click - detect double-click
 *
 * Returns non-zero if (when, row, col) is a double-click relative
 * to the last recorded click.
 */
short
term_ui_sel_check_double_click(unsigned long when, short row, short col)
{
	short is_dbl;

	is_dbl = (sel.last_click_ticks != 0 &&
	    (when - sel.last_click_ticks) <= (*(unsigned long *)0x02F0) &&
	    row == sel.last_click_row &&
	    col == sel.last_click_col);

	sel.last_click_ticks = when;
	sel.last_click_row = row;
	sel.last_click_col = col;

	return is_dbl;
}

/*
 * term_ui_sel_dirty_rows - mark rows between old and new extent dirty
 */
void
term_ui_sel_dirty_rows(Terminal *term, short old_extent_row,
    short new_extent_row)
{
	short lo, hi, r;

	lo = old_extent_row;
	hi = new_extent_row;
	if (lo > hi) {
		short tmp;
		tmp = lo; lo = hi; hi = tmp;
	}

	/* Also include anchor row in case of direction change */
	if (sel.anchor_row < lo)
		lo = sel.anchor_row;
	if (sel.anchor_row > hi)
		hi = sel.anchor_row;

	if (lo < 0) lo = 0;
	if (hi >= TERM_ROWS) hi = TERM_ROWS - 1;

	for (r = lo; r <= hi; r++)
		term->dirty[r] = 1;
}

/*
 * term_ui_sel_dirty_all - mark all rows in selection range dirty
 */
void
term_ui_sel_dirty_all(Terminal *term)
{
	short sr, sc, er, ec, r;

	if (!sel.active)
		return;

	sel_normalize(&sr, &sc, &er, &ec);

	if (sr < 0) sr = 0;
	if (er >= TERM_ROWS) er = TERM_ROWS - 1;

	for (r = sr; r <= er; r++)
		term->dirty[r] = 1;
}
