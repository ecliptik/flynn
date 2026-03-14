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
#include <Multiverse.h>
#include <string.h>
#include "terminal_ui.h"
#include "color.h"
#include "glyphs.h"

/*
 * Set foreground/background color by palette index using RGBForeColor/
 * RGBBackColor.  PmForeColor/PmBackColor don't work in Retro68's
 * Palette Manager trap glue, so we use direct RGB calls instead.
 *
 * Cached indices skip redundant RGBForeColor/RGBBackColor trap calls.
 * Reset to -1 at the start of each term_ui_draw() pass.
 */
static short cached_fg_idx = -1;
static short cached_bg_idx = -1;

static void
set_fg_color(unsigned char index)
{
	RGBColor rgb;
	if ((short)index == cached_fg_idx)
		return;
	cached_fg_idx = (short)index;
	color_get_rgb(index, &rgb);
	RGBForeColor(&rgb);
}

static void
set_bg_color(unsigned char index)
{
	RGBColor rgb;
	if ((short)index == cached_bg_idx)
		return;
	cached_bg_idx = (short)index;
	color_get_rgb(index, &rgb);
	RGBBackColor(&rgb);
}

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
static short		g_mono_dark = 0;	/* monochrome dark mode active */
static RgnHandle	g_scroll_rgn = 0L;	/* pre-allocated for ScrollRect */

/* Offscreen double buffer (Phase 1) */
static BitMap	g_offscreen;		/* offscreen bitmap descriptor */
static Ptr	g_offscreen_bits;	/* pixel data (NewPtr) */
static short	g_offscreen_cols;	/* allocated width in cells */
static short	g_offscreen_rows;	/* allocated height in cells */
static BitMap	g_saved_bits;		/* saved real portBits during offscreen */
static WindowPtr g_offscreen_win;	/* window that owns current offscreen */

/* Current effective colors for glyph shade blending (set by draw_row) */
static unsigned char	g_eff_fg = 0;
static unsigned char	g_eff_bg = 15;

/* Row pixel helpers */
static short row_top(short row)    { return TOP_MARGIN + row * g_cell_height; }
static short row_bottom(short row) { return TOP_MARGIN + (row + 1) * g_cell_height; }
static short col_left(short col)   { return LEFT_MARGIN + col * g_cell_width; }
static short col_right(short col)  { return LEFT_MARGIN + (col + 1) * g_cell_width; }

/*
 * mono_eff_inv - effective inverse for monochrome rendering
 *
 * In dark mode, normal cells render as white-on-black (srcBic/patBic).
 * In dark mode, ATTR_INVERSE cells render as black-on-white (srcOr).
 * This eliminates the EraseRect→InvertRect flash ("shutter effect").
 */
static short
mono_eff_inv(unsigned char attr)
{
	if (g_has_color_qd)
		return 0;
	if (g_mono_dark)
		return !(attr & ATTR_INVERSE);
	return (attr & ATTR_INVERSE) != 0;
}

/*
 * mono_cell_prep - prepare cell background for monochrome rendering
 *
 * Returns 1 if caller should use srcBic/patBic (white on black).
 * Returns 0 if caller should use srcOr/patCopy (black on white).
 * Fills cell background as needed (EraseRect for dark+inverse).
 */
static short
mono_cell_prep(Rect *cell_r, unsigned char attr)
{
	if (g_has_color_qd)
		return 0;
	if (g_mono_dark) {
		if (attr & ATTR_INVERSE) {
			/* Dark + inverse = normal appearance */
			EraseRect(cell_r);
			return 0;
		}
		/* Dark + normal: bg already black from row erase */
		return 1;
	}
	if (attr & ATTR_INVERSE) {
		/* Light + inverse: fill black */
		PaintRect(cell_r);
		return 1;
	}
	/* Light + normal: bg already white from row erase */
	return 0;
}

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
static void cursor_set_rect(Rect *r, short crow, short ccol,
	    unsigned char style);
static void sel_normalize(short *sr, short *sc, short *er, short *ec);
static void find_word_bounds(Terminal *term, short row, short col,
	    short *start, short *end);
static void glyph_cache_rebuild(void);

/*
 * offscreen_alloc - allocate/reallocate offscreen buffer for given dimensions
 *
 * Returns 1 on success, 0 on failure (caller should fall back to direct
 * drawing).  Reuses existing allocation if dimensions haven't changed.
 * Sets g_offscreen.bounds to match the window's portRect coordinate space
 * so all existing drawing coordinates work unmodified.
 */
static short
offscreen_alloc(WindowPtr win, short cols, short rows)
{
	short pixel_w, pixel_h, rb;
	long size;

	pixel_w = win->portRect.right - win->portRect.left;
	pixel_h = win->portRect.bottom - win->portRect.top;
	rb = ((pixel_w + 15) / 16) * 2;
	size = (long)rb * pixel_h;

	/* Reuse if dimensions and window unchanged */
	if (g_offscreen_bits && g_offscreen_cols == cols &&
	    g_offscreen_rows == rows && g_offscreen_win == win)
		return 1;

	/* Same dimensions, different window: reuse buffer,
	 * just update bounds and owner (no realloc needed) */
	if (g_offscreen_bits && g_offscreen_cols == cols &&
	    g_offscreen_rows == rows) {
		g_offscreen.bounds = win->portRect;
		g_offscreen_win = win;
		return 1;
	}

	if (g_offscreen_bits) {
		DisposePtr(g_offscreen_bits);
		g_offscreen_bits = 0L;
	}

	g_offscreen_bits = NewPtr(size);
	if (!g_offscreen_bits)
		return 0;

	memset(g_offscreen_bits, 0, size);

	g_offscreen.baseAddr = g_offscreen_bits;
	g_offscreen.rowBytes = rb;
	g_offscreen.bounds = win->portRect;

	g_offscreen_cols = cols;
	g_offscreen_rows = rows;
	g_offscreen_win = win;

	return 1;
}

/*
 * offscreen_free - free offscreen buffer
 */
static void
offscreen_free(void)
{
	if (g_offscreen_bits) {
		DisposePtr(g_offscreen_bits);
		g_offscreen_bits = 0L;
	}
	g_offscreen_cols = 0;
	g_offscreen_rows = 0;
	g_offscreen_win = 0L;
}

/*
 * term_ui_has_offscreen - check if valid offscreen buffer matches dimensions
 */
short
term_ui_has_offscreen(WindowPtr win, short cols, short rows)
{
	return g_offscreen_bits && g_offscreen_win == win &&
	    g_offscreen_cols == cols && g_offscreen_rows == rows;
}

/*
 * term_ui_blit_offscreen - CopyBits from offscreen buffer to window
 */
void
term_ui_blit_offscreen(WindowPtr win)
{
	if (!g_offscreen_bits)
		return;
	CopyBits(&g_offscreen, &win->portBits,
	    &g_offscreen.bounds, &g_offscreen.bounds,
	    srcCopy, 0L);
}

/*
 * term_ui_invalidate_offscreen - force reallocation on next draw
 *
 * Called on session switch to prevent stale offscreen blits.
 */
void
term_ui_invalidate_offscreen(void)
{
	g_offscreen_cols = 0;
	g_offscreen_rows = 0;
	g_offscreen_win = 0L;
}

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

	/* Rebuild glyph bitmap cache for new font metrics */
	glyph_cache_rebuild();

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
	short row, any_dirty;
	short use_offscreen;
	short was_dirty[TERM_ROWS];
	Rect r;

	TextFont(g_font_id);
	TextSize(g_font_size);
	g_mono_dark = g_dark_mode && !g_has_color_qd;

	/* Reset color cache so first set_fg/bg_color takes effect */
	cached_fg_idx = -1;
	cached_bg_idx = -1;

	/* Step 1: Erase cursor XOR artifact on REAL screen.
	 * ScrollRect would blit the XOR mark to wrong positions,
	 * and dirty row redraws need clean pixels underneath. */
	if (cursor_initialized && cursor_visible) {
		Rect cur_r;
		cursor_set_rect(&cur_r, cursor_prev_row,
		    cursor_prev_col, term->cursor_style);
		PenMode(patXor);
		PaintRect(&cur_r);
		PenNormal();
		cursor_visible = 0;
	}

	/* Track if this draw involves a scroll — suppress cursor
	 * drawing during scroll draws to prevent XOR artifacts
	 * from being blitted by subsequent ScrollRect calls */
	{
	short had_scroll = term->scroll_pending;

	/* If cursor moved and no scroll, mark old/new rows dirty */
	if (cursor_initialized && !had_scroll) {
		short crow, ccol;
		terminal_get_cursor(term, &crow, &ccol);
		if (crow != cursor_prev_row || ccol != cursor_prev_col) {
			term->dirty[cursor_prev_row] = 1;
			term->dirty[crow] = 1;
		}
	}

	/* Step 2: ScrollRect on REAL screen — blit existing pixels
	 * instead of redrawing all rows.  Only the newly exposed
	 * rows (marked dirty by term_scroll_up/down) need drawing. */
	if (term->scroll_pending) {
		short rgn_height = term->scroll_rgn_bot -
		    term->scroll_rgn_top + 1;

		if (term->scroll_count < rgn_height) {
			Rect scroll_r;
			short dv;

			SetRect(&scroll_r, LEFT_MARGIN,
			    row_top(term->scroll_rgn_top),
			    LEFT_MARGIN +
			    term->active_cols * g_cell_width,
			    row_bottom(term->scroll_rgn_bot));

			if (term->scroll_dir > 0)
				dv = -(term->scroll_count *
				    g_cell_height);
			else
				dv = term->scroll_count *
				    g_cell_height;

			/* In dark mode, set background to black so
			 * ScrollRect fills exposed region with black
			 * instead of white */
			if (g_dark_mode || g_mono_dark)
				BackPat(&qd.black);

			/* Use pre-allocated region to avoid
			 * NewRgn/DisposeRgn overhead per draw */
			if (!g_scroll_rgn)
				g_scroll_rgn = NewRgn();
			SetEmptyRgn(g_scroll_rgn);
			ScrollRect(&scroll_r, 0, dv, g_scroll_rgn);
			/* Validate so Window Manager doesn't
			 * generate updateEvt → full redraw */
			ValidRgn(g_scroll_rgn);

			if (g_dark_mode || g_mono_dark)
				BackPat(&qd.white);
		}
		term->scroll_pending = 0;
	}

	/* Check if any rows are dirty */
	any_dirty = 0;
	for (row = 0; row < term->active_rows; row++) {
		if (terminal_is_row_dirty(term, row)) {
			any_dirty = 1;
			break;
		}
	}

	/* Step 3-4: Switch to offscreen for dirty row rendering.
	 * All QuickDraw calls between SetPortBits go to the
	 * invisible offscreen buffer — no visible flash. */
	use_offscreen = any_dirty &&
	    offscreen_alloc(win, term->active_cols, term->active_rows);

	if (use_offscreen) {
		/* Save real portBits */
		g_saved_bits = win->portBits;

		/* Phase 3: Sync scroll result into offscreen.
		 * After ScrollRect shifted real-screen pixels,
		 * copy the scroll region so offscreen matches. */
		if (had_scroll) {
			Rect scroll_area;
			SetRect(&scroll_area, LEFT_MARGIN,
			    row_top(term->scroll_rgn_top),
			    LEFT_MARGIN +
			    term->active_cols * g_cell_width,
			    row_bottom(term->scroll_rgn_bot));
			CopyBits(&win->portBits, &g_offscreen,
			    &scroll_area, &scroll_area,
			    srcCopy, 0L);
		}

		/* Redirect all QD operations to offscreen */
		SetPortBits(&g_offscreen);
	}

	/* Save dirty flags before render loop clears them —
	 * needed for partial CopyBits after rendering */
	{
		short di;
		for (di = 0; di < TERM_ROWS; di++)
			was_dirty[di] = terminal_is_row_dirty(term, di);
	}

	/* Step 5: Dirty row loop (renders to offscreen if active) */
	for (row = 0; row < term->active_rows; row++) {
		if (!was_dirty[row])
			continue;

		/* Erase the row background */
		SetRect(&r, LEFT_MARGIN, row_top(row),
		    LEFT_MARGIN + term->active_cols * g_cell_width,
		    row_bottom(row));

		if (g_has_color_qd && g_dark_mode) {
			/* Color dark mode: erase with black background */
			set_bg_color(0);
			EraseRect(&r);
		} else if (g_has_color_qd) {
			/* Color light mode: erase with white background */
			set_bg_color(15);
			EraseRect(&r);
		} else if (g_mono_dark) {
			/* Mono dark: fill black directly (no white flash) */
			PaintRect(&r);
		} else {
			EraseRect(&r);
		}

		/* Set default fg color before drawing row */
		if (g_has_color_qd)
			set_fg_color(g_dark_mode ? 15 : 0);

		/* Skip draw_row for blank rows on mono — EraseRect already cleared */
		if (!g_has_color_qd) {
			TermCell *rc;
			short blank, ci;
			if (term->scroll_offset == 0)
				rc = &term->screen[row][0];
			else {
				/* For scrollback, skip optimization — always draw */
				goto do_draw;
			}
			blank = 1;
			for (ci = 0; ci < term->active_cols; ci++) {
				if (rc[ci].ch != ' ' || rc[ci].attr != ATTR_NORMAL) {
					blank = 0;
					break;
				}
			}
			if (blank)
				continue;
		}
do_draw:
		draw_row(term, row);

		/*
		 * On color systems, draw_row() handles dark mode
		 * by resolving COLOR_DEFAULT to white-on-black.
		 * On monochrome, draw_row/sub-functions now render
		 * directly in white-on-black (srcBic/patBic) to
		 * avoid the EraseRect→InvertRect flash.
		 */
	}

	terminal_clear_dirty(term);

	/* Draw scrollback position indicator on right edge */
	if (term->scroll_offset > 0 && term->sb_count > 0) {
		Rect indicator_r;
		short win_height, indicator_y, indicator_h;
		short right_edge;
		short bottom_y;

		/* Calculate position proportional to scroll offset */
		right_edge = win->portRect.right - 2;
		win_height = win->portRect.bottom - TOP_MARGIN * 2;

		/* Erase the rail area first to avoid artifacts */
		SetRect(&indicator_r, right_edge - 4, TOP_MARGIN,
		    right_edge + 1, TOP_MARGIN + win_height);
		if (g_dark_mode)
			PaintRect(&indicator_r);
		else
			EraseRect(&indicator_r);

		/* Draw thin rail line for full scroll area */
		PenNormal();
		if (g_dark_mode) {
			PenPat(&qd.white);
		}
		PenSize(1, 1);
		MoveTo(right_edge - 2, TOP_MARGIN);
		LineTo(right_edge - 2, TOP_MARGIN + win_height);
		PenNormal();

		/* Indicator height = visible fraction of total */
		indicator_h = (win_height * term->active_rows) /
		    (term->active_rows + term->sb_count);
		if (indicator_h < 8)
			indicator_h = 8;

		/* Indicator position = proportional to scroll offset */
		indicator_y = TOP_MARGIN +
		    ((win_height - indicator_h) *
		    (term->sb_count - term->scroll_offset)) /
		    term->sb_count;

		SetRect(&indicator_r, right_edge - 3, indicator_y,
		    right_edge, indicator_y + indicator_h);

		/* Draw filled rectangle as scroll indicator */
		if (g_dark_mode)
			EraseRect(&indicator_r);
		else
			PaintRect(&indicator_r);

		/* Draw "scrolled back" separator at bottom */
		bottom_y = row_top(term->active_rows - 1) + g_cell_height;
		PenPat(&qd.gray);
		MoveTo(LEFT_MARGIN, bottom_y + 1);
		LineTo(win->portRect.right - LEFT_MARGIN, bottom_y + 1);
		PenNormal();
	}

	/* Step 6-7: Restore real screen and blit offscreen → screen.
	 * Partial CopyBits: only blit dirty row strips instead of
	 * the entire offscreen.  Coalesces adjacent dirty rows into
	 * contiguous rectangles to minimise trap calls.  Falls back
	 * to full-screen blit when ≥20 rows dirty (cheaper than
	 * many small blits due to per-call trap overhead). */
	if (use_offscreen) {
		short dirty_count, first, has_sb_indicator;

		SetPortBits(&g_saved_bits);

		/* Count dirty rows for fallback decision */
		dirty_count = 0;
		for (row = 0; row < term->active_rows; row++) {
			if (was_dirty[row])
				dirty_count++;
		}

		has_sb_indicator = (term->scroll_offset > 0 &&
		    term->sb_count > 0);

		if (dirty_count >= 20 || has_sb_indicator) {
			/* Full blit: cheaper than 20+ small blits,
			 * or scrollback indicator touched right edge */
			CopyBits(&g_offscreen, &win->portBits,
			    &g_offscreen.bounds, &g_offscreen.bounds,
			    srcCopy, 0L);
		} else {
			/* Partial blit: coalesce adjacent dirty rows */
			first = -1;
			for (row = 0; row <= term->active_rows; row++) {
				if (row < term->active_rows &&
				    was_dirty[row]) {
					if (first < 0)
						first = row;
				} else if (first >= 0) {
					Rect blit_r;
					SetRect(&blit_r,
					    g_offscreen.bounds.left,
					    row_top(first),
					    g_offscreen.bounds.right,
					    row_bottom(row - 1));
					CopyBits(&g_offscreen,
					    &win->portBits,
					    &blit_r, &blit_r,
					    srcCopy, 0L);
					first = -1;
				}
			}
		}
	}

	/* Step 8: Draw cursor on REAL screen (after blit).
	 * During scroll draws, suppress cursor to avoid XOR
	 * artifacts blitted to wrong positions by next ScrollRect.
	 * cursor_blink() re-shows the cursor during idle time. */
	if (!had_scroll && term->scroll_offset == 0 &&
	    term->cursor_visible)
		draw_cursor(term, 1);
	} /* end had_scroll scope */
}

/*
 * try_merge_dec_run - merge consecutive identical DEC graphics chars
 *
 * Scans buf[pos..run_len) for consecutive cells matching the DEC char.
 * For 'q' (horizontal line), renders as a single LineTo spanning N cells.
 * For 'a' (checkerboard), renders as a single FillRect spanning N cells.
 *
 * Returns the number of cells consumed (>= 2) on success, 0 if not
 * mergeable or only 1 cell exists.
 */
static short
try_merge_dec_run(unsigned char ch, const char *buf, short pos,
    short run_len, short x, short y, unsigned char attr, short cell_w)
{
	short count, j, mw, cy;
	Rect mr;

	/* Only 'q' and 'a' are worth merging */
	if (ch != 'q' && ch != 'a')
		return 0;

	/* Count consecutive identical characters */
	count = 1;
	for (j = pos + 1; j < run_len; j++) {
		if ((unsigned char)buf[j] != ch)
			break;
		count++;
	}
	if (count < 2)
		return 0;

	mw = count * cell_w;

	switch (ch) {
	case 'q':
		/* ─ horizontal line: single LineTo spanning N cells */
		cy = y + g_cell_height / 2;
		if (attr & ATTR_BOLD)
			PenSize(2, 1);
		else
			PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + mw - 1, cy);
		return count;

	case 'a':
		/* ▒ checkerboard: single FillRect spanning N cells */
		SetRect(&mr, x, y, x + mw, y + g_cell_height);
		FillRect(&mr, &qd.gray);
		return count;
	}

	return 0;
}

/*
 * try_merge_glyph_run - merge consecutive identical glyphs into one call
 *
 * Scans buf[pos..run_len) for consecutive cells matching glyph_id.
 * For mergeable glyphs (horizontal lines, block fills, shades),
 * renders the entire run with a single QuickDraw call.
 *
 * Returns the number of cells consumed (>= 2) on success, 0 if the
 * glyph is not mergeable or only 1 cell exists.
 */
static short
try_merge_glyph_run(unsigned char gid, const char *buf, short pos,
    short run_len, short x, short y, unsigned char attr, short cell_w)
{
	short count, j, mw, cy;
	Rect mr;

	/* Count consecutive identical glyphs */
	count = 1;
	for (j = pos + 1; j < run_len; j++) {
		if ((unsigned char)buf[j] != gid)
			break;
		count++;
	}
	if (count < 2)
		return 0;

	mw = count * cell_w;
	cy = y + g_cell_height / 2;

	switch (gid) {
	/* --- Horizontal line merges --- */
	case GLYPH_BOX_H:
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + mw, cy);
		return count;

	case GLYPH_BOX2_H:
		PenSize(1, 1);
		MoveTo(x, cy - 1);
		LineTo(x + mw, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(x + mw, cy + 1);
		return count;

	/* --- Full-width block merges --- */
	case GLYPH_BLOCK_FULL:
		SetRect(&mr, x, y, x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;

	case GLYPH_BLOCK_UPPER:
		SetRect(&mr, x, y, x + mw, cy);
		PaintRect(&mr);
		return count;

	case GLYPH_BLOCK_LOWER:
		SetRect(&mr, x, cy, x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;

	/* --- Fractional lower block merges --- */
	case GLYPH_BLOCK_LOWER_1:
		SetRect(&mr, x,
		    y + g_cell_height - g_cell_height / 8,
		    x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;
	case GLYPH_BLOCK_LOWER_2:
		SetRect(&mr, x,
		    y + g_cell_height - g_cell_height * 2 / 8,
		    x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;
	case GLYPH_BLOCK_LOWER_3:
		SetRect(&mr, x,
		    y + g_cell_height - g_cell_height * 3 / 8,
		    x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;
	case GLYPH_BLOCK_LOWER_5:
		SetRect(&mr, x,
		    y + g_cell_height - g_cell_height * 5 / 8,
		    x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;
	case GLYPH_BLOCK_LOWER_6:
		SetRect(&mr, x,
		    y + g_cell_height - g_cell_height * 6 / 8,
		    x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;
	case GLYPH_BLOCK_LOWER_7:
		SetRect(&mr, x,
		    y + g_cell_height - g_cell_height * 7 / 8,
		    x + mw, y + g_cell_height);
		PaintRect(&mr);
		return count;
	case GLYPH_BLOCK_UPPER_1:
		SetRect(&mr, x, y, x + mw,
		    y + g_cell_height / 8);
		PaintRect(&mr);
		return count;

	/* --- Shade merges --- */
	case GLYPH_SHADE_LIGHT:
		SetRect(&mr, x, y, x + mw, y + g_cell_height);
		if (g_has_color_qd) {
			RGBColor frgb, brgb, blend;
			color_get_rgb(g_eff_fg, &frgb);
			color_get_rgb(g_eff_bg, &brgb);
			blend.red   = brgb.red +
			    (frgb.red - brgb.red) / 4;
			blend.green = brgb.green +
			    (frgb.green - brgb.green) / 4;
			blend.blue  = brgb.blue +
			    (frgb.blue - brgb.blue) / 4;
			RGBForeColor(&blend);
			PaintRect(&mr);
			set_fg_color(g_eff_fg);
		} else {
			if (attr & ATTR_INVERSE)
				FillRect(&mr, &qd.dkGray);
			else
				FillRect(&mr, &qd.ltGray);
		}
		return count;

	case GLYPH_SHADE_MEDIUM:
		SetRect(&mr, x, y, x + mw, y + g_cell_height);
		if (g_has_color_qd) {
			RGBColor frgb, brgb, blend;
			color_get_rgb(g_eff_fg, &frgb);
			color_get_rgb(g_eff_bg, &brgb);
			blend.red   = brgb.red +
			    (frgb.red - brgb.red) / 2;
			blend.green = brgb.green +
			    (frgb.green - brgb.green) / 2;
			blend.blue  = brgb.blue +
			    (frgb.blue - brgb.blue) / 2;
			RGBForeColor(&blend);
			PaintRect(&mr);
			set_fg_color(g_eff_fg);
		} else {
			FillRect(&mr, &qd.gray);
		}
		return count;

	case GLYPH_SHADE_DARK:
		SetRect(&mr, x, y, x + mw, y + g_cell_height);
		if (g_has_color_qd) {
			RGBColor frgb, brgb, blend;
			color_get_rgb(g_eff_fg, &frgb);
			color_get_rgb(g_eff_bg, &brgb);
			blend.red   = frgb.red -
			    (frgb.red - brgb.red) / 4;
			blend.green = frgb.green -
			    (frgb.green - brgb.green) / 4;
			blend.blue  = frgb.blue -
			    (frgb.blue - brgb.blue) / 4;
			RGBForeColor(&blend);
			PaintRect(&mr);
			set_fg_color(g_eff_fg);
		} else {
			if (attr & ATTR_INVERSE)
				FillRect(&mr, &qd.ltGray);
			else
				FillRect(&mr, &qd.dkGray);
		}
		return count;

	default:
		return 0;
	}
}

/*
 * Bitmap cache for frequently used multi-call glyphs.
 *
 * Pre-renders box drawing corners, tees, and double-line equivalents
 * to offscreen bitmaps.  CopyBits (1 trap) replaces 3-8 QuickDraw
 * calls per cached glyph.  Invalidated on font change.
 *
 * Memory: ~3.9KB static (49 glyphs × 2 variants × 40 bytes max).
 */
#define GLYPH_CACHE_COUNT	49
#define GLYPH_CACHE_MAX_RB	2	/* rowBytes for up to 15px wide */
#define GLYPH_CACHE_MAX_H	20	/* max cell height supported */
#define GLYPH_CACHE_BM_SIZE	(GLYPH_CACHE_MAX_RB * GLYPH_CACHE_MAX_H)

static const unsigned char cached_glyph_ids[GLYPH_CACHE_COUNT] = {
	/* Single-line box drawing (10) */
	GLYPH_BOX_V,   GLYPH_BOX_DR,  GLYPH_BOX_DL,
	GLYPH_BOX_UR,  GLYPH_BOX_UL,  GLYPH_BOX_VR,
	GLYPH_BOX_VL,  GLYPH_BOX_DH,  GLYPH_BOX_UH,
	GLYPH_BOX_VH,
	/* Double-line box drawing (10) */
	GLYPH_BOX2_V,  GLYPH_BOX2_DR,
	GLYPH_BOX2_DL, GLYPH_BOX2_UR, GLYPH_BOX2_UL,
	GLYPH_BOX2_VR, GLYPH_BOX2_VL, GLYPH_BOX2_DH,
	GLYPH_BOX2_UH, GLYPH_BOX2_VH,
	/* Quadrants — heavy in ANSI art (4) */
	GLYPH_QUAD_UL, GLYPH_QUAD_UR,
	GLYPH_QUAD_LL, GLYPH_QUAD_LR,
	/* Half blocks — non-mergeable individual cells (2) */
	GLYPH_BLOCK_LEFT, GLYPH_BLOCK_RIGHT,
	/* Arrows — btop scroll indicators, 6 QD calls each (2) */
	GLYPH_ARROW_UP, GLYPH_ARROW_DOWN,
	/* Mixed single/double junctions — btop panels (2) */
	GLYPH_BOX_sVdR, GLYPH_BOX_sVdL,
	/* Common symbols (2) */
	GLYPH_CIRCLE_FILLED, GLYPH_CIRCLE_EMPTY,
	/* Claude Code UI glyphs (4) */
	GLYPH_TRI_RIGHT_SM, GLYPH_DOT_MIDDLE,
	GLYPH_CHEVRON_RIGHT, GLYPH_ASTERISK_8,
	/* Three-quarter quadrants — ANSI art (4) */
	GLYPH_QUAD_UL_UR_LL, GLYPH_QUAD_UL_UR_LR,
	GLYPH_QUAD_UL_LL_LR, GLYPH_QUAD_UR_LL_LR,
	/* Diagonal quadrants — ANSI art (2) */
	GLYPH_QUAD_UL_LR, GLYPH_QUAD_UR_LL,
	/* Mixed junctions — btop panel headers (2) */
	GLYPH_BOX_dHsD, GLYPH_BOX_dHsU,
	/* Common symbols — CI/test output, dev tools (5) */
	GLYPH_CHECK, GLYPH_CHECK_HEAVY,
	GLYPH_CROSS, GLYPH_CROSS_HEAVY,
	GLYPH_STAR_FILLED
};

static struct {
	short		valid;
	short		font_id;
	short		font_size;
	short		cell_w;
	short		cell_h;
	short		rowBytes;
	unsigned char	bits[GLYPH_CACHE_COUNT][2][GLYPH_CACHE_BM_SIZE];
} g_glyph_cache;

/*
 * glyph_cache_find - look up glyph in cache, return index or -1
 */
static short
glyph_cache_find(unsigned char gid)
{
	short i;

	for (i = 0; i < GLYPH_CACHE_COUNT; i++) {
		if (cached_glyph_ids[i] == gid)
			return i;
	}
	return -1;
}

/*
 * glyph_cache_rebuild - re-render all cached glyphs for current font
 *
 * Creates a temporary offscreen GrafPort, renders each glyph to a
 * 1-bit bitmap, stores normal and bold variants.
 */
static void
glyph_cache_rebuild(void)
{
	GrafPort offPort;
	GrafPtr savePort;
	short i, v, rb, bm_size;
	Rect bounds;

	rb = (g_cell_width + 15) / 16 * 2;
	if (rb > GLYPH_CACHE_MAX_RB || g_cell_height > GLYPH_CACHE_MAX_H) {
		g_glyph_cache.valid = 0;
		return;
	}

	bm_size = rb * g_cell_height;
	SetRect(&bounds, 0, 0, g_cell_width, g_cell_height);

	GetPort(&savePort);
	OpenPort(&offPort);
	offPort.portBits.rowBytes = rb;
	offPort.portBits.bounds = bounds;
	offPort.portRect = bounds;
	RectRgn(offPort.clipRgn, &bounds);
	RectRgn(offPort.visRgn, &bounds);

	for (i = 0; i < GLYPH_CACHE_COUNT; i++) {
		for (v = 0; v < 2; v++) {
			unsigned char attr;
			unsigned char *dst;

			attr = v ? ATTR_BOLD : ATTR_NORMAL;
			dst = g_glyph_cache.bits[i][v];

			/* Clear bitmap (0 = white in 1-bit) */
			memset(dst, 0, bm_size);
			offPort.portBits.baseAddr = (Ptr)dst;

			/* Render glyph at origin */
			PenNormal();
			draw_glyph_prim(cached_glyph_ids[i],
			    0, 0, attr);
		}
	}

	PenNormal();
	ClosePort(&offPort);
	SetPort(savePort);

	g_glyph_cache.font_id = g_font_id;
	g_glyph_cache.font_size = g_font_size;
	g_glyph_cache.cell_w = g_cell_width;
	g_glyph_cache.cell_h = g_cell_height;
	g_glyph_cache.rowBytes = rb;
	g_glyph_cache.valid = 1;
}

/*
 * glyph_cache_draw - render a cached glyph via CopyBits
 *
 * Returns 1 if drawn from cache, 0 if not cached (caller should
 * fall back to draw_glyph_prim).
 */
static short
glyph_cache_draw(unsigned char gid, short x, short y, unsigned char attr)
{
	short idx, variant;
	BitMap src;
	Rect src_r, dst_r;

	if (!g_glyph_cache.valid)
		return 0;

	/* Invalidate if font changed */
	if (g_glyph_cache.font_id != g_font_id ||
	    g_glyph_cache.font_size != g_font_size) {
		g_glyph_cache.valid = 0;
		return 0;
	}

	idx = glyph_cache_find(gid);
	if (idx < 0)
		return 0;

	variant = (attr & ATTR_BOLD) ? 1 : 0;

	/* Set up source BitMap */
	src.baseAddr = (Ptr)g_glyph_cache.bits[idx][variant];
	src.rowBytes = g_glyph_cache.rowBytes;
	SetRect(&src.bounds, 0, 0,
	    g_glyph_cache.cell_w, g_glyph_cache.cell_h);
	SetRect(&src_r, 0, 0,
	    g_glyph_cache.cell_w, g_glyph_cache.cell_h);
	SetRect(&dst_r, x, y,
	    x + g_glyph_cache.cell_w, y + g_glyph_cache.cell_h);

	/* srcOr for normal, srcBic for inverse/dark mono */
	CopyBits(&src, &qd.thePort->portBits,
	    &src_r, &dst_r,
	    mono_eff_inv(attr) ? srcBic : srcOr,
	    0L);

	return 1;
}

/*
 * draw_row - render one row of terminal cells
 *
 * Scans left-to-right, batching consecutive cells with the same
 * attribute.  Uses DrawText() for all text rendering where possible.
 * Bold text uses the double-DrawText technique: two DrawText passes
 * at x and x+1 with plain TextFace replicates QuickDraw bold's
 * shift-and-OR with correct advance widths.  Only double-width
 * cells require per-character MoveTo()+DrawChar().
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
	unsigned char run_fg, run_bg;
	TermCell *cell;
	CellColor *cc;
	short baseline;
	short last_face = -1;
	short row_y;
	short sel_start_col, sel_end_col;
	short cell_w, eff_cols;
	unsigned char lattr;
	short use_color;
	short color_dirty = 0;
	TermCell *row_cells;
	CellColor *row_colors;
	short run_x, run_w;

	baseline = row_top(row) + g_cell_baseline;
	row_y = row_top(row);
	use_color = g_has_color_qd && term->has_color;

	/* Double-width/height: halve columns, double cell width */
	lattr = term->line_attr[row];
	if (lattr == LINE_ATTR_DBLW ||
	    lattr == LINE_ATTR_DBLH_TOP ||
	    lattr == LINE_ATTR_DBLH_BOT) {
		cell_w = g_cell_width * 2;
		eff_cols = term->active_cols / 2;
	} else {
		cell_w = g_cell_width;
		eff_cols = term->active_cols;
	}

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
				sel_end_col = eff_cols - 1;
			} else if (row == er) {
				sel_start_col = 0;
				sel_end_col = ec;
			} else {
				/* Middle row: fully selected */
				sel_start_col = 0;
				sel_end_col = eff_cols - 1;
			}
		}
	}

	/* Pre-compute row pointers to avoid per-cell function calls */
	if (term->scroll_offset == 0) {
		row_cells = &term->screen[row][0];
		if (use_color)
			row_colors = &term->screen_color[
			    row * TERM_COLS];
		else
			row_colors = 0L;
	} else {
		short sb_row = row - term->scroll_offset;
		if (sb_row < 0) {
			short sb_idx = term->sb_head +
			    (term->sb_count + sb_row);
			if (sb_idx < 0)
				sb_idx += TERM_SCROLLBACK_LINES;
			if (sb_idx >= TERM_SCROLLBACK_LINES)
				sb_idx -= TERM_SCROLLBACK_LINES;
			if (sb_idx < 0 ||
			    sb_idx >= TERM_SCROLLBACK_LINES) {
				row_cells = &term->screen[0][0];
				row_colors = 0L;
			} else {
				row_cells =
				    &term->scrollback[sb_idx][0];
				if (use_color && term->sb_color)
					row_colors =
					    &term->sb_color[
					    sb_idx * TERM_COLS];
				else
					row_colors = 0L;
			}
		} else {
			row_cells = &term->screen[sb_row][0];
			if (use_color)
				row_colors =
				    &term->screen_color[
				    sb_row * TERM_COLS];
			else
				row_colors = 0L;
		}
	}

	col = 0;
	while (col < eff_cols) {
		unsigned char cell_attr;

		run_start = col;
		run_len = 0;

		if (use_color) {
			/* Color path: collect run matching attr+color */
			unsigned char cell_fg, cell_bg;

			while (col < eff_cols) {
				cell = &row_cells[col];
				cell_attr = cell->attr;
				if (col >= sel_start_col &&
				    col <= sel_end_col)
					cell_attr ^= ATTR_INVERSE;

				cell_fg = COLOR_DEFAULT;
				cell_bg = COLOR_DEFAULT;
				if ((cell_attr & ATTR_HAS_COLOR) &&
				    row_colors) {
					cell_fg = row_colors[col].fg;
					cell_bg = row_colors[col].bg;
				}

				if (run_len == 0) {
					run_attr = cell_attr;
					run_fg = cell_fg;
					run_bg = cell_bg;
				} else {
					if (cell_attr != run_attr)
						break;
					if (cell_fg != run_fg ||
					    cell_bg != run_bg)
						break;
				}

				buf[run_len] = cell->ch;
				run_len++;
				col++;
			}

			/* Skip plain spaces unless they have bg */
			if (run_attr == ATTR_NORMAL &&
			    run_bg == COLOR_DEFAULT) {
				short all_space = 1, i;
				for (i = 0; i < run_len; i++) {
					if (buf[i] != ' ') {
						all_space = 0;
						break;
					}
				}
				if (all_space)
					continue;
			}
		} else {
			/* Mono path: collect run by attr only */
			run_fg = COLOR_DEFAULT;
			run_bg = COLOR_DEFAULT;

			while (col < eff_cols) {
				cell = &row_cells[col];
				cell_attr = cell->attr;
				if (col >= sel_start_col &&
				    col <= sel_end_col)
					cell_attr ^= ATTR_INVERSE;

				if (run_len == 0) {
					run_attr = cell_attr;
				} else {
					if (cell_attr != run_attr)
						break;
				}

				buf[run_len] = cell->ch;
				run_len++;
				col++;
			}

			/* Skip plain spaces (already erased) */
			if (run_attr == ATTR_NORMAL) {
				short all_space = 1, i;
				for (i = 0; i < run_len; i++) {
					if (buf[i] != ' ') {
						all_space = 0;
						break;
					}
				}
				if (all_space)
					continue;
			}
		}

		/* Pre-compute run pixel position and width */
		run_x = LEFT_MARGIN + run_start * cell_w;
		run_w = run_len * cell_w;

		/*
		 * Resolve effective fg/bg for rendering.
		 *
		 * On color systems, COLOR_DEFAULT resolves to:
		 *   Normal mode: fg=black(0), bg=white(15)
		 *   Dark mode:   fg=white(15), bg=black(0)
		 *
		 * On monochrome, inverse and dark mode are handled
		 * by mono_cell_prep/mono_eff_inv (srcBic/patBic).
		 */
		{
			unsigned char eff_fg, eff_bg;

			if (use_color) {
				/*
				 * Resolve effective fg/bg for
				 * rendering.  COLOR_DEFAULT resolves
				 * to fg=black(0)/bg=white(15) in
				 * normal mode, swapped in dark mode.
				 */
				eff_fg = run_fg;
				eff_bg = run_bg;

				if (eff_fg == COLOR_DEFAULT)
					eff_fg = g_dark_mode ? 15 : 0;
				if (eff_bg == COLOR_DEFAULT)
					eff_bg = g_dark_mode ? 0 : 15;

				/*
				 * Bold-to-bright promotion: standard
				 * ANSI/BBS behavior.  SGR 1;30m means
				 * "bright black" (dark gray, index 8),
				 * not "bold black" (index 0).  Without
				 * this, bold+dark colors are invisible
				 * on matching backgrounds.
				 */
				if ((run_attr & ATTR_BOLD) &&
				    eff_fg < 8)
					eff_fg += 8;

				if (run_attr & ATTR_INVERSE) {
					unsigned char tmp = eff_fg;
					eff_fg = eff_bg;
					eff_bg = tmp;
				}

				/* Draw bg when it differs from erase
				 * color (dark=black, normal=white) */
				{
					unsigned char erase_bg =
					    g_dark_mode ? 0 : 15;
					if (eff_bg != erase_bg) {
						Rect bg_r;
						SetRect(&bg_r,
						    run_x, row_y,
						    run_x + run_w,
						    row_bottom(row));
						set_bg_color(eff_bg);
						EraseRect(&bg_r);
					}
				}

				/* Set fg for pen-based paths
				 * (line drawing, braille, glyphs).
				 * Text fg set AFTER TextFace below. */
				set_fg_color(eff_fg);
				g_eff_fg = eff_fg;
				g_eff_bg = eff_bg;
			}

			/* Cell type dispatch */
			switch (run_attr & CELL_TYPE_MASK) {
			case CELL_TYPE_DEC: {
				short i, x, merged;
				Rect run_r;
				SetRect(&run_r, run_x, row_y,
				    run_x + run_w,
				    row_bottom(row));
				if (mono_cell_prep(&run_r,
				    run_attr))
					PenMode(patBic);
				x = run_x;
				i = 0;
				while (i < run_len) {
					merged =
					    try_merge_dec_run(
					    (unsigned char)buf[i],
					    buf, i, run_len,
					    x, row_y, run_attr,
					    cell_w);
					if (merged > 0) {
						x += merged * cell_w;
						i += merged;
						continue;
					}
					draw_line_char(buf[i],
					    x, row_y, run_attr);
					x += cell_w;
					i++;
				}
				PenNormal();
				if (use_color)
					color_dirty = 1;
				continue;
			}
			case CELL_TYPE_BRAILLE: {
				short i, x;
				Rect run_r;
				SetRect(&run_r, run_x, row_y,
				    run_x + run_w,
				    row_bottom(row));
				if (mono_cell_prep(&run_r,
				    run_attr))
					PenMode(patBic);
				x = run_x;
				for (i = 0; i < run_len; i++) {
					draw_braille((unsigned char)buf[i],
					    x, row_y, run_attr);
					x += cell_w;
				}
				PenNormal();
				if (use_color)
					color_dirty = 1;
				continue;
			}
			case CELL_TYPE_GLYPH: {
				short i, x, merged;
				unsigned char gid;
				Rect run_r;
				SetRect(&run_r, run_x, row_y,
				    run_x + run_w,
				    row_bottom(row));
				if (mono_cell_prep(&run_r,
				    run_attr))
					PenMode(patBic);
				x = run_x;
				i = 0;
				while (i < run_len) {
					gid = (unsigned char)buf[i];
					if (gid == GLYPH_WIDE_SPACER) {
						x += cell_w;
						i++;
						continue;
					}
					merged =
					    try_merge_glyph_run(gid,
					    buf, i, run_len,
					    x, row_y, run_attr,
					    cell_w);
					if (merged > 0) {
						x += merged * cell_w;
						i += merged;
						continue;
					}
					if (gid < GLYPH_EMOJI_BASE) {
						if (!glyph_cache_draw(
						    gid, x, row_y,
						    run_attr))
							draw_glyph_prim(
							    gid, x,
							    row_y,
							    run_attr);
					} else
						draw_glyph_bitmap(gid,
						    x, row_y, run_attr);
					x += cell_w;
					i++;
				}
				PenNormal();
				if (use_color)
					color_dirty = 1;
				continue;
			}
			default:
				break;  /* fall through to normal text */
			}

			/*
			 * Set text face for this run.
			 *
			 * For bold with normal cell width, we use
			 * the double-DrawText technique instead of
			 * TextFace(bold): two DrawText passes at
			 * x and x+1 with TextFace(0) replicate QD
			 * bold's shift-and-OR with correct advance
			 * widths (no drift).  14.5x faster than
			 * per-char MoveTo+DrawChar.
			 *
			 * Bold is still set for double-width cells
			 * which must use per-char rendering anyway.
			 */
			{
				short face = 0;
				if ((run_attr & ATTR_BOLD) &&
				    cell_w != g_cell_width)
					face |= bold;
				if (run_attr & ATTR_UNDERLINE)
					face |= underline;
				if (run_attr & ATTR_ITALIC)
					face |= italic;
				if (face != last_face) {
					TextFace(face);
					last_face = face;
				}
			}

			/*
			 * Set text fg AFTER TextFace — on Color
			 * QuickDraw, TextFace resets the port's
			 * foreground color.
			 */
			if (use_color)
				set_fg_color(eff_fg);

			if (run_attr & ATTR_INVERSE) {
				if (cell_w != g_cell_width) {
					/*
					 * Double-width inverse:
					 * per-char required.
					 */
					short x = run_x;
					short i;
					if (!use_color) {
						Rect inv_r;
						SetRect(&inv_r,
						    run_x, row_y,
						    run_x + run_w,
						    row_bottom(row));
						if (mono_cell_prep(
						    &inv_r, run_attr))
							TextMode(srcBic);
					}
					for (i = 0; i < run_len; i++) {
						MoveTo(x, baseline);
						DrawChar(buf[i]);
						x += cell_w;
					}
					if (!use_color)
						TextMode(srcOr);
				} else if (!use_color) {
					/*
					 * Monochrome inverse:
					 * batched DrawText.
					 * mono_cell_prep fills bg.
					 */
					Rect inv_r;
					short use_bic;
					SetRect(&inv_r,
					    run_x, row_y,
					    run_x + run_w,
					    row_bottom(row));
					use_bic =
					    mono_cell_prep(&inv_r,
					    run_attr);
					if (use_bic)
						TextMode(srcBic);
					MoveTo(col_left(run_start),
					    baseline);
					DrawText(buf, 0, run_len);
					if (run_attr & ATTR_BOLD) {
						MoveTo(
						    col_left(run_start)
						    + 1, baseline);
						DrawText(buf, 0,
						    run_len);
					}
					if (use_bic)
						TextMode(srcOr);
				} else {
					/*
					 * Color inverse: batched
					 * DrawText for all cases.
					 */
					MoveTo(col_left(run_start),
					    baseline);
					DrawText(buf, 0, run_len);
					if (run_attr & ATTR_BOLD) {
						MoveTo(
						    col_left(run_start)
						    + 1, baseline);
						DrawText(buf, 0,
						    run_len);
					}
				}
			} else if (cell_w != g_cell_width) {
				/*
				 * Double-width: per-char required
				 * for 2x cell spacing.
				 */
				short x = run_x;
				short i;
				if (g_mono_dark)
					TextMode(srcBic);
				for (i = 0; i < run_len; i++) {
					MoveTo(x, baseline);
					DrawChar(buf[i]);
					x += cell_w;
				}
				if (g_mono_dark)
					TextMode(srcOr);
			} else if (run_attr & ATTR_BOLD) {
				/*
				 * Bold normal-width: double-DrawText.
				 * Two passes at x and x+1 with plain
				 * TextFace replicates QD bold's
				 * shift-and-OR algorithm with correct
				 * advance widths.
				 */
				if (g_mono_dark)
					TextMode(srcBic);
				MoveTo(col_left(run_start), baseline);
				DrawText(buf, 0, run_len);
				MoveTo(col_left(run_start) + 1,
				    baseline);
				DrawText(buf, 0, run_len);
				if (g_mono_dark)
					TextMode(srcOr);
			} else {
				/*
				 * Normal text: batched DrawText.
				 */
				if (g_mono_dark)
					TextMode(srcBic);
				MoveTo(col_left(run_start), baseline);
				DrawText(buf, 0, run_len);
				if (g_mono_dark)
					TextMode(srcOr);
			}

			/* Strikethrough post-pass */
			if (run_attr & ATTR_STRIKETHROUGH) {
				short strike_y = row_y +
				    g_cell_height / 2;
				short x0 = run_x;
				short x1 = run_x + run_w;
				if (mono_eff_inv(run_attr))
					PenMode(patBic);
				MoveTo(x0, strike_y);
				LineTo(x1, strike_y);
				if (mono_eff_inv(run_attr))
					PenMode(patCopy);
			}

			if (use_color)
				color_dirty = 1;
		}
	}

	/* Restore default colors once at end of row */
	if (color_dirty) {
		set_fg_color(g_dark_mode ? 15 : 0);
		set_bg_color(g_dark_mode ? 0 : 15);
	}

	/* Restore normal face only if it was changed */
	if (last_face > 0)
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

			if (mono_eff_inv(attr))
				TextMode(srcBic);
			MoveTo(x, bl);
			DrawChar(ch);
			if (mono_eff_inv(attr))
				TextMode(srcOr);
		}
		break;
	}

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

	/* --- Fractional lower blocks --- */
	case GLYPH_BLOCK_LOWER_1:
		SetRect(&r, x, y + g_cell_height - g_cell_height / 8,
		    x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LOWER_2:
		SetRect(&r, x, y + g_cell_height - g_cell_height * 2 / 8,
		    x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LOWER_3:
		SetRect(&r, x, y + g_cell_height - g_cell_height * 3 / 8,
		    x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LOWER_5:
		SetRect(&r, x, y + g_cell_height - g_cell_height * 5 / 8,
		    x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LOWER_6:
		SetRect(&r, x, y + g_cell_height - g_cell_height * 6 / 8,
		    x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LOWER_7:
		SetRect(&r, x, y + g_cell_height - g_cell_height * 7 / 8,
		    x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;

	/* --- Fractional left blocks --- */
	case GLYPH_BLOCK_LEFT_7:
		SetRect(&r, x, y, x + g_cell_width * 7 / 8,
		    y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LEFT_6:
		SetRect(&r, x, y, x + g_cell_width * 6 / 8,
		    y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LEFT_5:
		SetRect(&r, x, y, x + g_cell_width * 5 / 8,
		    y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LEFT_3:
		SetRect(&r, x, y, x + g_cell_width * 3 / 8,
		    y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LEFT_2:
		SetRect(&r, x, y, x + g_cell_width * 2 / 8,
		    y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_LEFT_1:
		SetRect(&r, x, y, x + g_cell_width / 8,
		    y + g_cell_height);
		PaintRect(&r);
		break;

	/* --- Edge blocks --- */
	case GLYPH_BLOCK_UPPER_1:
		SetRect(&r, x, y, x + g_cell_width,
		    y + g_cell_height / 8);
		PaintRect(&r);
		break;
	case GLYPH_BLOCK_RIGHT_1:
		SetRect(&r, x + g_cell_width - g_cell_width / 8, y,
		    x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;

	/* --- Missing quadrants --- */
	case GLYPH_QUAD_UL_LL_LR:
		/* UL + LL + LR (missing UR) */
		SetRect(&r, x, y, cx, y + g_cell_height);
		PaintRect(&r);
		SetRect(&r, cx, cy, x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_QUAD_UL_LR:
		/* UL + LR (diagonal) */
		SetRect(&r, x, y, cx, cy);
		PaintRect(&r);
		SetRect(&r, cx, cy, x + g_cell_width, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_QUAD_UR_LL:
		/* UR + LL (diagonal) */
		SetRect(&r, cx, y, x + g_cell_width, cy);
		PaintRect(&r);
		SetRect(&r, x, cy, cx, y + g_cell_height);
		PaintRect(&r);
		break;
	case GLYPH_QUAD_UR_LL_LR:
		/* UR + LL + LR (missing UL) */
		SetRect(&r, cx, y, x + g_cell_width, cy);
		PaintRect(&r);
		SetRect(&r, x, cy, x + g_cell_width, y + g_cell_height);
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
		if (g_has_color_qd) {
			/* Solid blend: 25% fg + 75% bg */
			RGBColor frgb, brgb, blend;
			color_get_rgb(g_eff_fg, &frgb);
			color_get_rgb(g_eff_bg, &brgb);
			blend.red   = brgb.red   + (frgb.red   - brgb.red)   / 4;
			blend.green = brgb.green + (frgb.green - brgb.green) / 4;
			blend.blue  = brgb.blue  + (frgb.blue  - brgb.blue)  / 4;
			RGBForeColor(&blend);
			PaintRect(&cell_r);
			set_fg_color(g_eff_fg);
		} else {
			if (attr & ATTR_INVERSE)
				FillRect(&cell_r, &qd.dkGray);
			else
				FillRect(&cell_r, &qd.ltGray);
		}
		break;

	case GLYPH_SHADE_MEDIUM:
		/* Medium shade ~50% fill */
		if (g_has_color_qd) {
			/* Solid blend: 50% fg + 50% bg */
			RGBColor frgb, brgb, blend;
			color_get_rgb(g_eff_fg, &frgb);
			color_get_rgb(g_eff_bg, &brgb);
			blend.red   = brgb.red   + (frgb.red   - brgb.red)   / 2;
			blend.green = brgb.green + (frgb.green - brgb.green) / 2;
			blend.blue  = brgb.blue  + (frgb.blue  - brgb.blue)  / 2;
			RGBForeColor(&blend);
			PaintRect(&cell_r);
			set_fg_color(g_eff_fg);
		} else {
			FillRect(&cell_r, &qd.gray);
		}
		break;

	case GLYPH_SHADE_DARK:
		/* Dark shade ~75% fill */
		if (g_has_color_qd) {
			/* Solid blend: 75% fg + 25% bg */
			RGBColor frgb, brgb, blend;
			color_get_rgb(g_eff_fg, &frgb);
			color_get_rgb(g_eff_bg, &brgb);
			blend.red   = frgb.red   - (frgb.red   - brgb.red)   / 4;
			blend.green = frgb.green - (frgb.green - brgb.green) / 4;
			blend.blue  = frgb.blue  - (frgb.blue  - brgb.blue)  / 4;
			RGBForeColor(&blend);
			PaintRect(&cell_r);
			set_fg_color(g_eff_fg);
		} else {
			if (attr & ATTR_INVERSE)
				FillRect(&cell_r, &qd.ltGray);
			else
				FillRect(&cell_r, &qd.dkGray);
		}
		break;

	/* --- Outline triangles --- */
	case GLYPH_TRI_UP_EMPTY:
		PenSize(1, 1);
		MoveTo(cx, y + 2);
		LineTo(right - 1, bottom - 2);
		LineTo(x + 1, bottom - 2);
		LineTo(cx, y + 2);
		break;

	case GLYPH_TRI_RIGHT_EMPTY:
		PenSize(1, 1);
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, cy);
		LineTo(x + 1, bottom - 2);
		LineTo(x + 1, y + 2);
		break;

	case GLYPH_TRI_DOWN_EMPTY:
		PenSize(1, 1);
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, y + 2);
		LineTo(cx, bottom - 2);
		LineTo(x + 1, y + 2);
		break;

	case GLYPH_TRI_LEFT_EMPTY:
		PenSize(1, 1);
		MoveTo(right - 1, y + 2);
		LineTo(x + 1, cy);
		LineTo(right - 1, bottom - 2);
		LineTo(right - 1, y + 2);
		break;

	/* --- Small filled triangles --- */
	case GLYPH_TRI_RIGHT_SM:
		/* Small right triangle (inset ~25%) */
		MoveTo(x + w4, cy - h4);
		LineTo(right - w4, cy);
		LineTo(x + w4, cy + h4);
		LineTo(x + w4, cy - h4);
		break;

	case GLYPH_TRI_LEFT_SM:
		/* Small left triangle (inset ~25%) */
		MoveTo(right - w4, cy - h4);
		LineTo(x + w4, cy);
		LineTo(right - w4, cy + h4);
		LineTo(right - w4, cy - h4);
		break;

	/* --- Diamonds --- */
	case GLYPH_DIAMOND_FILLED:
		/* Filled diamond */
		{
			short dx = g_cell_width / 3;
			short dy = g_cell_height / 3;
			Rect dr;

			SetRect(&dr, cx - dx, cy - dy,
			    cx + dx + 1, cy + dy + 1);
			PaintOval(&dr);
		}
		break;

	case GLYPH_DIAMOND_EMPTY:
		/* Empty diamond */
		PenSize(1, 1);
		MoveTo(cx, y + 2);
		LineTo(right - 1, cy);
		LineTo(cx, bottom - 2);
		LineTo(x + 1, cy);
		LineTo(cx, y + 2);
		break;

	/* --- Circle variants --- */
	case GLYPH_CIRCLE_HALF_L:
		/* Left half black circle */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		SetRect(&r, x + 1, y + 2, cx, bottom - 1);
		PaintRect(&r);
		/* Clip to circle by redrawing outline */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		break;

	case GLYPH_CIRCLE_HALF_R:
		/* Right half black circle */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		SetRect(&r, cx, y + 2, right, bottom - 1);
		PaintRect(&r);
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		break;

	case GLYPH_CIRCLE_HALF_B:
		/* Bottom half black circle */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		SetRect(&r, x + 1, cy, right, bottom - 1);
		PaintRect(&r);
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		break;

	case GLYPH_CIRCLE_HALF_T:
		/* Top half black circle */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		SetRect(&r, x + 1, y + 2, right, cy);
		PaintRect(&r);
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		break;

	case GLYPH_CIRCLE_DOT:
		/* Circle with dot inside (fisheye) */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		SetRect(&r, cx - 1, cy - 1, cx + 2, cy + 2);
		PaintOval(&r);
		break;

	/* --- Six-pointed star --- */
	case GLYPH_STAR_SIX:
		/* Two overlapping triangles */
		PenSize(1, 1);
		/* Up triangle */
		MoveTo(cx, y + 1);
		LineTo(right - 1, bottom - 3);
		LineTo(x + 1, bottom - 3);
		LineTo(cx, y + 1);
		/* Down triangle */
		MoveTo(cx, bottom - 1);
		LineTo(right - 1, y + 3);
		LineTo(x + 1, y + 3);
		LineTo(cx, bottom - 1);
		break;

	/* --- Circled operators --- */
	case GLYPH_CIRCLED_DOT:
		/* Circle with centered dot */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		SetRect(&r, cx - 1, cy - 1, cx + 1, cy + 1);
		PaintOval(&r);
		break;

	case GLYPH_CIRCLED_PLUS:
		/* Circle with + inside */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		PenSize(1, 1);
		MoveTo(cx, y + 3);
		LineTo(cx, bottom - 2);
		MoveTo(x + 2, cy);
		LineTo(right - 1, cy);
		break;

	case GLYPH_CIRCLED_MINUS:
		/* Circle with - inside */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		PenSize(1, 1);
		MoveTo(x + 2, cy);
		LineTo(right - 1, cy);
		break;

	case GLYPH_CIRCLED_TIMES:
		/* Circle with X inside */
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		FrameOval(&r);
		PenSize(1, 1);
		MoveTo(x + 2, y + 3);
		LineTo(right - 2, bottom - 2);
		MoveTo(right - 2, y + 3);
		LineTo(x + 2, bottom - 2);
		break;

	/* --- Superscript digits --- */
	case GLYPH_SUPER_0: case GLYPH_SUPER_1:
	case GLYPH_SUPER_2: case GLYPH_SUPER_3:
	case GLYPH_SUPER_4: case GLYPH_SUPER_5:
	case GLYPH_SUPER_6: case GLYPH_SUPER_7:
	case GLYPH_SUPER_8: case GLYPH_SUPER_9:
		/* Draw digit at ~60% size in upper portion of cell */
		{
			char digit;
			short small_size;

			digit = '0' + (glyph_id - GLYPH_SUPER_0);
			small_size = g_font_size * 3 / 5;
			if (small_size < 6) small_size = 6;
			TextSize(small_size);
			if (mono_eff_inv(attr))
				TextMode(srcBic);
			MoveTo(x + 1, y + small_size);
			DrawChar(digit);
			if (mono_eff_inv(attr))
				TextMode(srcOr);
			TextSize(g_font_size);
		}
		break;

	/* --- Subscript digits --- */
	case GLYPH_SUB_0: case GLYPH_SUB_1:
	case GLYPH_SUB_2: case GLYPH_SUB_3:
	case GLYPH_SUB_4: case GLYPH_SUB_5:
	case GLYPH_SUB_6: case GLYPH_SUB_7:
	case GLYPH_SUB_8: case GLYPH_SUB_9:
		/* Draw digit at ~60% size in lower portion of cell */
		{
			char digit;
			short small_size;

			digit = '0' + (glyph_id - GLYPH_SUB_0);
			small_size = g_font_size * 3 / 5;
			if (small_size < 6) small_size = 6;
			TextSize(small_size);
			if (mono_eff_inv(attr))
				TextMode(srcBic);
			MoveTo(x + 1, bottom - 1);
			DrawChar(digit);
			if (mono_eff_inv(attr))
				TextMode(srcOr);
			TextSize(g_font_size);
		}
		break;

	/* ================================================================
	 * CP437 double-line box drawing
	 *
	 * Double lines drawn as two parallel lines offset ±1px from center.
	 * For mixed single/double: single through center, double offset ±1.
	 * ================================================================ */

	case GLYPH_BOX2_V:
		/* ║ double vertical */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, y + g_cell_height);
		break;

	case GLYPH_BOX2_H:
		/* ═ double horizontal */
		PenSize(1, 1);
		MoveTo(x, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		break;

	case GLYPH_BOX2_DR:
		/* ╔ double down+right */
		PenSize(1, 1);
		MoveTo(cx - 1, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(cx + 1, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		MoveTo(cx - 1, cy - 1);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, cy + 1);
		LineTo(cx + 1, y + g_cell_height);
		break;

	case GLYPH_BOX2_DL:
		/* ╗ double down+left */
		PenSize(1, 1);
		MoveTo(x, cy - 1);
		LineTo(cx + 1, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx - 1, cy + 1);
		MoveTo(cx + 1, cy - 1);
		LineTo(cx + 1, y + g_cell_height);
		MoveTo(cx - 1, cy + 1);
		LineTo(cx - 1, y + g_cell_height);
		break;

	case GLYPH_BOX2_UR:
		/* ╚ double up+right */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy + 1);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy - 1);
		MoveTo(cx + 1, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(cx - 1, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		break;

	case GLYPH_BOX2_UL:
		/* ╝ double up+left */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy + 1);
		MoveTo(x, cy - 1);
		LineTo(cx - 1, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx + 1, cy + 1);
		break;

	case GLYPH_BOX2_VR:
		/* ╠ double vert + double right */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy - 1);
		MoveTo(cx + 1, cy + 1);
		LineTo(cx + 1, y + g_cell_height);
		MoveTo(cx + 1, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(cx + 1, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		break;

	case GLYPH_BOX2_VL:
		/* ╣ double vert + double left */
		PenSize(1, 1);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, y + g_cell_height);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx - 1, cy + 1);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(x, cy - 1);
		LineTo(cx - 1, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx - 1, cy + 1);
		break;

	case GLYPH_BOX2_DH:
		/* ╦ double down + double horiz */
		PenSize(1, 1);
		MoveTo(x, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx - 1, cy + 1);
		MoveTo(cx + 1, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		MoveTo(cx - 1, cy + 1);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, cy + 1);
		LineTo(cx + 1, y + g_cell_height);
		break;

	case GLYPH_BOX2_UH:
		/* ╩ double up + double horiz */
		PenSize(1, 1);
		MoveTo(x, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		MoveTo(x, cy - 1);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx + 1, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy - 1);
		break;

	case GLYPH_BOX2_VH:
		/* ╬ double cross */
		PenSize(1, 1);
		/* Outer vertical lines with gaps */
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx - 1, cy + 1);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy - 1);
		MoveTo(cx + 1, cy + 1);
		LineTo(cx + 1, y + g_cell_height);
		/* Outer horizontal lines with gaps */
		MoveTo(x, cy - 1);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx + 1, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx - 1, cy + 1);
		MoveTo(cx + 1, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		break;

	/* --- Mixed single/double box junctions --- */

	case GLYPH_BOX_sVdL:
		/* ╡ single vert, double left */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, y + g_cell_height);
		MoveTo(x, cy - 1);
		LineTo(cx, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx, cy + 1);
		break;

	case GLYPH_BOX_dVsL:
		/* ╢ double vert, single left */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, y + g_cell_height);
		MoveTo(x, cy);
		LineTo(cx - 1, cy);
		break;

	case GLYPH_BOX_dDsL:
		/* ╖ double down, single left */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(cx + 1, cy);
		MoveTo(cx - 1, cy);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, cy);
		LineTo(cx + 1, y + g_cell_height);
		break;

	case GLYPH_BOX_sDdL:
		/* ╕ single down, double left */
		PenSize(1, 1);
		MoveTo(x, cy - 1);
		LineTo(cx, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx, cy + 1);
		MoveTo(cx, cy - 1);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_BOX_dUsL:
		/* ╜ double up, single left */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy);
		MoveTo(x, cy);
		LineTo(cx + 1, cy);
		break;

	case GLYPH_BOX_sUdL:
		/* ╛ single up, double left */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, cy + 1);
		MoveTo(x, cy - 1);
		LineTo(cx, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx, cy + 1);
		break;

	case GLYPH_BOX_sVdR:
		/* ╞ single vert, double right */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, y + g_cell_height);
		MoveTo(cx, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(cx, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		break;

	case GLYPH_BOX_dVsR:
		/* ╟ double vert, single right */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, y + g_cell_height);
		MoveTo(cx + 1, cy);
		LineTo(x + g_cell_width, cy);
		break;

	case GLYPH_BOX_dHsU:
		/* ╧ double horiz, single up */
		PenSize(1, 1);
		MoveTo(x, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		MoveTo(cx, y);
		LineTo(cx, cy - 1);
		break;

	case GLYPH_BOX_sHdU:
		/* ╨ single horiz, double up */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy);
		break;

	case GLYPH_BOX_dHsD:
		/* ╤ double horiz, single down */
		PenSize(1, 1);
		MoveTo(x, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		MoveTo(cx, cy + 1);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_BOX_sHdD:
		/* ╥ single horiz, double down */
		PenSize(1, 1);
		MoveTo(x, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx - 1, cy);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, cy);
		LineTo(cx + 1, y + g_cell_height);
		break;

	case GLYPH_BOX_dUsR:
		/* ╙ double up, single right */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, cy);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, cy);
		MoveTo(cx - 1, cy);
		LineTo(x + g_cell_width, cy);
		break;

	case GLYPH_BOX_sUdR:
		/* ╘ single up, double right */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, cy - 1);
		MoveTo(cx, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(cx, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		break;

	case GLYPH_BOX_sDdR:
		/* ╒ single down, double right */
		PenSize(1, 1);
		MoveTo(cx, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(cx, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		MoveTo(cx, cy + 1);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_BOX_dDsR:
		/* ╓ double down, single right */
		PenSize(1, 1);
		MoveTo(cx - 1, cy);
		LineTo(x + g_cell_width, cy);
		MoveTo(cx - 1, cy);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, cy);
		LineTo(cx + 1, y + g_cell_height);
		break;

	case GLYPH_BOX_dVsVH:
		/* ╫ double vert, single horiz cross */
		PenSize(1, 1);
		MoveTo(cx - 1, y);
		LineTo(cx - 1, y + g_cell_height);
		MoveTo(cx + 1, y);
		LineTo(cx + 1, y + g_cell_height);
		MoveTo(x, cy);
		LineTo(cx - 1, cy);
		MoveTo(cx + 1, cy);
		LineTo(x + g_cell_width, cy);
		break;

	case GLYPH_BOX_sVdVH:
		/* ╪ single vert, double horiz cross */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, y + g_cell_height);
		MoveTo(x, cy - 1);
		LineTo(cx, cy - 1);
		MoveTo(cx, cy - 1);
		LineTo(x + g_cell_width, cy - 1);
		MoveTo(x, cy + 1);
		LineTo(cx, cy + 1);
		MoveTo(cx, cy + 1);
		LineTo(x + g_cell_width, cy + 1);
		break;

	/* ================================================================
	 * CP437 symbol glyphs
	 * ================================================================ */

	case GLYPH_SMILEY:
		/* ☺ white smiley face */
		PenSize(1, 1);
		SetRect(&r, x + 1, y + 1, right, bottom - 1);
		FrameOval(&r);
		/* Eyes */
		MoveTo(cx - 1, cy - 1);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx + 1, cy - 1);
		LineTo(cx + 1, cy - 1);
		/* Mouth: small arc approximation */
		MoveTo(cx - 1, cy + 1);
		LineTo(cx - 1, cy + 1);
		MoveTo(cx, cy + 2);
		LineTo(cx, cy + 2);
		MoveTo(cx + 1, cy + 1);
		LineTo(cx + 1, cy + 1);
		break;

	case GLYPH_SMILEY_INV:
		/* ☻ black smiley face (filled circle, white features) */
		SetRect(&r, x + 1, y + 1, right, bottom - 1);
		PaintOval(&r);
		/* Features: opposite of body (white on black, or
		 * black on white when cell is effective-inverse) */
		PenMode(mono_eff_inv(attr) ? patCopy : patBic);
		/* Eyes */
		MoveTo(cx - 1, cy - 1);
		LineTo(cx - 1, cy - 1);
		MoveTo(cx + 1, cy - 1);
		LineTo(cx + 1, cy - 1);
		/* Mouth */
		MoveTo(cx - 1, cy + 1);
		LineTo(cx - 1, cy + 1);
		MoveTo(cx, cy + 2);
		LineTo(cx, cy + 2);
		MoveTo(cx + 1, cy + 1);
		LineTo(cx + 1, cy + 1);
		PenMode(patCopy);
		break;

	case GLYPH_INV_BULLET:
		/* ◘ inverse bullet: filled rect + white circle */
		PaintRect(&cell_r);
		PenMode(mono_eff_inv(attr) ? patCopy : patBic);
		SetRect(&r, x + 1, y + 2, right, bottom - 1);
		PaintOval(&r);
		PenMode(patCopy);
		break;

	case GLYPH_INV_CIRCLE:
		/* ◙ inverse circle: filled circle + white circle inside */
		SetRect(&r, x + 1, y + 1, right, bottom - 1);
		PaintOval(&r);
		PenMode(mono_eff_inv(attr) ? patCopy : patBic);
		SetRect(&r, x + 2, y + 3, right - 1, bottom - 2);
		PaintOval(&r);
		PenMode(patCopy);
		break;

	case GLYPH_MALE:
		/* ♂ male sign: circle + arrow up-right */
		PenSize(1, 1);
		SetRect(&r, x + 1, cy - 1, cx + 1, bottom - 1);
		FrameOval(&r);
		MoveTo(cx, cy - 2);
		LineTo(right - 1, y + 1);
		MoveTo(right - 1, y + 1);
		LineTo(right - 3, y + 1);
		MoveTo(right - 1, y + 1);
		LineTo(right - 1, y + 3);
		break;

	case GLYPH_FEMALE:
		/* ♀ female sign: circle + cross below */
		PenSize(1, 1);
		SetRect(&r, x + 1, y + 1, right - 1, cy + 1);
		FrameOval(&r);
		MoveTo(cx, cy + 1);
		LineTo(cx, bottom - 1);
		MoveTo(cx - 1, cy + 3);
		LineTo(cx + 1, cy + 3);
		break;

	case GLYPH_SUN:
		/* ☼ sun: circle + radiating lines */
		PenSize(1, 1);
		SetRect(&r, cx - 2, cy - 2, cx + 3, cy + 3);
		FrameOval(&r);
		/* Rays */
		MoveTo(cx, y + 1);
		LineTo(cx, cy - 3);
		MoveTo(cx, cy + 3);
		LineTo(cx, bottom - 1);
		MoveTo(x + 1, cy);
		LineTo(cx - 3, cy);
		MoveTo(cx + 3, cy);
		LineTo(right - 1, cy);
		break;

	case GLYPH_ARROW_UPDOWN:
		/* ↕ up-down arrow */
		PenSize(1, 1);
		MoveTo(cx, y + 1);
		LineTo(cx, bottom - 1);
		/* Up head */
		MoveTo(cx, y + 1);
		LineTo(cx - w4, y + h4 + 1);
		MoveTo(cx, y + 1);
		LineTo(cx + w4, y + h4 + 1);
		/* Down head */
		MoveTo(cx, bottom - 1);
		LineTo(cx - w4, bottom - h4 - 1);
		MoveTo(cx, bottom - 1);
		LineTo(cx + w4, bottom - h4 - 1);
		break;

	case GLYPH_BAR_H:
		/* ▬ horizontal bar (half-height rectangle) */
		SetRect(&r, x + 1, cy - 1, right, cy + 2);
		PaintRect(&r);
		break;

	case GLYPH_ARROW_UPDOWN_BASE:
		/* ↨ up-down arrow with base line */
		PenSize(1, 1);
		MoveTo(cx, y + 1);
		LineTo(cx, bottom - 2);
		/* Up head */
		MoveTo(cx, y + 1);
		LineTo(cx - w4, y + h4 + 1);
		MoveTo(cx, y + 1);
		LineTo(cx + w4, y + h4 + 1);
		/* Down head */
		MoveTo(cx, bottom - 2);
		LineTo(cx - w4, bottom - h4 - 2);
		MoveTo(cx, bottom - 2);
		LineTo(cx + w4, bottom - h4 - 2);
		/* Base line */
		MoveTo(x + 1, bottom - 1);
		LineTo(right - 1, bottom - 1);
		break;

	case GLYPH_RIGHT_ANGLE:
		/* ∟ right angle */
		PenSize(1, 1);
		MoveTo(x + 1, y + 2);
		LineTo(x + 1, bottom - 1);
		LineTo(right - 1, bottom - 1);
		break;

	case GLYPH_ARROW_LEFTRIGHT:
		/* ↔ left-right arrow */
		PenSize(1, 1);
		MoveTo(x + 1, cy);
		LineTo(right - 1, cy);
		/* Left head */
		MoveTo(x + 1, cy);
		LineTo(x + w4 + 1, cy - h4);
		MoveTo(x + 1, cy);
		LineTo(x + w4 + 1, cy + h4);
		/* Right head */
		MoveTo(right - 1, cy);
		LineTo(right - w4 - 1, cy - h4);
		MoveTo(right - 1, cy);
		LineTo(right - w4 - 1, cy + h4);
		break;

	case GLYPH_HOUSE:
		/* ⌂ house: triangle roof + rectangle body */
		PenSize(1, 1);
		/* Roof */
		MoveTo(cx, y + 1);
		LineTo(right - 1, cy);
		LineTo(x + 1, cy);
		LineTo(cx, y + 1);
		/* Body */
		SetRect(&r, x + 2, cy, right - 1, bottom - 1);
		FrameRect(&r);
		break;

	/* ================================================================
	 * CP437 math/Greek glyphs
	 * ================================================================ */

	case GLYPH_REVERSED_NOT:
		/* ⌐ reversed not sign: horizontal + descender on left */
		PenSize(1, 1);
		MoveTo(x + 1, cy);
		LineTo(right - 1, cy);
		MoveTo(x + 1, cy);
		LineTo(x + 1, cy + h4 + 1);
		break;

	case GLYPH_HALF:
		/* ½ fraction one-half */
		{
			short small_size;
			small_size = g_font_size * 3 / 5;
			if (small_size < 6) small_size = 6;
			TextSize(small_size);
			if (mono_eff_inv(attr))
				TextMode(srcBic);
			MoveTo(x, y + small_size);
			DrawChar('1');
			PenSize(1, 1);
			MoveTo(x + 1, bottom - 2);
			LineTo(right - 1, y + 2);
			MoveTo(cx, bottom - 1);
			DrawChar('2');
			if (mono_eff_inv(attr))
				TextMode(srcOr);
			TextSize(g_font_size);
		}
		break;

	case GLYPH_QUARTER:
		/* ¼ fraction one-quarter */
		{
			short small_size;
			small_size = g_font_size * 3 / 5;
			if (small_size < 6) small_size = 6;
			TextSize(small_size);
			if (mono_eff_inv(attr))
				TextMode(srcBic);
			MoveTo(x, y + small_size);
			DrawChar('1');
			PenSize(1, 1);
			MoveTo(x + 1, bottom - 2);
			LineTo(right - 1, y + 2);
			MoveTo(cx, bottom - 1);
			DrawChar('4');
			if (mono_eff_inv(attr))
				TextMode(srcOr);
			TextSize(g_font_size);
		}
		break;

	case GLYPH_GAMMA:
		/* Γ Greek capital gamma: L-shape (top + left) */
		PenSize(1, 1);
		MoveTo(x + 1, y + 2);
		LineTo(right - 1, y + 2);
		MoveTo(x + 1, y + 2);
		LineTo(x + 1, bottom - 1);
		break;

	case GLYPH_PHI_UC:
		/* Φ Greek capital phi: circle + vertical line */
		PenSize(1, 1);
		SetRect(&r, x + 1, cy - 2, right - 1, cy + 3);
		FrameOval(&r);
		MoveTo(cx, y + 1);
		LineTo(cx, bottom - 1);
		break;

	case GLYPH_THETA:
		/* Θ Greek capital theta: circle + horizontal bar */
		PenSize(1, 1);
		SetRect(&r, x + 1, y + 1, right - 1, bottom - 1);
		FrameOval(&r);
		MoveTo(x + 2, cy);
		LineTo(right - 2, cy);
		break;

	case GLYPH_DELTA_LC:
		/* δ Greek lowercase delta: rounded shape */
		PenSize(1, 1);
		SetRect(&r, x + 1, cy - 1, right - 1, bottom - 1);
		FrameOval(&r);
		MoveTo(right - 2, cy - 1);
		LineTo(cx, y + 1);
		break;

	case GLYPH_INFINITY:
		/* ∞ infinity: figure-eight on its side */
		PenSize(1, 1);
		SetRect(&r, x + 1, cy - 2, cx, cy + 3);
		FrameOval(&r);
		SetRect(&r, cx, cy - 2, right - 1, cy + 3);
		FrameOval(&r);
		break;

	case GLYPH_INTERSECT:
		/* ∩ intersection: inverted U shape */
		PenSize(1, 1);
		SetRect(&r, x + 1, y + 2, right - 1, bottom + 2);
		FrameOval(&r);
		/* Erase bottom half (fill with cell background) */
		SetRect(&r, x + 2, cy, right - 2, bottom);
		FillRect(&r, mono_eff_inv(attr) ? &qd.black : &qd.white);
		/* Vertical sides */
		MoveTo(x + 1, cy);
		LineTo(x + 1, bottom - 1);
		MoveTo(right - 2, cy);
		LineTo(right - 2, bottom - 1);
		break;

	case GLYPH_IDENTICAL:
		/* ≡ identical: three horizontal lines */
		PenSize(1, 1);
		MoveTo(x + 1, cy - 2);
		LineTo(right - 1, cy - 2);
		MoveTo(x + 1, cy);
		LineTo(right - 1, cy);
		MoveTo(x + 1, cy + 2);
		LineTo(right - 1, cy + 2);
		break;

	case GLYPH_INTEGRAL_T:
		/* ⌠ top half of integral */
		PenSize(1, 1);
		MoveTo(cx + 1, y + 1);
		LineTo(cx, y + 2);
		LineTo(cx, y + g_cell_height);
		break;

	case GLYPH_INTEGRAL_B:
		/* ⌡ bottom half of integral */
		PenSize(1, 1);
		MoveTo(cx, y);
		LineTo(cx, bottom - 2);
		LineTo(cx - 1, bottom - 1);
		break;

	case GLYPH_APPROX:
		/* ≈ approximately equal: two tilde curves */
		PenSize(1, 1);
		/* Upper tilde */
		MoveTo(x + 1, cy - 1);
		LineTo(cx - 1, cy - 2);
		LineTo(cx + 1, cy - 1);
		LineTo(right - 1, cy - 2);
		/* Lower tilde */
		MoveTo(x + 1, cy + 2);
		LineTo(cx - 1, cy + 1);
		LineTo(cx + 1, cy + 2);
		LineTo(right - 1, cy + 1);
		break;

	case GLYPH_SQRT:
		/* √ square root: radical sign */
		PenSize(1, 1);
		MoveTo(x + 1, cy);
		LineTo(x + 2, cy);
		LineTo(cx - 1, bottom - 1);
		LineTo(right - 1, y + 1);
		break;

	case GLYPH_SUPER_N:
		/* ⁿ superscript n */
		{
			short small_size;
			small_size = g_font_size * 3 / 5;
			if (small_size < 6) small_size = 6;
			TextSize(small_size);
			if (mono_eff_inv(attr))
				TextMode(srcBic);
			MoveTo(x + 1, y + small_size);
			DrawChar('n');
			if (mono_eff_inv(attr))
				TextMode(srcOr);
			TextSize(g_font_size);
		}
		break;

	case GLYPH_FLOWER:
		/* ✿ Flower: petals around center dot */
		PenSize(1, 1);
		/* Center dot */
		SetRect(&r, cx - 1, cy - 1, cx + 2, cy + 2);
		PaintOval(&r);
		/* Top petal */
		SetRect(&r, cx - 1, y + 1, cx + 2, cy - 1);
		PaintOval(&r);
		/* Bottom petal */
		SetRect(&r, cx - 1, cy + 2, cx + 2, bottom);
		PaintOval(&r);
		/* Left petal */
		SetRect(&r, x, cy - 1, cx - 1, cy + 2);
		PaintOval(&r);
		/* Right petal */
		SetRect(&r, cx + 2, cy - 1, right + 1, cy + 2);
		PaintOval(&r);
		break;

	case GLYPH_SNOWFLAKE:
		/* ❄ Snowflake: 6 arms with small cross-branches */
		PenSize(1, 1);
		/* Vertical arm */
		MoveTo(cx, y + 1);
		LineTo(cx, bottom - 1);
		/* Diagonal arms */
		MoveTo(x + 1, cy - h4);
		LineTo(right - 1, cy + h4);
		MoveTo(x + 1, cy + h4);
		LineTo(right - 1, cy - h4);
		/* Cross-branches on vertical arm */
		MoveTo(cx - 1, y + 3);
		LineTo(cx + 1, y + 3);
		MoveTo(cx - 1, bottom - 3);
		LineTo(cx + 1, bottom - 3);
		/* Cross-branches on diagonal arms */
		MoveTo(x + 1, cy);
		LineTo(x + 2, cy - 1);
		MoveTo(right - 1, cy);
		LineTo(right - 2, cy + 1);
		break;

	default:
		if (glyph_id >= GLYPH_SEXTANT_BASE &&
		    glyph_id < GLYPH_SEXTANT_BASE + GLYPH_SEXTANT_COUNT) {
			/* Sextant: 2x3 grid of filled sub-cells */
			unsigned char idx, pat;
			short cw, rh, rx, ry1, ry2;

			idx = glyph_id - GLYPH_SEXTANT_BASE;
			/* Unpack stored index (0-59) to 6-bit pattern */
			/* Skipped patterns: 0 (empty), 21 (left half),
			 * 42 (right half), 63 (full block) */
			if (idx < 20)
				pat = idx + 1;
			else if (idx < 40)
				pat = idx + 2;
			else
				pat = idx + 3;

			cw = g_cell_width / 2;
			rh = g_cell_height / 3;
			rx = x + cw;
			ry1 = y + rh;
			ry2 = y + rh + rh;

			if (pat & 0x01) {
				SetRect(&r, x, y, rx, ry1);
				PaintRect(&r);
			}
			if (pat & 0x02) {
				SetRect(&r, rx, y, x + g_cell_width, ry1);
				PaintRect(&r);
			}
			if (pat & 0x04) {
				SetRect(&r, x, ry1, rx, ry2);
				PaintRect(&r);
			}
			if (pat & 0x08) {
				SetRect(&r, rx, ry1, x + g_cell_width, ry2);
				PaintRect(&r);
			}
			if (pat & 0x10) {
				SetRect(&r, x, ry2, rx, y + g_cell_height);
				PaintRect(&r);
			}
			if (pat & 0x20) {
				SetRect(&r, rx, ry2,
				    x + g_cell_width, y + g_cell_height);
				PaintRect(&r);
			}
		} else {
			/* Unknown primitive: draw ? */
			short bl = y + g_cell_baseline;

			if (mono_eff_inv(attr))
				TextMode(srcBic);
			MoveTo(x, bl);
			DrawChar('?');
			if (mono_eff_inv(attr))
				TextMode(srcOr);
		}
		break;
	}

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
	Rect src_r, dst_r;
	short dx, dy, cell2_w;

	bm = glyph_get_bitmap(glyph_id);
	if (!bm) {
		/* No bitmap: draw ? as fallback */
		draw_glyph_prim(glyph_id, x, y, attr);
		return;
	}

	/* 2-cell wide area */
	cell2_w = g_cell_width * 2;

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

	/* Blit: srcOr for normal, srcBic for inverse on monochrome */
	CopyBits(&src_bits, &qd.thePort->portBits,
	    &src_r, &dst_r,
	    mono_eff_inv(attr) ? srcBic : srcOr,
	    NULL);
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
	Rect dot_r;
	short dot_w, dot_h, gap_x, gap_y;
	short dx, dy;
	short col_idx, row_idx;
	/* Bit positions: [row][col] */
	static const unsigned char bit_pos[4][2] = {
		{ 0, 3 }, { 1, 4 }, { 2, 5 }, { 6, 7 }
	};

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

}

/*
 * cursor_set_rect - compute cursor rectangle based on DECSCUSR style
 */
static void
cursor_set_rect(Rect *r, short crow, short ccol, unsigned char style)
{
	if (style == 3 || style == 4) {
		/* Underline cursor: bottom 2 pixels */
		SetRect(r,
		    col_left(ccol), row_bottom(crow) - 2,
		    col_right(ccol), row_bottom(crow));
	} else if (style == 5 || style == 6) {
		/* Bar cursor: left 2 pixels */
		SetRect(r,
		    col_left(ccol), row_top(crow),
		    col_left(ccol) + 2, row_bottom(crow));
	} else {
		/* Block cursor (0, 1, 2) */
		SetRect(r,
		    col_left(ccol), row_top(crow),
		    col_right(ccol), row_bottom(crow));
	}
}

/*
 * draw_cursor - draw or erase the cursor at current position
 *
 * Uses XOR (patXor) so drawing twice erases it, giving us blink.
 * Cursor shape depends on DECSCUSR style (block/underline/bar).
 */
static void
draw_cursor(Terminal *term, short on)
{
	short crow, ccol;
	Rect cur_r;

	terminal_get_cursor(term, &crow, &ccol);

	if (on) {
		cursor_set_rect(&cur_r, crow, ccol,
		    term->cursor_style);
		PenMode(patXor);
		PaintRect(&cur_r);
		PenNormal();

		cursor_visible = 1;
		cursor_prev_row = crow;
		cursor_prev_col = ccol;
	}
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
	unsigned char style;

	if (!cursor_initialized || !term->cursor_visible || sel.active)
		return;

	/* Steady cursor styles don't blink */
	style = term->cursor_style;
	if (style == 2 || style == 4 || style == 6)
		return;

	now = TickCount();
	if (now - cursor_last_tick < CURSOR_BLINK_TICKS)
		return;

	cursor_last_tick = now;

	terminal_get_cursor(term, &crow, &ccol);

	GetPort(&old_port);
	SetPort(win);

	/* XOR the cursor rect to toggle it */
	cursor_set_rect(&cur_r, crow, ccol, style);
	PenMode(patXor);
	PaintRect(&cur_r);
	PenNormal();

	cursor_visible = !cursor_visible;

	/* Track where we drew so term_ui_draw can erase it.
	 * Without this, the erase targets the old cursor_prev
	 * position while the actual XOR mark is here. */
	if (cursor_visible) {
		cursor_prev_row = crow;
		cursor_prev_col = ccol;
	}

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
	    (when - sel.last_click_ticks) <= (unsigned long)LMGetDoubleTime() &&
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
