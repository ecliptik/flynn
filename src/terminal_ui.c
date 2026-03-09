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
#include "glyphs.h"

/* Cursor state */
static unsigned long	cursor_last_tick;
static short		cursor_visible;
static short		cursor_prev_row;
static short		cursor_prev_col;
static short		cursor_initialized;

/* Selection state */
static Selection	sel;

/* Runtime cell dimensions */
short			g_cell_width = CELL_WIDTH;
short			g_cell_height = CELL_HEIGHT;
short			g_cell_baseline = CELL_HEIGHT - 2;
short			g_font_id = 4;
short			g_font_size = 9;
static short		g_dark_mode = 0;

/* Row pixel helpers */
static short row_top(short row)    { return TOP_MARGIN + row * g_cell_height; }
static short row_bottom(short row) { return TOP_MARGIN + (row + 1) * g_cell_height; }
static short col_left(short col)   { return LEFT_MARGIN + col * g_cell_width; }
static short col_right(short col)  { return LEFT_MARGIN + (col + 1) * g_cell_width; }

static void draw_row(Terminal *term, short row);
static void draw_line_char(unsigned char ch, short x, short y,
	    unsigned char attr);
static void draw_braille(unsigned char pattern, short x, short y,
	    unsigned char attr);
static void draw_glyph_prim(unsigned char glyph_id, short x, short y,
	    unsigned char attr);
static void draw_glyph_bitmap(unsigned char glyph_id, short x, short y,
	    unsigned char attr);
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

	TextFont(g_font_id);
	TextSize(g_font_size);
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
 * term_ui_set_font - change terminal font and update cell metrics
 *
 * Uses GetFontInfo to measure the font and sets global cell dimensions.
 */
void
term_ui_set_font(WindowPtr win, short font_id, short font_size)
{
	FontInfo fi;
	GrafPtr old_port;

	GetPort(&old_port);
	SetPort(win);

	TextFont(font_id);
	TextSize(font_size);
	GetFontInfo(&fi);

	g_cell_width = fi.widMax;
	g_cell_height = fi.ascent + fi.descent + fi.leading;
	g_cell_baseline = fi.ascent + fi.leading;
	g_font_id = font_id;
	g_font_size = font_size;

	TextFace(0);
	TextMode(srcOr);

	SetPort(old_port);
}

/*
 * term_ui_set_dark_mode - enable/disable dark mode rendering
 */
void
term_ui_set_dark_mode(short enabled)
{
	g_dark_mode = enabled;
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

	TextFont(g_font_id);
	TextSize(g_font_size);

	for (row = 0; row < term->active_rows; row++) {
		if (!terminal_is_row_dirty(term, row))
			continue;

		/* Erase the row background */
		SetRect(&r, LEFT_MARGIN, row_top(row),
		    LEFT_MARGIN + term->active_cols * g_cell_width,
		    row_bottom(row));
		EraseRect(&r);

		draw_row(term, row);

		/* Dark mode: invert the rendered row (XOR all pixels) */
		if (g_dark_mode)
			InvertRect(&r);
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
 * attribute.  Non-bold plain text uses DrawText() for ~10x speedup
 * over per-character MoveTo()+DrawChar().  Bold and inverse text
 * still uses per-character positioning to avoid advance width drift.
 *
 * Selection bounds are pre-computed per row to avoid calling
 * term_ui_sel_contains() per cell.  Inner-loop x-coordinates use
 * incremental ADD instead of col_left() MULU (saves ~42 cycles/char
 * on 68000).
 */
static void
draw_row(Terminal *term, short row)
{
	char buf[TERM_COLS];
	short col, run_start, run_len;
	unsigned char run_attr;
	TermCell *cell;
	short baseline;
	short last_face = -1;
	short row_y;
	short sel_start_col, sel_end_col;

	baseline = row_top(row) + g_cell_baseline;
	row_y = row_top(row);

	/*
	 * Pre-compute selection column range for this row.
	 * Avoids calling term_ui_sel_contains() per cell.
	 */
	sel_start_col = -1;
	sel_end_col = -1;
	if (sel.active) {
		short sr, sc, er, ec;

		sel_normalize(&sr, &sc, &er, &ec);
		if (row >= sr && row <= er) {
			if (sr == er) {
				/* Single-row selection */
				sel_start_col = sc;
				sel_end_col = ec;
			} else if (row == sr) {
				sel_start_col = sc;
				sel_end_col = term->active_cols - 1;
			} else if (row == er) {
				sel_start_col = 0;
				sel_end_col = ec;
			} else {
				/* Middle row: fully selected */
				sel_start_col = 0;
				sel_end_col = term->active_cols - 1;
			}
		}
	}

	col = 0;
	while (col < term->active_cols) {
		unsigned char cell_attr;

		cell = terminal_get_display_cell(term, row, col);
		cell_attr = cell->attr;
		if (col >= sel_start_col && col <= sel_end_col)
			cell_attr ^= ATTR_INVERSE;
		run_attr = cell_attr;
		run_start = col;
		run_len = 0;

		/* Collect run of cells with same effective attributes */
		while (col < term->active_cols) {
			cell = terminal_get_display_cell(term, row, col);
			cell_attr = cell->attr;
			if (col >= sel_start_col && col <= sel_end_col)
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

		/* DEC Special Graphics: render each cell individually
		 * Batch PenNormal: set pen state before run, restore after */
		if (run_attr & ATTR_DEC_GRAPHICS) {
			short i, x;

			x = col_left(run_start);
			for (i = 0; i < run_len; i++) {
				draw_line_char(buf[i], x, row_y, run_attr);
				x += g_cell_width;
			}
			continue;
		}

		/* Braille patterns: render dot grid */
		if (run_attr & ATTR_BRAILLE) {
			short i, x;

			x = col_left(run_start);
			for (i = 0; i < run_len; i++) {
				draw_braille((unsigned char)buf[i],
				    x, row_y, run_attr);
				x += g_cell_width;
			}
			continue;
		}

		/* Custom glyphs: primitives and emoji */
		if (run_attr & ATTR_GLYPH) {
			short i, x;

			x = col_left(run_start);
			for (i = 0; i < run_len; i++) {
				unsigned char gid;

				gid = (unsigned char)buf[i];
				if (gid == GLYPH_WIDE_SPACER) {
					x += g_cell_width;
					continue;	/* skip second cell */
				}
				if (gid < GLYPH_EMOJI_BASE) {
					draw_glyph_prim(gid,
					    x, row_y, run_attr);
				} else {
					draw_glyph_bitmap(gid,
					    x, row_y, run_attr);
				}
				x += g_cell_width;
			}
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
			short x;

			/* Paint background black for inverse region */
			SetRect(&inv_r,
			    col_left(run_start), row_y,
			    col_left(run_start) + run_len * g_cell_width,
			    row_bottom(row));
			PaintRect(&inv_r);

			/*
			 * Draw text in white (srcBic: clears bits where
			 * text is).  Always use per-character positioning
			 * to avoid bold advance width drift.
			 * Use incremental x instead of col_left() MULU.
			 */
			TextMode(srcBic);
			x = col_left(run_start);
			{
				short i;
				for (i = 0; i < run_len; i++) {
					MoveTo(x, baseline);
					DrawChar(buf[i]);
					x += g_cell_width;
				}
			}
			TextMode(srcOr);
		} else if (run_attr & ATTR_BOLD) {
			/*
			 * Bold text: must use per-character MoveTo+DrawChar
			 * because QuickDraw bold adds 1px per char advance
			 * width, causing drift over a run.
			 * Use incremental x instead of col_left() MULU.
			 */
			short x = col_left(run_start);
			short i;
			for (i = 0; i < run_len; i++) {
				MoveTo(x, baseline);
				DrawChar(buf[i]);
				x += g_cell_width;
			}
		} else {
			/*
			 * Non-bold, non-inverse text: safe to use DrawText()
			 * for the entire run.  This batches ~80 QuickDraw
			 * calls into 1, giving ~10x speedup for plain text.
			 * The bold drift bug only affects bold TextFace.
			 */
			MoveTo(col_left(run_start), baseline);
			DrawText(buf, 0, run_len);
		}
	}

	/* Restore normal face */
	TextFace(0);
}

/*
 * draw_line_char - render a DEC Special Graphics character
 *
 * Draws box-drawing glyphs using QuickDraw primitives (MoveTo/LineTo)
 * within the cell at pixel position (x, y).  Supports bold (thicker
 * lines) and inverse (white-on-black) attributes.
 */
static void
draw_line_char(unsigned char ch, short x, short y, unsigned char attr)
{
	short cx, cy, right, bottom;
	Rect cell_r;

	cx = x + g_cell_width / 2;
	cy = y + g_cell_height / 2;
	right = x + g_cell_width - 1;
	bottom = y + g_cell_height - 1;

	SetRect(&cell_r, x, y, x + g_cell_width, y + g_cell_height);

	/* Inverse: paint cell black first, draw lines in white */
	if (attr & ATTR_INVERSE) {
		PaintRect(&cell_r);
		PenMode(patBic);
	}

	/* Bold: thicker horizontal pen */
	if (attr & ATTR_BOLD)
		PenSize(2, 1);
	else
		PenSize(1, 1);

	switch (ch) {
	case 'q':	/* ─ horizontal line */
		MoveTo(x, cy);
		LineTo(right, cy);
		break;

	case 'x':	/* │ vertical line */
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, y);
		LineTo(cx, bottom);
		break;

	case 'l':	/* ┌ upper-left corner */
		MoveTo(cx, cy);
		LineTo(right, cy);
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, cy);
		LineTo(cx, bottom);
		break;

	case 'k':	/* ┐ upper-right corner */
		MoveTo(x, cy);
		LineTo(cx, cy);
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, cy);
		LineTo(cx, bottom);
		break;

	case 'm':	/* └ lower-left corner */
		MoveTo(cx, cy);
		LineTo(right, cy);
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, y);
		LineTo(cx, cy);
		break;

	case 'j':	/* ┘ lower-right corner */
		MoveTo(x, cy);
		LineTo(cx, cy);
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, y);
		LineTo(cx, cy);
		break;

	case 'n':	/* ┼ crossing lines */
		MoveTo(x, cy);
		LineTo(right, cy);
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, y);
		LineTo(cx, bottom);
		break;

	case 't':	/* ├ left tee */
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, y);
		LineTo(cx, bottom);
		if (attr & ATTR_BOLD)
			PenSize(2, 1);
		MoveTo(cx, cy);
		LineTo(right, cy);
		break;

	case 'u':	/* ┤ right tee */
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, y);
		LineTo(cx, bottom);
		if (attr & ATTR_BOLD)
			PenSize(2, 1);
		MoveTo(x, cy);
		LineTo(cx, cy);
		break;

	case 'w':	/* ┬ top tee */
		MoveTo(x, cy);
		LineTo(right, cy);
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, cy);
		LineTo(cx, bottom);
		break;

	case 'v':	/* ┴ bottom tee */
		MoveTo(x, cy);
		LineTo(right, cy);
		PenSize((attr & ATTR_BOLD) ? 2 : 1, 1);
		MoveTo(cx, y);
		LineTo(cx, cy);
		break;

	case '`':	/* ◆ diamond */
		{
			Rect dr;

			SetRect(&dr, cx - 1, cy - 2, cx + 2, cy + 3);
			PaintRect(&dr);
		}
		break;

	case 'a':	/* ▒ checkerboard */
		FillRect(&cell_r, &qd.gray);
		break;

	case '~':	/* · centered dot */
		MoveTo(cx, cy);
		LineTo(cx, cy);
		break;

	case 'f':	/* ° degree symbol */
		{
			Rect dr;

			PenSize(1, 1);
			SetRect(&dr, cx - 1, cy - 3, cx + 2, cy);
			FrameRect(&dr);
		}
		break;

	case 'g':	/* ± plus/minus */
		/* Plus sign */
		MoveTo(x + 1, cy - 1);
		LineTo(right - 1, cy - 1);
		PenSize(1, 1);
		MoveTo(cx, cy - 4);
		LineTo(cx, cy + 1);
		/* Minus below */
		if (attr & ATTR_BOLD)
			PenSize(2, 1);
		MoveTo(x + 1, cy + 3);
		LineTo(right - 1, cy + 3);
		break;

	default:
		/* Unknown graphic: draw as regular text */
		{
			short bl = y + g_cell_baseline;

			if (attr & ATTR_INVERSE)
				TextMode(srcBic);
			MoveTo(x, bl);
			DrawChar(ch);
			if (attr & ATTR_INVERSE)
				TextMode(srcOr);
		}
		break;
	}

	PenNormal();
}

/*
 * draw_glyph_prim - render a primitive symbol using QuickDraw
 *
 * Draws arrows, checkmarks, shapes, card suits, etc. using
 * QuickDraw primitives.  All coordinates scale with font cell size.
 */
static void
draw_glyph_prim(unsigned char glyph_id, short x, short y,
    unsigned char attr)
{
	short cx, cy, right, bottom;
	short w4, h4;		/* quarter-cell offsets */
	Rect cell_r, r;

	cx = x + g_cell_width / 2;
	cy = y + g_cell_height / 2;
	right = x + g_cell_width - 1;
	bottom = y + g_cell_height - 1;
	w4 = g_cell_width / 4;
	h4 = g_cell_height / 4;

	SetRect(&cell_r, x, y, x + g_cell_width, y + g_cell_height);

	/* Inverse: paint cell black, draw in white */
	if (attr & ATTR_INVERSE) {
		PaintRect(&cell_r);
		PenMode(patBic);
	}

	/* Bold: thicker strokes */
	if (attr & ATTR_BOLD)
		PenSize(2, 1);
	else
		PenSize(1, 1);

	switch (glyph_id) {
	case GLYPH_ARROW_LEFT:
		/* Horizontal shaft + left arrowhead */
		MoveTo(x + 1, cy);
		LineTo(right - 1, cy);
		MoveTo(x + 1, cy);
		LineTo(x + w4 + 1, cy - h4);
		MoveTo(x + 1, cy);
		LineTo(x + w4 + 1, cy + h4);
		break;

	case GLYPH_ARROW_UP:
		/* Vertical shaft + up arrowhead */
		PenSize(1, 1);
		MoveTo(cx, y + 1);
		LineTo(cx, bottom - 1);
		MoveTo(cx, y + 1);
		LineTo(cx - w4, y + h4 + 1);
		MoveTo(cx, y + 1);
		LineTo(cx + w4, y + h4 + 1);
		break;

	case GLYPH_ARROW_RIGHT:
		/* Horizontal shaft + right arrowhead */
		MoveTo(x + 1, cy);
		LineTo(right - 1, cy);
		MoveTo(right - 1, cy);
		LineTo(right - w4 - 1, cy - h4);
		MoveTo(right - 1, cy);
		LineTo(right - w4 - 1, cy + h4);
		break;

	case GLYPH_ARROW_DOWN:
		/* Vertical shaft + down arrowhead */
		PenSize(1, 1);
		MoveTo(cx, y + 1);
		LineTo(cx, bottom - 1);
		MoveTo(cx, bottom - 1);
		LineTo(cx - w4, bottom - h4 - 1);
		MoveTo(cx, bottom - 1);
		LineTo(cx + w4, bottom - h4 - 1);
		break;

	case GLYPH_CHECK:
		/* Checkmark: short left stroke + long right stroke */
		MoveTo(x + 1, cy);
		LineTo(cx - 1, cy + h4);
		LineTo(right - 1, cy - h4 - 1);
		break;

	case GLYPH_CROSS:
		/* X: two diagonal lines */
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, bottom - 2);
		MoveTo(right - 1, y + 2);
		LineTo(x + 1, bottom - 2);
		break;

	case GLYPH_STAR_FILLED:
		/* Five points: draw as filled diamond + top spike */
		SetRect(&r, cx - w4, cy - h4, cx + w4 + 1, cy + h4 + 1);
		PaintRect(&r);
		MoveTo(cx, y + 1);
		LineTo(cx, cy - h4);
		MoveTo(x + 1, cy);
		LineTo(cx - w4, cy);
		MoveTo(right - 1, cy);
		LineTo(cx + w4, cy);
		MoveTo(cx, bottom - 1);
		LineTo(cx, cy + h4);
		break;

	case GLYPH_STAR_EMPTY:
		/* Star outline: similar to filled but FrameRect */
		SetRect(&r, cx - w4, cy - h4, cx + w4 + 1, cy + h4 + 1);
		FrameRect(&r);
		MoveTo(cx, y + 1);
		LineTo(cx, cy - h4);
		break;

	case GLYPH_HEART:
		/* Heart shape: two small circles + triangle below */
		SetRect(&r, x + 1, cy - h4, cx, cy + 1);
		PaintOval(&r);
		SetRect(&r, cx - 1, cy - h4, right - 1, cy + 1);
		PaintOval(&r);
		/* Triangle bottom */
		MoveTo(x + 1, cy);
		LineTo(cx, bottom - 1);
		LineTo(right - 1, cy);
		break;

	case GLYPH_CIRCLE_FILLED:
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		PaintOval(&r);
		break;

	case GLYPH_CIRCLE_EMPTY:
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		break;

	case GLYPH_SQUARE_FILLED:
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		PaintRect(&r);
		break;

	case GLYPH_SQUARE_EMPTY:
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameRect(&r);
		break;

	case GLYPH_TRI_UP:
		/* Triangle pointing up */
		MoveTo(cx, y + 2);
		LineTo(right - 1, bottom - 2);
		LineTo(x + 1, bottom - 2);
		LineTo(cx, y + 2);
		break;

	case GLYPH_TRI_RIGHT:
		/* Triangle pointing right */
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, cy);
		LineTo(x + 1, bottom - 2);
		LineTo(x + 1, y + 2);
		break;

	case GLYPH_TRI_DOWN:
		/* Triangle pointing down */
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, y + 2);
		LineTo(cx, bottom - 2);
		LineTo(x + 1, y + 2);
		break;

	case GLYPH_TRI_LEFT:
		/* Triangle pointing left */
		MoveTo(right - 1, y + 2);
		LineTo(x + 1, cy);
		LineTo(right - 1, bottom - 2);
		LineTo(right - 1, y + 2);
		break;

	case GLYPH_MUSIC_NOTE:
		/* Single note: stem + head */
		PenSize(1, 1);
		MoveTo(cx + 1, y + 2);
		LineTo(cx + 1, bottom - 2);
		SetRect(&r, cx - 1, bottom - 4, cx + 2, bottom - 1);
		PaintOval(&r);
		break;

	case GLYPH_MUSIC_NOTES:
		/* Double note: two stems + beam + heads */
		PenSize(1, 1);
		MoveTo(x + 2, y + 2);
		LineTo(x + 2, bottom - 2);
		MoveTo(right - 2, y + 2);
		LineTo(right - 2, bottom - 2);
		MoveTo(x + 2, y + 2);
		LineTo(right - 2, y + 2);
		SetRect(&r, x, bottom - 4, x + 3, bottom - 1);
		PaintOval(&r);
		SetRect(&r, right - 3, bottom - 4, right, bottom - 1);
		PaintOval(&r);
		break;

	case GLYPH_SPADE:
		/* Spade: inverted heart + stem */
		SetRect(&r, x + 1, y + 2, cx, cy + 1);
		PaintOval(&r);
		SetRect(&r, cx - 1, y + 2, right - 1, cy + 1);
		PaintOval(&r);
		MoveTo(x + 1, cy);
		LineTo(cx, y + 2);
		LineTo(right - 1, cy);
		PenSize(1, 1);
		MoveTo(cx, cy);
		LineTo(cx, bottom - 1);
		break;

	case GLYPH_CLUB:
		/* Club: three circles + stem */
		{
			short cs = g_cell_width / 5;

			if (cs < 1) cs = 1;
			SetRect(&r, cx - cs, y + 2, cx + cs + 1,
			    y + 2 + cs * 2 + 1);
			PaintOval(&r);
			SetRect(&r, x + 1, cy - cs, x + 1 + cs * 2 + 1,
			    cy + cs + 1);
			PaintOval(&r);
			SetRect(&r, right - cs * 2 - 1, cy - cs, right,
			    cy + cs + 1);
			PaintOval(&r);
			PenSize(1, 1);
			MoveTo(cx, cy);
			LineTo(cx, bottom - 1);
		}
		break;

	case GLYPH_DIAMOND:
		/* Diamond: four lines forming diamond shape */
		MoveTo(cx, y + 2);
		LineTo(right - 1, cy);
		LineTo(cx, bottom - 2);
		LineTo(x + 1, cy);
		LineTo(cx, y + 2);
		break;

	case GLYPH_LOZENGE:
		/* Lozenge: same as diamond, thinner */
		PenSize(1, 1);
		MoveTo(cx, y + 1);
		LineTo(right - 1, cy);
		LineTo(cx, bottom - 1);
		LineTo(x + 1, cy);
		LineTo(cx, y + 1);
		break;

	case GLYPH_ELLIPSIS_V:
		/* Vertical ellipsis: three dots */
		{
			short ds = (g_cell_width > 4) ? 2 : 1;

			SetRect(&r, cx - ds / 2, y + 2,
			    cx + (ds + 1) / 2, y + 2 + ds);
			PaintRect(&r);
			SetRect(&r, cx - ds / 2, cy - ds / 2,
			    cx + (ds + 1) / 2, cy + (ds + 1) / 2);
			PaintRect(&r);
			SetRect(&r, cx - ds / 2, bottom - 2 - ds,
			    cx + (ds + 1) / 2, bottom - 2);
			PaintRect(&r);
		}
		break;

	case GLYPH_DASH_EM:
		/* Em dash: wide horizontal line */
		MoveTo(x, cy);
		LineTo(right, cy);
		break;

	case GLYPH_BLOCK_FULL:
		/* Full block: solid fill */
		PaintRect(&cell_r);
		break;

	case GLYPH_BLOCK_UPPER:
		/* Upper half block */
		SetRect(&r, x, y, x + g_cell_width, cy);
		PaintRect(&r);
		break;

	case GLYPH_BLOCK_LOWER:
		/* Lower half block */
		SetRect(&r, x, cy, x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;

	case GLYPH_BLOCK_LEFT:
		/* Left half block */
		SetRect(&r, x, y, cx, y + g_cell_height);
		PaintRect(&r);
		break;

	case GLYPH_BLOCK_RIGHT:
		/* Right half block */
		SetRect(&r, cx, y, x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;

	case GLYPH_QUAD_UL:
		/* Quadrant upper left */
		SetRect(&r, x, y, cx, cy);
		PaintRect(&r);
		break;

	case GLYPH_QUAD_UR:
		/* Quadrant upper right */
		SetRect(&r, cx, y, x + g_cell_width, cy);
		PaintRect(&r);
		break;

	case GLYPH_QUAD_LL:
		/* Quadrant lower left */
		SetRect(&r, x, cy, cx, y + g_cell_height);
		PaintRect(&r);
		break;

	case GLYPH_QUAD_LR:
		/* Quadrant lower right */
		SetRect(&r, cx, cy, x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;

	case GLYPH_QUAD_UL_UR_LL:
		/* Three quadrants: UL + UR + LL (missing LR) */
		SetRect(&r, x, y, x + g_cell_width, cy);
		PaintRect(&r);
		SetRect(&r, x, cy, cx, y + g_cell_height);
		PaintRect(&r);
		break;

	case GLYPH_QUAD_UL_UR_LR:
		/* Three quadrants: UL + UR + LR (missing LL) */
		SetRect(&r, x, y, x + g_cell_width, cy);
		PaintRect(&r);
		SetRect(&r, cx, cy, x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;

	case GLYPH_ASTERISK_TEARDROP:
		/* 6-spoke asterisk */
		PenSize(1, 1);
		MoveTo(cx, y + 2);
		LineTo(cx, bottom - 2);
		MoveTo(x + 1, cy - h4);
		LineTo(right - 1, cy + h4);
		MoveTo(x + 1, cy + h4);
		LineTo(right - 1, cy - h4);
		break;

	case GLYPH_ASTERISK_FOUR:
		/* 4-spoke asterisk (+ rotated 45 degrees = X) */
		PenSize(1, 1);
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, bottom - 2);
		MoveTo(right - 1, y + 2);
		LineTo(x + 1, bottom - 2);
		break;

	case GLYPH_ASTERISK_HEAVY:
		/* Heavy 6-spoke asterisk */
		PenSize(2, 1);
		MoveTo(cx, y + 2);
		LineTo(cx, bottom - 2);
		MoveTo(x + 1, cy - h4);
		LineTo(right - 1, cy + h4);
		MoveTo(x + 1, cy + h4);
		LineTo(right - 1, cy - h4);
		break;

	case GLYPH_ASTERISK_OP:
		/* Small asterisk operator */
		PenSize(1, 1);
		MoveTo(cx, cy - h4);
		LineTo(cx, cy + h4);
		MoveTo(cx - w4, cy - h4 / 2);
		LineTo(cx + w4, cy + h4 / 2);
		MoveTo(cx - w4, cy + h4 / 2);
		LineTo(cx + w4, cy - h4 / 2);
		break;

	case GLYPH_SQ_MED_FILLED:
		/* Black medium square: slightly smaller than full */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		PaintRect(&r);
		break;

	case GLYPH_SQ_MED_EMPTY:
		/* White medium square: slightly smaller than full */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameRect(&r);
		break;

	case GLYPH_PLAY:
		/* Play symbol: right-pointing filled triangle */
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, cy);
		LineTo(x + 1, bottom - 2);
		LineTo(x + 1, y + 2);
		break;

	case GLYPH_ASTERISK_8:
		/* Eight-spoked asterisk: 4 lines through center */
		PenSize(1, 1);
		MoveTo(cx, y + 2);
		LineTo(cx, bottom - 2);
		MoveTo(x + 1, cy);
		LineTo(right - 1, cy);
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, bottom - 2);
		MoveTo(right - 1, y + 2);
		LineTo(x + 1, bottom - 2);
		break;

	case GLYPH_CHEVRON_RIGHT:
		/* Heavy right-pointing chevron > */
		if (attr & ATTR_BOLD)
			PenSize(2, 1);
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, cy);
		LineTo(x + 1, bottom - 2);
		break;

	case GLYPH_WARNING:
		/* Warning triangle with ! inside */
		PenSize(1, 1);
		MoveTo(cx, y + 1);
		LineTo(right - 1, bottom - 1);
		LineTo(x + 1, bottom - 1);
		LineTo(cx, y + 1);
		/* Exclamation mark inside */
		{
			short ey = cy - 1;

			MoveTo(cx, ey - 1);
			LineTo(cx, ey + 1);
			MoveTo(cx, ey + 3);
			LineTo(cx, ey + 3);
		}
		break;

	case GLYPH_CHECK_HEAVY:
		/* Heavy checkmark: like CHECK but thicker */
		PenSize(2, 1);
		MoveTo(x + 1, cy);
		LineTo(cx - 1, cy + h4);
		LineTo(right - 1, cy - h4 - 1);
		break;

	case GLYPH_CROSS_HEAVY:
		/* Heavy ballot X: like CROSS but thicker */
		PenSize(2, 1);
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, bottom - 2);
		MoveTo(right - 1, y + 2);
		LineTo(x + 1, bottom - 2);
		break;

	case GLYPH_SQ_SM_FILLED:
		/* Black small square: inset ~2px from cell edges */
		SetRect(&r, x + 2, y + 3, right - 1, bottom - 2);
		PaintRect(&r);
		break;

	case GLYPH_SQ_SM_EMPTY:
		/* White small square: inset ~2px from cell edges */
		SetRect(&r, x + 2, y + 3, right - 1, bottom - 2);
		FrameRect(&r);
		break;

	case GLYPH_DOT_MIDDLE:
		/* Centered dot: small filled circle at cell center */
		SetRect(&r, cx - 1, cy - 1, cx + 1, cy + 1);
		PaintOval(&r);
		break;

	case GLYPH_BOX_H:
		/* Light horizontal: edge to edge at vertical center */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + g_cell_width, cy);
		break;

	case GLYPH_BOX_V:
		/* Light vertical: edge to edge at horizontal center */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_BOX_DR:
		/* Down-right corner: center to right, center to bottom */
		PenSize(1, 1);
		MoveTo(cx, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx, cy);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_BOX_DL:
		/* Down-left corner: left to center, center to bottom */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(cx, cy);
		MoveTo(cx, cy);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_BOX_UR:
		/* Up-right corner: center to right, top to center */
		PenSize(1, 1);
		MoveTo(cx, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx, y);
		LineTo(cx, cy);
		break;

	case GLYPH_BOX_UL:
		/* Up-left corner: left to center, top to center */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(cx, cy);
		MoveTo(cx, y);
		LineTo(cx, cy);
		break;

	case GLYPH_BOX_VR:
		/* Vert + right tee: full vertical, center to right */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, y + g_cell_height);
		MoveTo(cx, cy);
		LineTo(x + g_cell_width, cy);
		break;

	case GLYPH_BOX_VL:
		/* Vert + left tee: full vertical, left to center */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, y + g_cell_height);
		MoveTo(x, cy);
		LineTo(cx, cy);
		break;

	case GLYPH_BOX_DH:
		/* Down + horizontal tee: full horizontal, center to bottom */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx, cy);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_BOX_UH:
		/* Up + horizontal tee: full horizontal, top to center */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx, y);
		LineTo(cx, cy);
		break;

	case GLYPH_BOX_VH:
		/* Cross: full horizontal + full vertical */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx, y);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_SHADE_LIGHT:
		/* Light shade ~25% fill */
		if (attr & ATTR_INVERSE)
			FillRect(&cell_r, &qd.dkGray);
		else
			FillRect(&cell_r, &qd.ltGray);
		break;

	case GLYPH_SHADE_MEDIUM:
		/* Medium shade ~50% fill */
		FillRect(&cell_r, &qd.gray);
		break;

	case GLYPH_SHADE_DARK:
		/* Dark shade ~75% fill */
		if (attr & ATTR_INVERSE)
			FillRect(&cell_r, &qd.ltGray);
		else
			FillRect(&cell_r, &qd.dkGray);
		break;

	default:
		/* Unknown primitive: draw ? */
		{
			short bl = y + g_cell_baseline;

			if (attr & ATTR_INVERSE)
				TextMode(srcBic);
			MoveTo(x, bl);
			DrawChar('?');
			if (attr & ATTR_INVERSE)
				TextMode(srcOr);
		}
		break;
	}

	PenNormal();
}

/*
 * draw_glyph_bitmap - render a bitmap emoji using CopyBits
 *
 * Builds a BitMap on the stack from the GlyphBitmap data and blits
 * it centered in the 2-cell-wide area.  Fixed 10x10 pixel size.
 */
static void
draw_glyph_bitmap(unsigned char glyph_id, short x, short y,
    unsigned char attr)
{
	const GlyphBitmap *bm;
	BitMap src_bits;
	Rect src_r, dst_r, cell_r;
	short dx, dy, cell2_w;

	bm = glyph_get_bitmap(glyph_id);
	if (!bm) {
		/* No bitmap: draw ? as fallback */
		draw_glyph_prim(glyph_id, x, y, attr);
		return;
	}

	/* 2-cell wide area */
	cell2_w = g_cell_width * 2;
	SetRect(&cell_r, x, y, x + cell2_w, y + g_cell_height);

	/* Inverse: paint 2-cell area black */
	if (attr & ATTR_INVERSE)
		PaintRect(&cell_r);

	/* Build source BitMap from glyph data */
	src_bits.baseAddr = (Ptr)bm->bits;
	src_bits.rowBytes = bm->rowBytes;
	SetRect(&src_bits.bounds, 0, 0, bm->width, bm->height);

	/* Source rect: entire bitmap */
	SetRect(&src_r, 0, 0, bm->width, bm->height);

	/* Center bitmap in 2-cell area */
	dx = x + (cell2_w - bm->width) / 2;
	dy = y + (g_cell_height - bm->height) / 2;
	SetRect(&dst_r, dx, dy, dx + bm->width, dy + bm->height);

	/* Blit: srcOr for normal, srcBic for inverse (white on black) */
	CopyBits(&src_bits, &qd.thePort->portBits,
	    &src_r, &dst_r,
	    (attr & ATTR_INVERSE) ? srcBic : srcOr, NULL);
}

/*
 * draw_braille - render a Braille pattern (U+2800-U+28FF)
 *
 * Standard Braille bit layout in a 2x4 dot grid:
 *   bit 0 = top-left       bit 3 = top-right
 *   bit 1 = mid-left       bit 4 = mid-right
 *   bit 2 = low-left       bit 5 = low-right
 *   bit 6 = bottom-left    bit 7 = bottom-right
 *
 * Dot size scales with cell dimensions.
 */
static void
draw_braille(unsigned char pattern, short x, short y, unsigned char attr)
{
	Rect cell_r, dot_r;
	short dot_w, dot_h, gap_x, gap_y;
	short dx, dy;
	short col_idx, row_idx;
	/* Bit positions: [row][col] */
	static const unsigned char bit_pos[4][2] = {
		{ 0, 3 }, { 1, 4 }, { 2, 5 }, { 6, 7 }
	};

	SetRect(&cell_r, x, y, x + g_cell_width, y + g_cell_height);

	/* Inverse: paint cell black, draw dots in white */
	if (attr & ATTR_INVERSE) {
		PaintRect(&cell_r);
		PenMode(patBic);
	}

	/* Compute dot dimensions (scale with font) */
	dot_w = g_cell_width / 4;
	dot_h = g_cell_height / 6;
	if (dot_w < 1) dot_w = 1;
	if (dot_h < 1) dot_h = 1;

	/* Bold: slightly larger dots */
	if (attr & ATTR_BOLD) {
		if (dot_w < g_cell_width / 3)
			dot_w++;
		if (dot_h < g_cell_height / 5)
			dot_h++;
	}

	/* Spacing between dot centers */
	gap_x = g_cell_width / 2;
	gap_y = g_cell_height / 4;

	/* Draw dots */
	for (row_idx = 0; row_idx < 4; row_idx++) {
		for (col_idx = 0; col_idx < 2; col_idx++) {
			if (!(pattern & (1 << bit_pos[row_idx][col_idx])))
				continue;

			dx = x + (g_cell_width / 4) + col_idx * gap_x
			    - dot_w / 2;
			dy = y + (g_cell_height / 8) + row_idx * gap_y
			    - dot_h / 2;

			SetRect(&dot_r, dx, dy, dx + dot_w, dy + dot_h);
			PaintRect(&dot_r);
		}
	}

	if (attr & ATTR_INVERSE)
		PenMode(patCopy);
	PenNormal();
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
	for (row = 0; row < term->active_rows; row++) {
		if (terminal_is_row_dirty(term, row)) {
			SetRect(&r, LEFT_MARGIN, row_top(row),
			    LEFT_MARGIN + term->active_cols * g_cell_width,
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
		    LEFT_MARGIN + term->active_cols * g_cell_width,
		    row_bottom(cursor_prev_row));
		InvalRect(&r);

		/* Invalidate new cursor position row */
		SetRect(&r, LEFT_MARGIN, row_top(crow),
		    LEFT_MARGIN + term->active_cols * g_cell_width,
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
	while (e < term->active_cols - 1) {
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
	if (hi >= term->active_rows) hi = term->active_rows - 1;

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
	if (er >= term->active_rows) er = term->active_rows - 1;

	for (r = sr; r <= er; r++)
		term->dirty[r] = 1;
}

/*
 * term_ui_save_state - save cursor blink and selection state to UIState
 */
void
term_ui_save_state(UIState *dst)
{
	dst->cursor_last_tick = cursor_last_tick;
	dst->cursor_visible = cursor_visible;
	dst->cursor_prev_row = cursor_prev_row;
	dst->cursor_prev_col = cursor_prev_col;
	dst->cursor_initialized = cursor_initialized;
	dst->sel = sel;
}

/*
 * term_ui_load_state - restore cursor blink and selection state from UIState
 */
void
term_ui_load_state(UIState *src)
{
	cursor_last_tick = src->cursor_last_tick;
	cursor_visible = src->cursor_visible;
	cursor_prev_row = src->cursor_prev_row;
	cursor_prev_col = src->cursor_prev_col;
	cursor_initialized = src->cursor_initialized;
	sel = src->sel;
}
