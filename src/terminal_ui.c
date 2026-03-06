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

/* Row pixel helpers */
static short row_top(short row)    { return TOP_MARGIN + row * CELL_HEIGHT; }
static short row_bottom(short row) { return TOP_MARGIN + (row + 1) * CELL_HEIGHT; }
static short col_left(short col)   { return LEFT_MARGIN + col * CELL_WIDTH; }
static short col_right(short col)  { return LEFT_MARGIN + (col + 1) * CELL_WIDTH; }

static void draw_row(Terminal *term, short row);
static void draw_cursor(Terminal *term, short on);

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

	baseline = row_top(row) + CELL_HEIGHT - 2;	/* baseline ~2px from bottom */

	col = 0;
	while (col < TERM_COLS) {
		cell = terminal_get_display_cell(term, row, col);
		run_attr = cell->attr;
		run_start = col;
		run_len = 0;

		/* Collect run of cells with same attributes */
		while (col < TERM_COLS) {
			cell = terminal_get_display_cell(term, row, col);
			if (cell->attr != run_attr)
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

		/* Set text face for this run */
		{
			short face = 0;

			if (run_attr & ATTR_BOLD)
				face |= bold;
			if (run_attr & ATTR_UNDERLINE)
				face |= underline;
			TextFace(face);
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

	if (!cursor_initialized || !term->cursor_visible)
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
