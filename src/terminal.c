/*
 * terminal.c - VT100 terminal emulation engine
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

#include <string.h>
#include <stdio.h>
#include <Memory.h>
#include <Multiverse.h>
#include "terminal.h"
#include "color.h"
#include "glyphs.h"
#include "cp437.h"

/* Forward declarations for internal functions */
static void term_clear_region(Terminal *term, short r1, short c1,
    short r2, short c2);
static void term_scroll_up(Terminal *term, short top, short bottom,
    short count);
static void term_scroll_down(Terminal *term, short top, short bottom,
    short count);
static void term_put_char(Terminal *term, unsigned char ch);
static void term_put_glyph(Terminal *term, unsigned char glyph_id,
    unsigned char attr_flag);
static void term_newline(Terminal *term);
static void term_carriage_return(Terminal *term);
static void term_process_esc(Terminal *term, unsigned char ch);
static void term_process_csi(Terminal *term, unsigned char ch);
static void term_execute_csi(Terminal *term, unsigned char cmd);
static void term_dec_set_mode(Terminal *term);
static void term_dec_reset_mode(Terminal *term);
static void term_set_attr(Terminal *term, short param);
static void term_put_cp437(Terminal *term, unsigned char byte);
static void term_process_osc(Terminal *term, unsigned char ch);
static void term_finish_osc(Terminal *term);
static void term_switch_to_alt(Terminal *term);
static void term_switch_to_main(Terminal *term);
static void term_dirty_row(Terminal *term, short row);
static void term_dirty_range(Terminal *term, short r1, short r2);
static short term_clamp(short val, short lo, short hi);
static long utf8_decode(unsigned char *buf, short len);
static void term_put_unicode(Terminal *term, long cp);
static unsigned char boxdraw_to_dec(unsigned short cp);
static unsigned char unicode_to_macroman(unsigned short cp);
static unsigned char unicode_symbol_to_macroman(unsigned short cp);

#include <OSUtils.h>

/*
 * terminal_init - set up terminal to power-on defaults
 */
void
terminal_init(Terminal *term)
{
	short saved_cols = term->active_cols;
	short saved_rows = term->active_rows;

	/* Zero parser state and cursor fields (small, ~200 bytes) */
	term->cur_row = 0;
	term->cur_col = 0;
	term->cur_attr = ATTR_NORMAL;
	term->parse_state = PARSE_NORMAL;
	term->num_params = 0;
	term->intermediate = 0;
	memset(term->params, 0, sizeof(term->params));
	term->cursor_visible = 1;
	term->sb_head = 0;
	term->sb_count = 0;
	term->scroll_offset = 0;
	term->response_len = 0;
	term->osc_len = 0;
	term->osc_param = 0;
	memset(term->osc_buf, 0, sizeof(term->osc_buf));
	memset(term->response, 0, sizeof(term->response));
	term->title_changed = 0;
	term->window_title[0] = '\0';
	term->utf8_len = 0;
	term->utf8_expect = 0;
	term->last_was_emoji = 0;
	term->wrap_pending = 0;

	/* Restore active dimensions (default if not set) */
	term->active_cols = (saved_cols > 0 && saved_cols <= TERM_COLS) ?
	    saved_cols : TERM_DEFAULT_COLS;
	term->active_rows = (saved_rows > 0 && saved_rows <= TERM_ROWS) ?
	    saved_rows : TERM_DEFAULT_ROWS;

	term->scroll_top = 0;
	term->scroll_bottom = term->active_rows - 1;

	/* Character sets */
	term->g0_charset = 'B';
	term->g1_charset = 'B';
	term->gl_charset = 0;

	/* DEC modes */
	term->autowrap = 1;
	term->cursor_key_mode = 0;
	term->keypad_mode = 0;
	term->origin_mode = 0;
	term->insert_mode = 0;
	term->bracketed_paste = 0;

	/* Alternate screen */
	term->alt_active = 0;
	term->alt_cur_row = 0;
	term->alt_cur_col = 0;
	term->alt_cur_attr = ATTR_NORMAL;

	/* Saved cursor */
	term->saved_row = 0;
	term->saved_col = 0;
	term->saved_attr = ATTR_NORMAL;
	term->saved_fg = COLOR_DEFAULT;
	term->saved_bg = COLOR_DEFAULT;
	term->saved_g0_charset = 'B';
	term->saved_g1_charset = 'B';
	term->saved_gl_charset = 0;
	term->saved_origin_mode = 0;
	term->saved_autowrap = 1;

	/* Color state */
	term->cur_fg = COLOR_DEFAULT;
	term->cur_bg = COLOR_DEFAULT;
	term->pre_dim_fg = COLOR_DEFAULT;
	term->alt_cur_fg = COLOR_DEFAULT;
	term->alt_cur_bg = COLOR_DEFAULT;

	/* Allocate screen color array on System 7 (first init only).
	 * alt_color and sb_color are lazy-allocated on first use
	 * to save ~38.5KB per session until needed. */
	if (g_has_color_qd && !term->has_color) {
		long sc_size = (long)TERM_ROWS * TERM_COLS * sizeof(CellColor);

		term->screen_color = (CellColor *)NewPtr(sc_size);

		if (term->screen_color) {
			term->has_color = 1;
		} else {
			term->screen_color = 0L;
			term->has_color = 0;
		}
		/* alt_color and sb_color start NULL, allocated on demand */
	}

	/* Fill screen color array with defaults */
	if (term->has_color) {
		memset(term->screen_color, COLOR_DEFAULT,
		    (long)TERM_ROWS * TERM_COLS * sizeof(CellColor));
	}

	term->cursor_style = 0;

	/* Tab stops: default every 8 columns */
	memset(term->tab_stops, 0, sizeof(term->tab_stops));
	{
		short c;
		for (c = 0; c < TERM_COLS; c += 8)
			term->tab_stops[c] = 1;
	}

	/* Line attributes */
	memset(term->line_attr, 0, sizeof(term->line_attr));

	/* Clear dirty flags then mark active rows dirty */
	memset(term->dirty, 0, sizeof(term->dirty));

	/* Clear only the active screen area (not full 132x50 buffer) */
	term_clear_region(term, 0, 0,
	    term->active_rows - 1, term->active_cols - 1);
	term_dirty_all(term);
}

/*
 * terminal_reset - reset terminal to initial state
 */
void
terminal_reset(Terminal *term)
{
	terminal_init(term);
}

/*
 * terminal_process - feed a buffer of bytes into the terminal engine
 */
void
terminal_process(Terminal *term, unsigned char *data, short len)
{
	short i;
	unsigned char ch;

	for (i = 0; i < len; i++) {
		ch = data[i];

		switch (term->parse_state) {
		case PARSE_NORMAL:
			/* CP437/ANSI-BBS mode: raw byte interpretation */
			if (term->cp437_mode) {
				if (ch == 0x1B) {
					term->parse_state = PARSE_ESC;
				} else if (ch == '\r') {
					term_carriage_return(term);
				} else if (ch == '\n') {
					term_newline(term);
				} else if (ch == '\b') {
					if (term->cur_col > 0) {
						term->cur_col--;
						term->wrap_pending = 0;
					}
				} else if (ch == '\t') {
					term->wrap_pending = 0;
					{
						short c;
						c = term->cur_col + 1;
						while (c < term->active_cols &&
						    !term->tab_stops[c])
							c++;
						if (c >= term->active_cols)
							c = term->active_cols - 1;
						term->cur_col = c;
					}
				} else if (ch == 0x07) {
					SysBeep(5);
				} else {
					term_put_cp437(term, ch);
				}
				break;
			}

			/*
			 * Fast path first: printable ASCII (0x20-0x7E)
			 * is by far the most common case in terminal
			 * data streams.  Testing it first avoids ~15
			 * comparisons per character on the 68000.
			 */
			if (ch >= 0x20 && ch < 0x7F) {
				/* Flush incomplete UTF-8 as CP437 */
				if (term->utf8_expect > 0) {
					short j;
					for (j = 0; j < term->utf8_len; j++)
						term_put_cp437(term,
						    term->utf8_buf[j]);
					term->utf8_expect = 0;
				}
				/* Inline fast path: ASCII, no wrap, standard charset */
				if (!term->wrap_pending &&
				    term->gl_charset == 0 &&
				    term->g0_charset == 'B') {
					unsigned char a = term->cur_attr;
					if (term->cur_fg != COLOR_DEFAULT ||
					    term->cur_bg != COLOR_DEFAULT)
						a |= ATTR_HAS_COLOR;
					term->screen[term->cur_row][term->cur_col].ch = ch;
					term->screen[term->cur_row][term->cur_col].attr = a;
					if (term->has_color &&
					    term->screen_color) {
						CellColor *cc =
						    &term->screen_color[
						    term->cur_row *
						    TERM_COLS +
						    term->cur_col];
						cc->fg = term->cur_fg;
						cc->bg = term->cur_bg;
					}
					term->dirty[term->cur_row] = 1;
					if (term->cur_col < term->active_cols - 1)
						term->cur_col++;
					else
						term->wrap_pending = 1;
				} else {
					term_put_char(term, ch);
				}
			} else if (ch == 0x1B) {
				/* ESC */
				if (term->utf8_expect > 0) {
					/* Flush incomplete UTF-8 as CP437 */
					short j;
					for (j = 0; j < term->utf8_len; j++)
						term_put_cp437(term,
						    term->utf8_buf[j]);
					term->utf8_expect = 0;
				}
				term->parse_state = PARSE_ESC;
			} else if (ch < 0x20) {
				/* Flush incomplete UTF-8 as CP437 */
				if (term->utf8_expect > 0) {
					short j;
					for (j = 0; j < term->utf8_len; j++)
						term_put_cp437(term,
						    term->utf8_buf[j]);
					term->utf8_expect = 0;
				}
				/* C0 control characters */
				switch (ch) {
				case '\r':
					term_carriage_return(term);
					break;
				case '\n':
				case 0x0B:
				case 0x0C:
					/* LF, VT, FF all treated as newline */
					term_newline(term);
					break;
				case '\b':
					/* Backspace: move cursor left one */
					if (term->cur_col > 0) {
						term->cur_col--;
						term->wrap_pending = 0;
					}
					break;
				case '\t':
					/* Tab: advance to next set tab stop */
					term->wrap_pending = 0;
					{
						short c;
						c = term->cur_col + 1;
						while (c < term->active_cols &&
						    !term->tab_stops[c])
							c++;
						if (c >= term->active_cols)
							c = term->active_cols - 1;
						term->cur_col = c;
					}
					break;
				case 0x07:
					/* Bell */
					SysBeep(5);
					break;
				case 0x0E:
					/* SO - Shift Out: activate G1 */
					term->gl_charset = 1;
					break;
				case 0x0F:
					/* SI - Shift In: activate G0 */
					term->gl_charset = 0;
					break;
				default:
					/* Other C0 codes (NUL, etc): ignore */
					break;
				}
			} else if (ch >= 0x80) {
				/* High bytes: UTF-8 continuation, C1 controls, UTF-8 start */
				if (ch < 0xC0 && term->utf8_expect > 0) {
					/* UTF-8 continuation byte (priority over C1) */
					term->utf8_buf[term->utf8_len++] = ch;
					if (term->utf8_len == term->utf8_expect) {
						long cp = utf8_decode(term->utf8_buf,
						    term->utf8_expect);
						/* CP437 fallback: 2-byte UTF-8 decoding
						 * to U+0100-U+07FF with no glyph is
						 * likely CP437 bytes misinterpreted.
						 * Handles servers sending raw CP437
						 * despite xterm-256color TTYPE. */
						if (term->utf8_expect == 2 &&
						    cp > 0xFF && cp < 0x800 &&
						    glyph_lookup(cp) < 0) {
							term_put_cp437(term,
							    term->utf8_buf[0]);
							term_put_cp437(term,
							    term->utf8_buf[1]);
						} else {
							term_put_unicode(term, cp);
						}
						term->utf8_expect = 0;
					}
				} else if (ch == 0x84) {
					/* IND (8-bit C1) - same as ESC D */
					term_newline(term);
				} else if (ch == 0x85) {
					/* NEL (8-bit C1) - same as ESC E */
					term_carriage_return(term);
					term_newline(term);
				} else if (ch == 0x8D) {
					/* RI (8-bit C1) - same as ESC M */
					if (term->cur_row == term->scroll_top)
						term_scroll_down(term,
						    term->scroll_top,
						    term->scroll_bottom, 1);
					else if (term->cur_row > 0)
						term->cur_row--;
					term_dirty_row(term, term->cur_row);
				} else if (ch == 0x90) {
					/* DCS (8-bit C1) - same as ESC P */
					term->parse_state = PARSE_DCS;
				} else if (ch == 0x9B) {
					/* CSI (8-bit C1) - same as ESC [ */
					term->parse_state = PARSE_CSI;
					term->num_params = 0;
					term->intermediate = 0;
					memset(term->params, 0,
					    sizeof(term->params));
				} else if (ch == 0x9D) {
					/* OSC (8-bit C1) - same as ESC ] */
					term->parse_state = PARSE_OSC;
					term->osc_len = 0;
					term->osc_param = 0;
				} else if (ch == 0x9C) {
					/* ST (8-bit C1) - string terminator */
				} else if (ch >= 0x80 && ch <= 0x9F) {
					/* Other C1 codes: ignore */
				} else if (ch >= 0xC0) {
					/* UTF-8 start byte: begin new sequence.
					 * If mid-sequence, flush incomplete
					 * bytes as CP437 (handles servers that
					 * send raw CP437 with xterm TTYPE). */
					if (term->utf8_expect > 0) {
						short j;
						for (j = 0; j < term->utf8_len;
						    j++)
							term_put_cp437(term,
							    term->utf8_buf[j]);
					}
					term->utf8_expect = 0;
					if (ch < 0xE0) {
						term->utf8_expect = 2;
					} else if (ch < 0xF0) {
						term->utf8_expect = 3;
					} else if (ch < 0xF8) {
						term->utf8_expect = 4;
					}
					if (term->utf8_expect) {
						term->utf8_buf[0] = ch;
						term->utf8_len = 1;
					} else {
						/* 0xF8-0xFF: invalid UTF-8
						 * start byte, treat as CP437
						 * (e.g. 0xFE = filled square)
						 */
						term_put_cp437(term, ch);
					}
				} else if (ch >= 0xA0) {
					/* Stray continuation: likely CP437 */
					term->utf8_expect = 0;
					term_put_cp437(term, ch);
				}
			}
			break;

		case PARSE_ESC:
			term_process_esc(term, ch);
			break;

		case PARSE_CSI:
		case PARSE_CSI_PARAM:
		case PARSE_CSI_INTERMEDIATE:
			term_process_csi(term, ch);
			break;

		case PARSE_OSC:
		case PARSE_OSC_ESC:
			term_process_osc(term, ch);
			break;

		case PARSE_DCS:
			/* Consume DCS payload until ST (ESC \) */
			if (ch == 0x1B)
				term->parse_state = PARSE_DCS_ESC;
			/* BEL or 8-bit ST terminates DCS */
			else if (ch == 0x07 || ch == 0x9C)
				term->parse_state = PARSE_NORMAL;
			break;

		case PARSE_DCS_ESC:
			/* ESC inside DCS: only \ terminates (ST) */
			if (ch == '\\')
				term->parse_state = PARSE_NORMAL;
			else
				/* Not ST — stay in DCS, consume byte */
				term->parse_state = PARSE_DCS;
			break;
		}
	}

}

/*
 * terminal_get_display_cell - get cell for display, accounting for scroll offset
 *
 * When scroll_offset > 0, the top rows come from the scrollback buffer
 * and the bottom rows come from the screen buffer.
 */
TermCell *
terminal_get_display_cell(Terminal *term, short row, short col)
{
	short sb_row;
	short sb_idx;

	if (col < 0 || col >= TERM_COLS)
		return &term->screen[0][0];
	if (row < 0 || row >= TERM_ROWS)
		return &term->screen[0][0];

	if (term->scroll_offset == 0)
		return &term->screen[row][col];

	/*
	 * With scroll_offset > 0, display row 0 maps to
	 * scrollback line (sb_count - scroll_offset), and the
	 * bottom rows map to screen rows.
	 */
	sb_row = row - term->scroll_offset;
	if (sb_row < 0) {
		/* This row comes from scrollback */
		sb_idx = term->sb_head + (term->sb_count + sb_row);
		/* Safety: check for negative result before modulo */
		if (sb_idx < 0)
			sb_idx += TERM_SCROLLBACK_LINES;
		if (sb_idx >= TERM_SCROLLBACK_LINES)
			sb_idx -= TERM_SCROLLBACK_LINES;
		/* Final bounds check */
		if (sb_idx < 0 || sb_idx >= TERM_SCROLLBACK_LINES)
			return &term->screen[0][0];
		return &term->scrollback[sb_idx][col];
	}

	/* This row comes from the live screen */
	return &term->screen[sb_row][col];
}

/*
 * terminal_get_display_color - get color for display cell, accounting
 * for scroll offset.  Returns NULL if color is not available.
 */
CellColor *
terminal_get_display_color(Terminal *term, short row, short col)
{
	short sb_row, sb_idx;

	if (!term->has_color || !term->screen_color)
		return 0L;

	if (col < 0 || col >= TERM_COLS || row < 0 || row >= TERM_ROWS)
		return 0L;

	if (term->scroll_offset == 0)
		return &term->screen_color[row * TERM_COLS + col];

	sb_row = row - term->scroll_offset;
	if (sb_row < 0) {
		/* From scrollback */
		if (!term->sb_color)
			return 0L;
		sb_idx = term->sb_head + (term->sb_count + sb_row);
		if (sb_idx < 0)
			sb_idx += TERM_SCROLLBACK_LINES;
		if (sb_idx >= TERM_SCROLLBACK_LINES)
			sb_idx -= TERM_SCROLLBACK_LINES;
		if (sb_idx < 0 || sb_idx >= TERM_SCROLLBACK_LINES)
			return 0L;
		return &term->sb_color[sb_idx * TERM_COLS + col];
	}

	return &term->screen_color[sb_row * TERM_COLS + col];
}

/*
 * terminal_scroll_back - scroll back N lines into scrollback history
 */
void
terminal_scroll_back(Terminal *term, short lines)
{
	if (lines <= 0) return;  /* Reject negative/zero scroll */
	term->scroll_offset += lines;
	if (term->scroll_offset > term->sb_count)
		term->scroll_offset = term->sb_count;
	term_dirty_all(term);
}

/*
 * terminal_scroll_forward - scroll forward N lines toward live view
 */
void
terminal_scroll_forward(Terminal *term, short lines)
{
	if (lines <= 0) return;
	term->scroll_offset -= lines;
	if (term->scroll_offset < 0)
		term->scroll_offset = 0;
	term_dirty_all(term);
}

/*
 * terminal_is_row_dirty - check if row needs redraw
 */
short
terminal_is_row_dirty(Terminal *term, short row)
{
	if (row < 0 || row >= TERM_ROWS)
		return 0;
	return term->dirty[row];
}

/*
 * terminal_clear_dirty - clear all dirty flags
 */
void
terminal_clear_dirty(Terminal *term)
{
	memset(term->dirty, 0, sizeof(term->dirty));
}

/*
 * terminal_get_cursor - get cursor position
 */
void
terminal_get_cursor(Terminal *term, short *row, short *col)
{
	*row = term->cur_row;
	*col = term->cur_col;
}

/* ----------------------------------------------------------------
 * Internal functions
 * ---------------------------------------------------------------- */

/*
 * term_clamp - clamp value to [lo, hi]
 */
static short
term_clamp(short val, short lo, short hi)
{
	if (val < lo)
		return lo;
	if (val > hi)
		return hi;
	return val;
}

/*
 * term_dirty_row - mark a single row as needing redraw
 */
static void
term_dirty_row(Terminal *term, short row)
{
	if (row >= 0 && row < term->active_rows)
		term->dirty[row] = 1;
}

/*
 * term_dirty_range - mark a range of rows dirty (inclusive)
 */
static void
term_dirty_range(Terminal *term, short r1, short r2)
{
	short r;

	r1 = term_clamp(r1, 0, term->active_rows - 1);
	r2 = term_clamp(r2, 0, term->active_rows - 1);
	for (r = r1; r <= r2; r++)
		term->dirty[r] = 1;
}

/*
 * term_dirty_all - mark entire screen dirty
 */
void
term_dirty_all(Terminal *term)
{
	term_dirty_range(term, 0, term->active_rows - 1);
}

/*
 * term_clear_region - fill a rectangular region with spaces and
 *                     current attributes
 */
static void
term_clear_region(Terminal *term, short r1, short c1, short r2, short c2)
{
	short r, c;

	r1 = term_clamp(r1, 0, term->active_rows - 1);
	r2 = term_clamp(r2, 0, term->active_rows - 1);
	c1 = term_clamp(c1, 0, term->active_cols - 1);
	c2 = term_clamp(c2, 0, term->active_cols - 1);

	/*
	 * Snapshot screen before full-screen clear (ESC[2J).
	 * Remote servers send clear-screen during logout before TCP
	 * closes. The snapshot lets us restore content on disconnect.
	 * Skip during alt screen (main screen saved in alt_screen).
	 */
	if (r1 == 0 && c1 == 0 &&
	    r2 >= term->active_rows - 1 &&
	    c2 >= term->active_cols - 1 &&
	    !term->alt_active) {
		memcpy(term->snap_screen, term->screen,
		    sizeof(term->screen));
		term->snap_rows = term->active_rows;
		term->snap_cols = term->active_cols;
		term->snap_valid = 1;
	}

	/*
	 * Word-at-a-time fill optimization: TermCell is 2 bytes
	 * (ch + attr), so we can fill with 16-bit writes.
	 * 68000 is big-endian: ch is high byte, attr is low byte.
	 * fill = (' ' << 8) | ATTR_NORMAL = 0x2000
	 */
	for (r = r1; r <= r2; r++) {
		unsigned short fill = ((unsigned short)' ' << 8) | ATTR_NORMAL;
		unsigned short *p = (unsigned short *)&term->screen[r][c1];
		for (c = c1; c <= c2; c++)
			*p++ = fill;
		term_dirty_row(term, r);
	}

	/* Clear color arrays separately (System 7 only) */
	if (term->has_color && term->screen_color) {
		for (r = r1; r <= r2; r++) {
			CellColor *cc = &term->screen_color[
			    r * TERM_COLS + c1];
			for (c = c1; c <= c2; c++) {
				cc->fg = COLOR_DEFAULT;
				cc->bg = COLOR_DEFAULT;
				cc++;
			}
		}
	}
}

/*
 * term_save_scrollback - save one line into the scrollback ring buffer
 */
static void
term_save_scrollback(Terminal *term, short row)
{
	if (row < 0 || row >= TERM_ROWS)
		return;

	memcpy(term->scrollback[term->sb_head], term->screen[row],
	    term->active_cols * sizeof(TermCell));

	/* Save color for this scrollback line (lazy-allocate on first use) */
	if (term->has_color && term->screen_color) {
		if (!term->sb_color) {
			long sb_size = (long)TERM_SCROLLBACK_LINES *
			    TERM_COLS * sizeof(CellColor);
			term->sb_color = (CellColor *)NewPtr(sb_size);
			if (term->sb_color)
				memset(term->sb_color, COLOR_DEFAULT,
				    sb_size);
		}
		if (term->sb_color) {
			memcpy(&term->sb_color[term->sb_head * TERM_COLS],
			    &term->screen_color[row * TERM_COLS],
			    term->active_cols * sizeof(CellColor));
		}
	}

	term->sb_head++;
	if (term->sb_head >= TERM_SCROLLBACK_LINES)
		term->sb_head = 0;
	if (term->sb_count < TERM_SCROLLBACK_LINES)
		term->sb_count++;
}

/*
 * term_scroll_up - scroll lines [top..bottom] up by count lines.
 *                  Lines scrolled off the top go to scrollback if
 *                  this is the main screen region.
 */
static void
term_scroll_up(Terminal *term, short top, short bottom, short count)
{
	short r, c;

	if (count <= 0 || top > bottom)
		return;
	if (count > (bottom - top + 1))
		count = bottom - top + 1;

	/* Save lines scrolled off top into scrollback (not on alt screen) */
	if (top == 0 && !term->alt_active) {
		for (r = 0; r < count; r++)
			term_save_scrollback(term, r);
	}

	/* Move lines up (copy only active columns for speed) */
	for (r = top; r <= bottom - count; r++) {
		memcpy(term->screen[r], term->screen[r + count],
		    term->active_cols * sizeof(TermCell));
		term->line_attr[r] = term->line_attr[r + count];
	}

	/* Scroll color arrays in parallel */
	if (term->has_color && term->screen_color) {
		for (r = top; r <= bottom - count; r++) {
			memcpy(&term->screen_color[r * TERM_COLS],
			    &term->screen_color[(r + count) * TERM_COLS],
			    term->active_cols * sizeof(CellColor));
		}
	}

	/* Clear newly exposed lines at bottom */
	for (r = bottom - count + 1; r <= bottom; r++) {
		for (c = 0; c < term->active_cols; c++) {
			term->screen[r][c].ch = ' ';
			term->screen[r][c].attr = ATTR_NORMAL;
		}
		term->line_attr[r] = LINE_ATTR_NORMAL;

		/* Clear color for exposed lines */
		if (term->has_color && term->screen_color) {
			CellColor *cc = &term->screen_color[r * TERM_COLS];
			for (c = 0; c < term->active_cols; c++) {
				cc->fg = COLOR_DEFAULT;
				cc->bg = COLOR_DEFAULT;
				cc++;
			}
		}
	}

	term_dirty_range(term, top, bottom);
}

/*
 * term_scroll_down - scroll lines [top..bottom] down by count lines.
 *                    New blank lines appear at the top.
 */
static void
term_scroll_down(Terminal *term, short top, short bottom, short count)
{
	short r, c;

	if (count <= 0 || top > bottom)
		return;
	if (count > (bottom - top + 1))
		count = bottom - top + 1;

	/* Move lines down (copy only active columns for speed) */
	for (r = bottom; r >= top + count; r--) {
		memcpy(term->screen[r], term->screen[r - count],
		    term->active_cols * sizeof(TermCell));
		term->line_attr[r] = term->line_attr[r - count];
	}

	/* Scroll color arrays in parallel */
	if (term->has_color && term->screen_color) {
		for (r = bottom; r >= top + count; r--) {
			memcpy(&term->screen_color[r * TERM_COLS],
			    &term->screen_color[(r - count) * TERM_COLS],
			    term->active_cols * sizeof(CellColor));
		}
	}

	/* Clear newly exposed lines at top */
	for (r = top; r < top + count; r++) {
		for (c = 0; c < term->active_cols; c++) {
			term->screen[r][c].ch = ' ';
			term->screen[r][c].attr = ATTR_NORMAL;
		}
		term->line_attr[r] = LINE_ATTR_NORMAL;

		/* Clear color for exposed lines */
		if (term->has_color && term->screen_color) {
			CellColor *cc = &term->screen_color[r * TERM_COLS];
			for (c = 0; c < term->active_cols; c++) {
				cc->fg = COLOR_DEFAULT;
				cc->bg = COLOR_DEFAULT;
				cc++;
			}
		}
	}

	term_dirty_range(term, top, bottom);
}

/*
 * term_put_char - place a printable character at the cursor and advance
 */
static void
term_put_char(Terminal *term, unsigned char ch)
{
	unsigned char active_set;
	unsigned char attr;
	short c;

	/* If wrap is pending, do the wrap now */
	if (term->wrap_pending) {
		if (term->autowrap) {
			term->wrap_pending = 0;
			term_carriage_return(term);
			term_newline(term);
		} else {
			/* No autowrap: overwrite at right margin */
			term->wrap_pending = 0;
			term->cur_col = term->active_cols - 1;
		}
	}

	/* Determine character attributes with charset translation */
	attr = term->cur_attr;
	active_set = (term->gl_charset == 0) ?
	    term->g0_charset : term->g1_charset;
	if (active_set == '0' && ch >= 0x60 && ch <= 0x7E)
		attr |= CELL_TYPE_DEC;

	/* Insert mode: shift line right before placing character */
	if (term->insert_mode) {
		for (c = term->active_cols - 1; c > term->cur_col; c--) {
			term->screen[term->cur_row][c] =
			    term->screen[term->cur_row][c - 1];
		}
		/* Shift color array too */
		if (term->has_color && term->screen_color) {
			for (c = term->active_cols - 1;
			    c > term->cur_col; c--) {
				term->screen_color[
				    term->cur_row * TERM_COLS + c] =
				    term->screen_color[
				    term->cur_row * TERM_COLS + c - 1];
			}
		}
	}

	/* Set ATTR_HAS_COLOR if non-default colors active */
	if (term->cur_fg != COLOR_DEFAULT || term->cur_bg != COLOR_DEFAULT)
		attr |= ATTR_HAS_COLOR;

	term->screen[term->cur_row][term->cur_col].ch = ch;
	term->screen[term->cur_row][term->cur_col].attr = attr;

	/* Write color to separate color array (System 7 only) */
	if (term->has_color && term->screen_color) {
		CellColor *cc = &term->screen_color[
		    term->cur_row * TERM_COLS + term->cur_col];
		cc->fg = term->cur_fg;
		cc->bg = term->cur_bg;
	}

	term_dirty_row(term, term->cur_row);

	if (term->cur_col < term->active_cols - 1) {
		term->cur_col++;
	} else {
		/* At right margin: set wrap pending for auto-wrap */
		term->wrap_pending = 1;
	}
}

/*
 * term_put_glyph - place a glyph cell (no charset translation)
 *
 * Like term_put_char but writes ch and attr directly.  Used for
 * CELL_TYPE_GLYPH and CELL_TYPE_BRAILLE cells.  For wide glyphs, places the
 * glyph in the first cell and GLYPH_WIDE_SPACER in the second.
 */
static void
term_put_glyph(Terminal *term, unsigned char glyph_id,
    unsigned char attr_flag)
{
	unsigned char attr;
	short wide;
	short c;

	wide = (attr_flag == CELL_TYPE_GLYPH) ? glyph_is_wide(glyph_id) : 0;

	/* Wide glyph at last column: place space and wrap first */
	if (wide && term->cur_col >= term->active_cols - 1) {
		/* Fill current cell with space */
		term->screen[term->cur_row][term->cur_col].ch = ' ';
		term->screen[term->cur_row][term->cur_col].attr = ATTR_NORMAL;
		term_dirty_row(term, term->cur_row);

		if (term->autowrap) {
			term_carriage_return(term);
			term_newline(term);
		} else {
			return;		/* can't fit, discard */
		}
	}

	/* Handle wrap_pending from previous character */
	if (term->wrap_pending) {
		if (term->autowrap) {
			term->wrap_pending = 0;
			term_carriage_return(term);
			term_newline(term);

			/* Re-check: wide glyph might not fit after wrap */
			if (wide && term->cur_col >= term->active_cols - 1)
				return;
		} else {
			term->wrap_pending = 0;
			term->cur_col = term->active_cols - 1;
		}
	}

	attr = term->cur_attr | attr_flag;

	/* Set ATTR_HAS_COLOR if non-default colors active */
	if (term->cur_fg != COLOR_DEFAULT || term->cur_bg != COLOR_DEFAULT)
		attr |= ATTR_HAS_COLOR;

	/* Insert mode: shift right */
	if (term->insert_mode) {
		short shift = wide ? 2 : 1;

		for (c = term->active_cols - 1;
		    c > term->cur_col + shift - 1; c--) {
			term->screen[term->cur_row][c] =
			    term->screen[term->cur_row][c - shift];
		}
		/* Shift color array too */
		if (term->has_color && term->screen_color) {
			for (c = term->active_cols - 1;
			    c > term->cur_col + shift - 1; c--) {
				term->screen_color[
				    term->cur_row * TERM_COLS + c] =
				    term->screen_color[
				    term->cur_row * TERM_COLS + c - shift];
			}
		}
	}

	/* Place primary glyph cell */
	term->screen[term->cur_row][term->cur_col].ch = glyph_id;
	term->screen[term->cur_row][term->cur_col].attr = attr;

	/* Write color for primary cell */
	if (term->has_color && term->screen_color) {
		CellColor *cc = &term->screen_color[
		    term->cur_row * TERM_COLS + term->cur_col];
		cc->fg = term->cur_fg;
		cc->bg = term->cur_bg;
	}

	term_dirty_row(term, term->cur_row);

	if (wide) {
		/* Place wide spacer in second cell */
		term->cur_col++;
		term->screen[term->cur_row][term->cur_col].ch =
		    GLYPH_WIDE_SPACER;
		term->screen[term->cur_row][term->cur_col].attr = attr;
		/* Write color for spacer cell too */
		if (term->has_color && term->screen_color) {
			CellColor *cc = &term->screen_color[
			    term->cur_row * TERM_COLS + term->cur_col];
			cc->fg = term->cur_fg;
			cc->bg = term->cur_bg;
		}
	}

	if (term->cur_col < term->active_cols - 1) {
		term->cur_col++;
	} else {
		term->wrap_pending = 1;
	}
}

/*
 * term_put_cp437 - place a CP437 character, bypassing UTF-8 decoder
 *
 * Looks up the byte in the CP437 table and stores the appropriate
 * ch + attr.  For ASCII/MACROMAN, stores as normal text.  For GLYPH,
 * stores with CELL_TYPE_GLYPH.  For SPACE, stores space.
 */
static void
term_put_cp437(Terminal *term, unsigned char byte)
{
	const CP437Entry *e;
	unsigned char ch, attr;

	e = &cp437_table[byte];

	switch (e->method) {
	case CP437_GLYPH:
		term_put_glyph(term, e->value, CELL_TYPE_GLYPH);
		return;
	case CP437_MACROMAN:
		ch = e->value;
		break;
	case CP437_ASCII:
		ch = e->value;
		break;
	case CP437_SPACE:
	default:
		ch = ' ';
		break;
	}

	/* Store as normal text cell (reuse term_put_char logic) */
	if (term->wrap_pending) {
		if (term->autowrap) {
			term->wrap_pending = 0;
			term_carriage_return(term);
			term_newline(term);
		} else {
			term->wrap_pending = 0;
			term->cur_col = term->active_cols - 1;
		}
	}

	attr = term->cur_attr;
	if (term->cur_fg != COLOR_DEFAULT || term->cur_bg != COLOR_DEFAULT)
		attr |= ATTR_HAS_COLOR;

	term->screen[term->cur_row][term->cur_col].ch = ch;
	term->screen[term->cur_row][term->cur_col].attr = attr;

	if (term->has_color && term->screen_color) {
		CellColor *cc = &term->screen_color[
		    term->cur_row * TERM_COLS + term->cur_col];
		cc->fg = term->cur_fg;
		cc->bg = term->cur_bg;
	}

	term_dirty_row(term, term->cur_row);

	if (term->cur_col < term->active_cols - 1)
		term->cur_col++;
	else
		term->wrap_pending = 1;
}

/*
 * term_newline - move cursor down one line, scrolling if at bottom
 *                of scroll region
 */
static void
term_newline(Terminal *term)
{
	term->wrap_pending = 0;

	if (term->cur_row == term->scroll_bottom) {
		/* At bottom of scroll region: scroll up */
		term_scroll_up(term, term->scroll_top, term->scroll_bottom, 1);
	} else if (term->cur_row < term->active_rows - 1) {
		term->cur_row++;
	}
}

/*
 * term_carriage_return - move cursor to column 0
 */
static void
term_carriage_return(Terminal *term)
{
	term->cur_col = 0;
	term->wrap_pending = 0;
}

/*
 * term_process_esc - handle byte after ESC
 */
static void
term_process_esc(Terminal *term, unsigned char ch)
{
	switch (ch) {
	case '[':
		/* CSI introducer */
		term->parse_state = PARSE_CSI;
		term->num_params = 0;
		term->intermediate = 0;
		memset(term->params, 0, sizeof(term->params));
		return;

	case '7':
		/* DECSC - save cursor position, attributes, and charsets */
		term->saved_row = term->cur_row;
		term->saved_col = term->cur_col;
		term->saved_attr = term->cur_attr;
		term->saved_fg = term->cur_fg;
		term->saved_bg = term->cur_bg;
		term->saved_g0_charset = term->g0_charset;
		term->saved_g1_charset = term->g1_charset;
		term->saved_gl_charset = term->gl_charset;
		term->saved_origin_mode = term->origin_mode;
		term->saved_autowrap = term->autowrap;
		break;

	case '8':
		/* DECRC - restore cursor position, attributes, and charsets */
		term->cur_row = term_clamp(term->saved_row, 0, term->active_rows - 1);
		term->cur_col = term_clamp(term->saved_col, 0, term->active_cols - 1);
		term->cur_attr = term->saved_attr;
		term->cur_fg = term->saved_fg;
		term->cur_bg = term->saved_bg;
		term->g0_charset = term->saved_g0_charset;
		term->g1_charset = term->saved_g1_charset;
		term->gl_charset = term->saved_gl_charset;
		term->origin_mode = term->saved_origin_mode;
		term->autowrap = term->saved_autowrap;
		term->wrap_pending = 0;
		break;

	case 'D':
		/* Index: move cursor down, scroll if at bottom */
		term_newline(term);
		break;

	case 'M':
		/* Reverse index: move cursor up, scroll if at top */
		term->wrap_pending = 0;
		if (term->cur_row == term->scroll_top) {
			term_scroll_down(term, term->scroll_top,
			    term->scroll_bottom, 1);
		} else if (term->cur_row > 0) {
			term->cur_row--;
		}
		break;

	case 'E':
		/* Next line: CR + LF */
		term_carriage_return(term);
		term_newline(term);
		break;

	case 'H':
		/* HTS - Horizontal Tab Set */
		term->tab_stops[term->cur_col] = 1;
		return;

	case '=':
		/* DECKPAM - application keypad mode */
		term->keypad_mode = 1;
		break;

	case '>':
		/* DECKPNM - normal keypad mode */
		term->keypad_mode = 0;
		break;

	case 'c':
		/* Full reset (RIS) */
		terminal_reset(term);
		return;

	case ']':
		/* OSC introducer */
		term->osc_len = 0;
		term->osc_param = -1;
		term->parse_state = PARSE_OSC;
		return;

	case 'P':
		/* DCS introducer - consume until ST */
		term->parse_state = PARSE_DCS;
		return;

	case '(':
	case ')':
	case '#':
		/*
		 * Character set designation / DEC double-width etc.
		 * These consume one more byte.  Store the intermediate
		 * and stay in PARSE_ESC to eat the follow-up byte.
		 */
		term->intermediate = ch;
		return;

	default:
		if (term->intermediate == '(') {
			/* ESC ( X - designate G0 charset */
			term->g0_charset = ch;
		} else if (term->intermediate == ')') {
			/* ESC ) X - designate G1 charset */
			term->g1_charset = ch;
		} else if (term->intermediate == '#') {
			/* DEC line attributes */
			switch (ch) {
			case '3':	/* DECDHL top half */
				term->line_attr[term->cur_row] =
				    LINE_ATTR_DBLH_TOP;
				term->dirty[term->cur_row] = 1;
				break;
			case '4':	/* DECDHL bottom half */
				term->line_attr[term->cur_row] =
				    LINE_ATTR_DBLH_BOT;
				term->dirty[term->cur_row] = 1;
				break;
			case '5':	/* DECSWL single-width */
				term->line_attr[term->cur_row] =
				    LINE_ATTR_NORMAL;
				term->dirty[term->cur_row] = 1;
				break;
			case '6':	/* DECDWL double-width */
				term->line_attr[term->cur_row] =
				    LINE_ATTR_DBLW;
				term->dirty[term->cur_row] = 1;
				break;
			}
		}
		term->intermediate = 0;
		break;
	}

	term->parse_state = PARSE_NORMAL;
}

/*
 * term_process_csi - accumulate CSI sequence bytes and dispatch
 */
static void
term_process_csi(Terminal *term, unsigned char ch)
{
	/*
	 * CSI format:  ESC [ [?] P1 ; P2 ; ... Ic F
	 *   P  = parameter byte    (0x30-0x3F: digits, semicolons, etc.)
	 *   Ic = intermediate byte (0x20-0x2F)
	 *   F  = final byte        (0x40-0x7E)
	 */

	if (ch >= '0' && ch <= '9') {
		/* Digit: accumulate into current parameter */
		if (term->parse_state == PARSE_CSI) {
			/* First digit: start first param */
			term->num_params = 1;
			term->params[0] = ch - '0';
			term->parse_state = PARSE_CSI_PARAM;
		} else if (term->parse_state == PARSE_CSI_PARAM) {
			if (term->num_params > 0 &&
			    term->num_params <= TERM_MAX_PARAMS) {
				short idx = term->num_params - 1;
				if (term->params[idx] < 3000) {
					short new_val = term->params[idx] * 10 +
					    (ch - '0');
					if (new_val >= 0)  /* signed overflow check */
						term->params[idx] = new_val;
				}
			}
			/* else: overflow params silently dropped */
		}
		/* Digits after intermediate are invalid; ignore */
		return;
	}

	if (ch == ';' || ch == ':') {
		/*
		 * Parameter separator.  Semicolons separate parameters;
		 * colons separate sub-parameters (SGR 38:2:R:G:B,
		 * SGR 4:3, etc.).  We treat both the same way --
		 * slightly wrong for sub-params but prevents aborting
		 * on modern color/underline sequences.
		 */
		if (term->parse_state == PARSE_CSI) {
			/* Implicit first param = 0 */
			term->num_params = 1;
			term->params[0] = 0;
		}
		if (term->num_params < TERM_MAX_PARAMS) {
			term->params[term->num_params] = 0;
			term->num_params++;
		}
		term->parse_state = PARSE_CSI_PARAM;
		return;
	}

	if (ch == '?' || ch == '>' || ch == '<' || ch == '=') {
		/* Private mode introducer (? for DECSET, > for secondary DA) */
		term->intermediate = ch;
		return;
	}

	if (ch >= 0x20 && ch <= 0x2F) {
		/* Intermediate byte */
		term->intermediate = ch;
		term->parse_state = PARSE_CSI_INTERMEDIATE;
		return;
	}

	if (ch >= 0x40 && ch <= 0x7E) {
		/* Final byte: execute the command */
		term_execute_csi(term, ch);
		term->parse_state = PARSE_NORMAL;
		return;
	}

	/*
	 * Unexpected byte (e.g. control char inside sequence).
	 * Abort the sequence and return to normal mode.
	 */
	term->parse_state = PARSE_NORMAL;
}

/*
 * term_dec_set_mode - handle CSI ? h (DECSET)
 */
static void
term_dec_set_mode(Terminal *term)
{
	short i;

	for (i = 0; i < term->num_params; i++) {
		switch (term->params[i]) {
		case 1:
			term->cursor_key_mode = 1;
			break;
		case 6:
			term->origin_mode = 1;
			term->cur_row = term->scroll_top;
			term->cur_col = 0;
			term->wrap_pending = 0;
			break;
		case 7:
			term->autowrap = 1;
			break;
		case 25:
			term->cursor_visible = 1;
			break;
		case 47:
		case 1047:
			term_switch_to_alt(term);
			break;
		case 1048:
			/* Save cursor */
			term->saved_row = term->cur_row;
			term->saved_col = term->cur_col;
			term->saved_attr = term->cur_attr;
			term->saved_fg = term->cur_fg;
			term->saved_bg = term->cur_bg;
			break;
		case 1049:
			/* Save cursor + switch to alt */
			term->saved_row = term->cur_row;
			term->saved_col = term->cur_col;
			term->saved_attr = term->cur_attr;
			term->saved_fg = term->cur_fg;
			term->saved_bg = term->cur_bg;
			term_switch_to_alt(term);
			break;
		case 2004:
			term->bracketed_paste = 1;
			break;
		case 12:	/* cursor blink */
		case 1000:	/* mouse click reporting */
		case 1002:	/* mouse button-event tracking */
		case 1003:	/* mouse all-motion tracking */
		case 1004:	/* focus events */
		case 1006:	/* SGR mouse mode */
			break;
		}
	}
}

/*
 * term_dec_reset_mode - handle CSI ? l (DECRST)
 */
static void
term_dec_reset_mode(Terminal *term)
{
	short i;

	for (i = 0; i < term->num_params; i++) {
		switch (term->params[i]) {
		case 1:
			term->cursor_key_mode = 0;
			break;
		case 6:
			term->origin_mode = 0;
			term->cur_row = 0;
			term->cur_col = 0;
			term->wrap_pending = 0;
			break;
		case 7:
			term->autowrap = 0;
			break;
		case 25:
			term->cursor_visible = 0;
			break;
		case 47:
		case 1047:
			term_switch_to_main(term);
			break;
		case 1048:
			/* Restore cursor */
			term->cur_row = term_clamp(
			    term->saved_row, 0,
			    term->active_rows - 1);
			term->cur_col = term_clamp(
			    term->saved_col, 0,
			    term->active_cols - 1);
			term->cur_attr = term->saved_attr;
			term->cur_fg = term->saved_fg;
			term->cur_bg = term->saved_bg;
			term->wrap_pending = 0;
			break;
		case 1049:
			/* Switch to main + restore cursor */
			term_switch_to_main(term);
			term->cur_row = term_clamp(
			    term->saved_row, 0,
			    term->active_rows - 1);
			term->cur_col = term_clamp(
			    term->saved_col, 0,
			    term->active_cols - 1);
			term->cur_attr = term->saved_attr;
			term->cur_fg = term->saved_fg;
			term->cur_bg = term->saved_bg;
			term->wrap_pending = 0;
			break;
		case 2004:
			term->bracketed_paste = 0;
			break;
		case 12:	/* cursor blink */
		case 1000:	/* mouse click reporting */
		case 1002:	/* mouse button-event tracking */
		case 1003:	/* mouse all-motion tracking */
		case 1004:	/* focus events */
		case 1006:	/* SGR mouse mode */
			break;
		}
	}
}

/*
 * term_execute_csi - execute a fully parsed CSI sequence
 */
static void
term_execute_csi(Terminal *term, unsigned char cmd)
{
	short p1, p2;
	short c;

	/* Convenience: first two params with defaults */
	p1 = (term->num_params >= 1 && term->params[0] > 0) ?
	    term->params[0] : 1;
	p2 = (term->num_params >= 2 && term->params[1] > 0) ?
	    term->params[1] : 1;

	switch (cmd) {
	case 'A':
		/* CUU - cursor up */
		term->wrap_pending = 0;
		term->cur_row -= p1;
		if (term->cur_row < 0)
			term->cur_row = 0;
		break;

	case 'B':
		/* CUD - cursor down */
		term->wrap_pending = 0;
		term->cur_row += p1;
		if (term->cur_row >= term->active_rows)
			term->cur_row = term->active_rows - 1;
		break;

	case 'C':
		/* CUF - cursor forward (right) */
		term->wrap_pending = 0;
		term->cur_col += p1;
		if (term->cur_col >= term->active_cols)
			term->cur_col = term->active_cols - 1;
		break;

	case 'D':
		/* CUB - cursor backward (left) */
		term->wrap_pending = 0;
		term->cur_col -= p1;
		if (term->cur_col < 0)
			term->cur_col = 0;
		break;

	case 'H':
	case 'f':
		/* CUP / HVP - cursor position (row;col, 1-based) */
		term->wrap_pending = 0;
		if (term->origin_mode) {
			term->cur_row = term_clamp(p1 - 1 + term->scroll_top,
			    term->scroll_top, term->scroll_bottom);
		} else {
			term->cur_row = term_clamp(p1 - 1, 0, term->active_rows - 1);
		}
		term->cur_col = term_clamp(p2 - 1, 0, term->active_cols - 1);
		break;

	case 'J':
		/* ED - erase in display */
		p1 = (term->num_params >= 1) ? term->params[0] : 0;
		switch (p1) {
		case 0:
			/* Cursor to end of screen */
			term_clear_region(term, term->cur_row, term->cur_col,
			    term->cur_row, term->active_cols - 1);
			if (term->cur_row < term->active_rows - 1)
				term_clear_region(term, term->cur_row + 1, 0,
				    term->active_rows - 1, term->active_cols - 1);
			break;
		case 1:
			/* Start of screen to cursor */
			if (term->cur_row > 0)
				term_clear_region(term, 0, 0,
				    term->cur_row - 1, term->active_cols - 1);
			term_clear_region(term, term->cur_row, 0,
			    term->cur_row, term->cur_col);
			break;
		case 2:
			/* Entire screen */
			term_clear_region(term, 0, 0,
			    term->active_rows - 1, term->active_cols - 1);
			break;
		}
		break;

	case 'K':
		/* EL - erase in line */
		p1 = (term->num_params >= 1) ? term->params[0] : 0;
		switch (p1) {
		case 0:
			/* Cursor to end of line */
			term_clear_region(term, term->cur_row, term->cur_col,
			    term->cur_row, term->active_cols - 1);
			break;
		case 1:
			/* Start of line to cursor */
			term_clear_region(term, term->cur_row, 0,
			    term->cur_row, term->cur_col);
			break;
		case 2:
			/* Entire line */
			term_clear_region(term, term->cur_row, 0,
			    term->cur_row, term->active_cols - 1);
			break;
		}
		break;

	case 'L':
		/* IL - insert lines at cursor row within scroll region */
		if (term->cur_row >= term->scroll_top &&
		    term->cur_row <= term->scroll_bottom) {
			term_scroll_down(term, term->cur_row,
			    term->scroll_bottom, p1);
		}
		break;

	case 'M':
		/* DL - delete lines at cursor row within scroll region */
		if (term->cur_row >= term->scroll_top &&
		    term->cur_row <= term->scroll_bottom) {
			term_scroll_up(term, term->cur_row,
			    term->scroll_bottom, p1);
		}
		break;

	case '@':
		/* ICH - insert characters at cursor, shifting right */
		p1 = term_clamp(p1, 1, term->active_cols - term->cur_col);
		for (c = term->active_cols - 1; c >= term->cur_col + p1; c--) {
			term->screen[term->cur_row][c] =
			    term->screen[term->cur_row][c - p1];
		}
		for (c = term->cur_col; c < term->cur_col + p1; c++) {
			term->screen[term->cur_row][c].ch = ' ';
			term->screen[term->cur_row][c].attr = ATTR_NORMAL;
		}
		if (term->has_color && term->screen_color) {
			short row_off = term->cur_row * TERM_COLS;
			for (c = term->active_cols - 1;
			    c >= term->cur_col + p1; c--)
				term->screen_color[row_off + c] =
				    term->screen_color[row_off + c - p1];
			for (c = term->cur_col; c < term->cur_col + p1; c++) {
				term->screen_color[row_off + c].fg =
				    COLOR_DEFAULT;
				term->screen_color[row_off + c].bg =
				    COLOR_DEFAULT;
			}
		}
		term_dirty_row(term, term->cur_row);
		break;

	case 'P':
		/* DCH - delete characters at cursor, shifting left */
		p1 = term_clamp(p1, 1, term->active_cols - term->cur_col);
		for (c = term->cur_col; c < term->active_cols - p1; c++) {
			term->screen[term->cur_row][c] =
			    term->screen[term->cur_row][c + p1];
		}
		for (c = term->active_cols - p1; c < term->active_cols; c++) {
			term->screen[term->cur_row][c].ch = ' ';
			term->screen[term->cur_row][c].attr = ATTR_NORMAL;
		}
		if (term->has_color && term->screen_color) {
			short row_off = term->cur_row * TERM_COLS;
			for (c = term->cur_col;
			    c < term->active_cols - p1; c++)
				term->screen_color[row_off + c] =
				    term->screen_color[row_off + c + p1];
			for (c = term->active_cols - p1;
			    c < term->active_cols; c++) {
				term->screen_color[row_off + c].fg =
				    COLOR_DEFAULT;
				term->screen_color[row_off + c].bg =
				    COLOR_DEFAULT;
			}
		}
		term_dirty_row(term, term->cur_row);
		break;

	case 'X':
		/* ECH - erase characters at cursor (don't move cursor) */
		p1 = term_clamp(p1, 1, term->active_cols - term->cur_col);
		for (c = term->cur_col; c < term->cur_col + p1; c++) {
			term->screen[term->cur_row][c].ch = ' ';
			term->screen[term->cur_row][c].attr = ATTR_NORMAL;
		}
		if (term->has_color && term->screen_color) {
			short row_off = term->cur_row * TERM_COLS;
			for (c = term->cur_col; c < term->cur_col + p1; c++) {
				term->screen_color[row_off + c].fg =
				    COLOR_DEFAULT;
				term->screen_color[row_off + c].bg =
				    COLOR_DEFAULT;
			}
		}
		term_dirty_row(term, term->cur_row);
		break;

	case 'r':
		/* DECSTBM - set scroll region (top;bottom, 1-based) */
		p1 = (term->num_params >= 1 && term->params[0] > 0) ?
		    term->params[0] : 1;
		p2 = (term->num_params >= 2 && term->params[1] > 0) ?
		    term->params[1] : term->active_rows;
		term->scroll_top = term_clamp(p1 - 1, 0, term->active_rows - 1);
		term->scroll_bottom = term_clamp(p2 - 1, 0, term->active_rows - 1);
		if (term->scroll_top >= term->scroll_bottom) {
			/* Invalid: reset to full screen */
			term->scroll_top = 0;
			term->scroll_bottom = term->active_rows - 1;
		}
		/* Cursor moves to home after setting scroll region */
		term->cur_row = term->origin_mode ? term->scroll_top : 0;
		term->cur_col = 0;
		term->wrap_pending = 0;
		break;

	case 'm':
		/* SGR - set graphic rendition */
		if (term->num_params == 0) {
			/* ESC[m with no params means reset */
			term_set_attr(term, 0);
		} else {
			short i;
			for (i = 0; i < term->num_params; i++) {
				if (term->params[i] == 38 ||
				    term->params[i] == 48) {
					/* Extended fg/bg color */
					short is_fg = (term->params[i] == 38);
					if (i + 1 < term->num_params &&
					    term->params[i + 1] == 5) {
						/* 256-color: 38;5;N / 48;5;N */
						if (i + 2 < term->num_params) {
							unsigned char ci =
							    (unsigned char)
							    term->params[i + 2];
							if (is_fg)
								term->cur_fg = ci;
							else
								term->cur_bg = ci;
						}
						i += 2;
					} else if (i + 1 < term->num_params &&
					    term->params[i + 1] == 2) {
						/* Truecolor: 38;2;R;G;B */
						if (i + 4 < term->num_params) {
							unsigned char ci =
							    color_nearest_256(
							    (unsigned char)
							    term->params[i + 2],
							    (unsigned char)
							    term->params[i + 3],
							    (unsigned char)
							    term->params[i + 4]);
							if (is_fg)
								term->cur_fg = ci;
							else
								term->cur_bg = ci;
						}
						i += 4;
					}
					continue;
				}
				if (term->params[i] == 58) {
					/* Underline color: skip */
					if (i + 1 < term->num_params &&
					    term->params[i + 1] == 5)
						i += 2;
					else if (i + 1 < term->num_params &&
					    term->params[i + 1] == 2)
						i += 4;
					continue;
				}
				term_set_attr(term, term->params[i]);
			}
		}
		break;

	case 'd':
		/* VPA - line position absolute (row, 1-based) */
		term->wrap_pending = 0;
		term->cur_row = term_clamp(p1 - 1, 0, term->active_rows - 1);
		break;

	case 'G':
	case '`':
		/* CHA / HPA - cursor character absolute (column, 1-based) */
		term->wrap_pending = 0;
		term->cur_col = term_clamp(p1 - 1, 0, term->active_cols - 1);
		break;

	case 'E':
		/* CNL - cursor next line */
		term->wrap_pending = 0;
		term->cur_row += p1;
		if (term->cur_row >= term->active_rows)
			term->cur_row = term->active_rows - 1;
		term->cur_col = 0;
		break;

	case 'F':
		/* CPL - cursor preceding line */
		term->wrap_pending = 0;
		term->cur_row -= p1;
		if (term->cur_row < 0)
			term->cur_row = 0;
		term->cur_col = 0;
		break;

	case 'S':
		/* SU - scroll up */
		term_scroll_up(term, term->scroll_top, term->scroll_bottom,
		    p1);
		break;

	case 'T':
		/* SD - scroll down */
		term_scroll_down(term, term->scroll_top, term->scroll_bottom,
		    p1);
		break;

	case 'h':
		/* SM - set mode */
		if (term->intermediate == '?')
			term_dec_set_mode(term);
		else if (term->intermediate == 0) {
			short i;
			for (i = 0; i < term->num_params; i++) {
				if (term->params[i] == 4)
					term->insert_mode = 1;
			}
		}
		break;

	case 'l':
		/* RM - reset mode */
		if (term->intermediate == '?')
			term_dec_reset_mode(term);
		else if (term->intermediate == 0) {
			short i;
			for (i = 0; i < term->num_params; i++) {
				if (term->params[i] == 4)
					term->insert_mode = 0;
			}
		}
		break;

	case 'c':
		/* DA - device attributes */
		if (term->intermediate == '>') {
			/* Secondary DA: respond as VT220 */
			memcpy(term->response, "\033[>1;10;0c", 10);
			term->response_len = 10;
		} else if (term->intermediate == 0 ||
		    term->intermediate == '?') {
			/* Primary DA: respond as VT220 */
			memcpy(term->response, "\033[?62;1;6c", 10);
			term->response_len = 10;
		}
		break;

	case 's':
		/* SCOSC - save cursor position (like ESC 7) */
		if (term->intermediate == 0) {
			term->saved_row = term->cur_row;
			term->saved_col = term->cur_col;
			term->saved_attr = term->cur_attr;
			term->saved_fg = term->cur_fg;
			term->saved_bg = term->cur_bg;
		}
		break;

	case 'u':
		/* SCORC - restore cursor position (like ESC 8) */
		if (term->intermediate == 0) {
			term->cur_row = term_clamp(term->saved_row,
			    0, term->active_rows - 1);
			term->cur_col = term_clamp(term->saved_col,
			    0, term->active_cols - 1);
			term->cur_attr = term->saved_attr;
			term->cur_fg = term->saved_fg;
			term->cur_bg = term->saved_bg;
			term->wrap_pending = 0;
		}
		break;

	case 'g':
		/* TBC - Tabulation Clear */
		if (term->intermediate == 0) {
			p1 = (term->num_params >= 1) ?
			    term->params[0] : 0;
			if (p1 == 0)
				term->tab_stops[term->cur_col] = 0;
			else if (p1 == 3)
				memset(term->tab_stops, 0,
				    sizeof(term->tab_stops));
		}
		break;

	case 'q':
		/* DECSCUSR - Set Cursor Style (CSI Ps SP q) */
		if (term->intermediate == ' ') {
			p1 = (term->num_params >= 1) ?
			    term->params[0] : 0;
			if (p1 <= 6)
				term->cursor_style =
				    (unsigned char)p1;
		}
		break;

	case 'p':
		/* DECSTR - soft reset (CSI ! p) */
		if (term->intermediate == '!') {
			term->cur_attr = ATTR_NORMAL;
			term->cur_fg = COLOR_DEFAULT;
			term->cur_bg = COLOR_DEFAULT;
			term->g0_charset = 'B';
			term->g1_charset = 'B';
			term->gl_charset = 0;
			term->origin_mode = 0;
			term->autowrap = 1;
			term->cursor_key_mode = 0;
			term->keypad_mode = 0;
			term->insert_mode = 0;
			term->cursor_visible = 1;
			term->scroll_top = 0;
			term->scroll_bottom = term->active_rows - 1;
			term->saved_row = 0;
			term->saved_col = 0;
			term->saved_attr = ATTR_NORMAL;
			term->saved_fg = COLOR_DEFAULT;
			term->saved_bg = COLOR_DEFAULT;
			term->wrap_pending = 0;
			term->cursor_style = 0;
			/* Reset tab stops to defaults */
			memset(term->tab_stops, 0,
			    sizeof(term->tab_stops));
			{
				short c;
				for (c = 0; c < TERM_COLS; c += 8)
					term->tab_stops[c] = 1;
			}
			memset(term->line_attr, 0,
			    sizeof(term->line_attr));
		}
		break;

	case 'n':
		/* DSR - device status report */
		p1 = (term->num_params >= 1) ? term->params[0] : 0;
		if (p1 == 6) {
			/* Cursor position report */
			term->response_len = snprintf(term->response,
			    sizeof(term->response),
			    "\033[%d;%dR",
			    term->cur_row + 1, term->cur_col + 1);
			if (term->response_len >= (short)sizeof(term->response))
				term->response_len = sizeof(term->response) - 1;
		} else if (p1 == 5) {
			/* Status report: OK */
			memcpy(term->response, "\033[0n", 4);
			term->response_len = 4;
		}
		break;

	default:
		/* Unrecognised CSI sequence: silently ignore */
		break;
	}
}

/*
 * term_set_attr - apply a single SGR parameter
 *
 * Note: 256-color (38;5;N) and truecolor (38;2;R;G;B) sequences
 * are skipped in the SGR loop in term_execute_csi, not here.
 */
static void
term_set_attr(Terminal *term, short param)
{
	switch (param) {
	case 0:
		/* Reset all attributes and colors */
		term->cur_attr = ATTR_NORMAL;
		term->cur_fg = COLOR_DEFAULT;
		term->cur_bg = COLOR_DEFAULT;
		break;
	case 1:
		/* Bold */
		term->cur_attr |= ATTR_BOLD;
		break;
	case 2:
		/* Dim/faint */
		term->cur_attr &= ~ATTR_BOLD;
		if (g_has_color_qd) {
			unsigned char dimmed;
			term->pre_dim_fg = term->cur_fg;
			if (term->cur_fg == COLOR_DEFAULT)
				dimmed = 8;	/* bright black (gray) */
			else
				dimmed = color_dim(term->cur_fg);
			term->cur_fg = dimmed;
			term->cur_attr |= ATTR_HAS_COLOR;
		}
		break;
	case 3:
		/* Italic */
		term->cur_attr |= ATTR_ITALIC;
		break;
	case 4:
		/* Underline */
		term->cur_attr |= ATTR_UNDERLINE;
		break;
	case 5:
	case 6:
		/* Blink / rapid blink */
		term->cur_attr |= ATTR_BOLD;
		if (g_has_color_qd) {
			/* DOS-style: bright background */
			if (term->cur_bg != COLOR_DEFAULT &&
			    term->cur_bg < 8)
				term->cur_bg += 8;
			term->cur_attr |= ATTR_HAS_COLOR;
		}
		break;
	case 7:
		/* Inverse / reverse video */
		term->cur_attr |= ATTR_INVERSE;
		break;
	case 9:
		/* Strikethrough */
		term->cur_attr |= ATTR_STRIKETHROUGH;
		break;
	case 22:
		/* Normal intensity (un-bold, un-dim) */
		term->cur_attr &= ~ATTR_BOLD;
		if (g_has_color_qd && term->pre_dim_fg != COLOR_DEFAULT) {
			term->cur_fg = term->pre_dim_fg;
			term->pre_dim_fg = COLOR_DEFAULT;
		}
		break;
	case 23:
		/* Not italic */
		term->cur_attr &= ~ATTR_ITALIC;
		break;
	case 24:
		/* Not underlined */
		term->cur_attr &= ~ATTR_UNDERLINE;
		break;
	case 25:
		/* Blink off - clear bold (since we mapped blink to bold) */
		term->cur_attr &= ~ATTR_BOLD;
		break;
	case 27:
		/* Not inverse */
		term->cur_attr &= ~ATTR_INVERSE;
		break;
	case 29:
		/* Not strikethrough */
		term->cur_attr &= ~ATTR_STRIKETHROUGH;
		break;
	case 39:
		/* Default foreground */
		term->cur_fg = COLOR_DEFAULT;
		break;
	case 49:
		/* Default background */
		term->cur_bg = COLOR_DEFAULT;
		break;
	default:
		if (param >= 30 && param <= 37) {
			/* Standard ANSI foreground (palette 0-7) */
			term->cur_fg = (unsigned char)(param - 30);
		} else if (param >= 40 && param <= 47) {
			/* Standard ANSI background (palette 0-7) */
			term->cur_bg = (unsigned char)(param - 40);
		} else if (param >= 90 && param <= 97) {
			/* Bright foreground (palette 8-15) */
			term->cur_fg = (unsigned char)(param - 90 + 8);
			/* Also set bold for monochrome fallback */
			term->cur_attr |= ATTR_BOLD;
		} else if (param >= 100 && param <= 107) {
			/* Bright background (palette 8-15) */
			term->cur_bg = (unsigned char)(param - 100 + 8);
		}
		break;
	}
}

/*
 * term_process_osc - accumulate OSC sequence bytes
 *
 * OSC format: ESC ] Ps ; Pt BEL   (or ESC ] Ps ; Pt ESC \)
 * Ps = numeric parameter, Pt = text payload
 */
static void
term_process_osc(Terminal *term, unsigned char ch)
{
	if (term->parse_state == PARSE_OSC_ESC) {
		/* Previous byte was ESC inside OSC */
		if (ch == '\\') {
			/* ST (String Terminator) - finish OSC */
			term_finish_osc(term);
		}
		/* Any other byte after ESC in OSC: abort */
		term->osc_buf[term->osc_len] = '\0';
		term->parse_state = PARSE_NORMAL;
		return;
	}

	/* PARSE_OSC state */
	if (ch == 0x07 || ch == 0x9C) {
		/* BEL or 8-bit ST terminates OSC */
		term_finish_osc(term);
		term->parse_state = PARSE_NORMAL;
		return;
	}

	if (ch == 0x1B) {
		/* ESC inside OSC: look for \ next */
		term->parse_state = PARSE_OSC_ESC;
		return;
	}

	/* Semicolon: end of parameter, start of payload */
	if (ch == ';' && term->osc_param == -1) {
		short i;

		term->osc_param = 0;
		for (i = 0; i < term->osc_len; i++)
			term->osc_param = term->osc_param * 10 +
			    (term->osc_buf[i] - '0');
		term->osc_len = 0;
		return;
	}

	/* Accumulate digits for numeric parameter */
	if (term->osc_param == -1) {
		if (ch >= '0' && ch <= '9') {
			if (term->osc_len < (short)(sizeof(term->osc_buf) - 1))
				term->osc_buf[term->osc_len++] = ch;
			return;
		}
		/* Unexpected byte in param area - discard */
		term->osc_buf[term->osc_len] = '\0';
		term->parse_state = PARSE_NORMAL;
		return;
	}

	/* Accumulate payload text */
	if (term->osc_len < (short)(sizeof(term->osc_buf) - 1))
		term->osc_buf[term->osc_len++] = ch;
}

/* ----------------------------------------------------------------
 * Base64 encode/decode for OSC 52 clipboard
 * ---------------------------------------------------------------- */

static const char b64_enc[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * b64_val - decode a single base64 character, -1 if invalid
 */
static short
b64_val(unsigned char ch)
{
	if (ch >= 'A' && ch <= 'Z') return ch - 'A';
	if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
	if (ch >= '0' && ch <= '9') return ch - '0' + 52;
	if (ch == '+') return 62;
	if (ch == '/') return 63;
	return -1;
}

/*
 * base64_decode - decode base64 data
 * Returns decoded length, or -1 on error.
 */
static short
base64_decode(const char *in, short in_len, char *out, short out_max)
{
	short i, o;
	short v0, v1, v2, v3;

	o = 0;
	for (i = 0; i + 3 < in_len; i += 4) {
		v0 = b64_val(in[i]);
		v1 = b64_val(in[i + 1]);
		if (v0 < 0 || v1 < 0)
			return -1;

		if (o < out_max)
			out[o++] = (char)((v0 << 2) | (v1 >> 4));

		if (in[i + 2] == '=')
			break;
		v2 = b64_val(in[i + 2]);
		if (v2 < 0)
			return -1;
		if (o < out_max)
			out[o++] = (char)((v1 << 4) | (v2 >> 2));

		if (in[i + 3] == '=')
			break;
		v3 = b64_val(in[i + 3]);
		if (v3 < 0)
			return -1;
		if (o < out_max)
			out[o++] = (char)((v2 << 6) | v3);
	}
	return o;
}

/*
 * base64_encode - encode data to base64
 * Returns encoded length (not null-terminated).
 */
static short
base64_encode(const char *in, short in_len, char *out, short out_max)
{
	short i, o;
	unsigned char a, b, c;

	o = 0;
	for (i = 0; i < in_len; i += 3) {
		a = in[i];
		b = (i + 1 < in_len) ? in[i + 1] : 0;
		c = (i + 2 < in_len) ? in[i + 2] : 0;

		if (o + 4 > out_max)
			break;

		out[o++] = b64_enc[a >> 2];
		out[o++] = b64_enc[((a & 0x03) << 4) | (b >> 4)];
		out[o++] = (i + 1 < in_len) ?
		    b64_enc[((b & 0x0F) << 2) | (c >> 6)] : '=';
		out[o++] = (i + 2 < in_len) ?
		    b64_enc[c & 0x3F] : '=';
	}
	return o;
}

/*
 * term_finish_osc - execute a completed OSC sequence
 */
static void
term_finish_osc(Terminal *term)
{
	short len;

	term->osc_buf[term->osc_len] = '\0';

	switch (term->osc_param) {
	case 0:
	case 2:
		/* Set window title */
		len = term->osc_len;
		if (len > (short)(sizeof(term->window_title) - 1))
			len = sizeof(term->window_title) - 1;
		memcpy(term->window_title, term->osc_buf, len);
		term->window_title[len] = '\0';
		term->title_changed = 1;
		break;
	case 1:
		/* Icon title only - ignore on Mac */
		break;
	case 4:
		/*
		 * OSC 4 - query/set palette color
		 * Query: ESC]4;<index>;?BEL → reply with rgb
		 * Set: silently ignore
		 * osc_buf contains "<index>;?" (semicolon after 4
		 * already consumed by OSC param parser)
		 */
		{
			short idx = 0;
			short i = 0;

			/* Parse color index from osc_buf */
			while (i < term->osc_len &&
			    term->osc_buf[i] >= '0' &&
			    term->osc_buf[i] <= '9') {
				idx = idx * 10 +
				    (term->osc_buf[i] - '0');
				i++;
			}
			/* Check for ";?" suffix = query */
			if (i < term->osc_len &&
			    term->osc_buf[i] == ';' &&
			    i + 1 < term->osc_len &&
			    term->osc_buf[i + 1] == '?' &&
			    idx >= 0 && idx <= 255) {
				RGBColor rgb;

				color_get_rgb((unsigned char)idx,
				    &rgb);
				term->response_len = snprintf(
				    term->response,
				    sizeof(term->response),
				    "\033]4;%d;rgb:%04x/%04x/"
				    "%04x\033\\",
				    idx,
				    (unsigned)rgb.red,
				    (unsigned)rgb.green,
				    (unsigned)rgb.blue);
			}
			/* Set (non-"?" payload): silently ignore */
		}
		break;
	case 10:
		/*
		 * OSC 10 - query/set default foreground color
		 * Query: "?" → reply with rgb
		 * Monochrome default: fg = black (0000/0000/0000)
		 * Dark mode: fg = white (ffff/ffff/ffff)
		 */
		if (term->osc_len == 1 && term->osc_buf[0] == '?') {
			if (term->dark_mode)
				term->response_len = snprintf(
				    term->response,
				    sizeof(term->response),
				    "\033]10;rgb:ffff/ffff/"
				    "ffff\033\\");
			else
				term->response_len = snprintf(
				    term->response,
				    sizeof(term->response),
				    "\033]10;rgb:0000/0000/"
				    "0000\033\\");
		}
		/* Set (non-"?" payload): silently ignore */
		break;
	case 11:
		/*
		 * OSC 11 - query/set default background color
		 * Query: "?" → reply with rgb
		 * Monochrome default: bg = white (ffff/ffff/ffff)
		 * Dark mode: bg = black (0000/0000/0000)
		 */
		if (term->osc_len == 1 && term->osc_buf[0] == '?') {
			if (term->dark_mode)
				term->response_len = snprintf(
				    term->response,
				    sizeof(term->response),
				    "\033]11;rgb:0000/0000/"
				    "0000\033\\");
			else
				term->response_len = snprintf(
				    term->response,
				    sizeof(term->response),
				    "\033]11;rgb:ffff/ffff/"
				    "ffff\033\\");
		}
		/* Set (non-"?" payload): silently ignore */
		break;
	case 12:
		/* OSC 12 - set cursor color: no-op (monochrome XOR) */
		break;
	case 112:
		/* OSC 112 - reset cursor color: no-op (monochrome XOR) */
		break;
	case 52:
		/*
		 * OSC 52 - clipboard access
		 * Format: <selection>;<base64-data>
		 * selection: c/s/p (all treated as clipboard on Mac)
		 * payload "?" = query clipboard, else set clipboard
		 */
		if (term->osc_len >= 2 && term->osc_buf[1] == ';') {
			char *payload = &term->osc_buf[2];
			short plen = term->osc_len - 2;

			if (plen == 1 && payload[0] == '?') {
				/* Query: read clipboard, base64 encode, reply */
				Handle h;
				long offset;
				long slen;

				h = NewHandle(0);
				if (h) {
					slen = GetScrap(h, 'TEXT', &offset);
					if (slen > 0) {
						short elen;
						short hdr;

						if (slen > 384)
							slen = 384;
						HLock(h);
						/* Build response: ESC]52;c;<b64>ESC\ */
						hdr = 6; /* \033]52;c; */
						term->response[0] = '\033';
						term->response[1] = ']';
						term->response[2] = '5';
						term->response[3] = '2';
						term->response[4] = ';';
						term->response[5] = 'c';
						term->response[6] = ';';
						hdr = 7;
						elen = base64_encode(*h,
						    (short)slen,
						    &term->response[hdr],
						    (short)(sizeof(term->response)
						    - hdr - 2));
						term->response[hdr + elen] =
						    '\033';
						term->response[hdr + elen + 1] =
						    '\\';
						term->response_len =
						    hdr + elen + 2;
						HUnlock(h);
					}
					DisposeHandle(h);
				}
			} else if (plen > 0) {
				/* Set: decode base64, put on clipboard */
				char dec_buf[384];
				short dlen;

				dlen = base64_decode(payload, plen,
				    dec_buf, (short)sizeof(dec_buf));
				if (dlen > 0) {
					ZeroScrap();
					PutScrap((long)dlen, 'TEXT',
					    dec_buf);
				}
			}
		}
		break;
	default:
		/* Other OSC sequences: silently discard */
		break;
	}
}

/*
 * term_switch_to_alt - switch to alternate screen buffer
 */
static void
term_switch_to_alt(Terminal *term)
{
	if (term->alt_active)
		return;

	/* Save main screen contents */
	memcpy(term->alt_screen, term->screen, sizeof(term->screen));
	memcpy(term->alt_line_attr, term->line_attr,
	    sizeof(term->line_attr));
	term->alt_cur_row = term->cur_row;
	term->alt_cur_col = term->cur_col;
	term->alt_cur_attr = term->cur_attr;
	term->alt_cur_fg = term->cur_fg;
	term->alt_cur_bg = term->cur_bg;
	term->alt_active = 1;

	/* Save color arrays to alt buffer (lazy-allocate on first use) */
	if (term->has_color && term->screen_color) {
		if (!term->alt_color) {
			long sc_size = (long)TERM_ROWS * TERM_COLS *
			    sizeof(CellColor);
			term->alt_color = (CellColor *)NewPtr(sc_size);
		}
		if (term->alt_color) {
			memcpy(term->alt_color, term->screen_color,
			    (long)TERM_ROWS * TERM_COLS *
			    sizeof(CellColor));
		}
	}

	/* Clear the screen and line attributes for alt buffer use */
	term_clear_region(term, 0, 0, term->active_rows - 1, term->active_cols - 1);
	memset(term->line_attr, 0, sizeof(term->line_attr));
	term->cur_row = 0;
	term->cur_col = 0;
	term->wrap_pending = 0;
	term_dirty_all(term);
}

/*
 * term_switch_to_main - switch back to main screen buffer
 */
static void
term_switch_to_main(Terminal *term)
{
	if (!term->alt_active)
		return;

	/* Restore main screen contents */
	memcpy(term->screen, term->alt_screen, sizeof(term->screen));
	memcpy(term->line_attr, term->alt_line_attr,
	    sizeof(term->line_attr));
	term->cur_row = term->alt_cur_row;
	term->cur_col = term->alt_cur_col;
	term->cur_attr = term->alt_cur_attr;
	term->cur_fg = term->alt_cur_fg;
	term->cur_bg = term->alt_cur_bg;
	term->alt_active = 0;
	term->wrap_pending = 0;

	/* Restore color arrays from alt buffer */
	if (term->has_color && term->screen_color && term->alt_color) {
		memcpy(term->screen_color, term->alt_color,
		    (long)TERM_ROWS * TERM_COLS * sizeof(CellColor));
	}

	term_dirty_all(term);
}

/* ----------------------------------------------------------------
 * UTF-8 decode and Unicode translation
 * ---------------------------------------------------------------- */

/*
 * utf8_decode - extract Unicode codepoint from accumulated UTF-8 bytes
 */
static long
utf8_decode(unsigned char *buf, short len)
{
	long cp;

	switch (len) {
	case 2:
		if ((buf[0] & 0xE0) != 0xC0 || (buf[1] & 0xC0) != 0x80)
			return '?';
		cp = ((long)(buf[0] & 0x1F) << 6) | (buf[1] & 0x3F);
		if (cp < 0x80) return '?';  /* overlong */
		break;
	case 3:
		if ((buf[0] & 0xF0) != 0xE0 || (buf[1] & 0xC0) != 0x80 ||
		    (buf[2] & 0xC0) != 0x80)
			return '?';
		cp = ((long)(buf[0] & 0x0F) << 12) |
		     ((long)(buf[1] & 0x3F) << 6) | (buf[2] & 0x3F);
		if (cp < 0x800) return '?';  /* overlong */
		if (cp >= 0xD800 && cp <= 0xDFFF) return '?';  /* surrogate */
		break;
	case 4:
		if ((buf[0] & 0xF8) != 0xF0 || (buf[1] & 0xC0) != 0x80 ||
		    (buf[2] & 0xC0) != 0x80 || (buf[3] & 0xC0) != 0x80)
			return '?';
		cp = ((long)(buf[0] & 0x07) << 18) |
		     ((long)(buf[1] & 0x3F) << 12) |
		     ((long)(buf[2] & 0x3F) << 6) | (buf[3] & 0x3F);
		if (cp < 0x10000 || cp > 0x10FFFF) return '?';  /* overlong/invalid */
		break;
	default:
		return '?';
	}

	return cp;
}

/*
 * boxdraw_to_dec - map Unicode box-drawing codepoint to DEC Special Graphics
 */
static unsigned char
boxdraw_to_dec(unsigned short cp)
{
	/*
	 * Only heavy variants remain here — light and double-line
	 * box drawing are now handled by glyph tables in glyphs.c,
	 * and block elements by block_element_glyphs[].
	 * glyph_lookup() is called first in term_put_unicode().
	 */
	switch (cp) {
	/* Heavy variants (GLYPH_NONE in box_drawing_glyphs[]) */
	case 0x2501: return 'q';
	case 0x2503: return 'x';
	case 0x250F: return 'l';
	case 0x2513: return 'k';
	case 0x2517: return 'm';
	case 0x251B: return 'j';
	case 0x2523: return 't';
	case 0x252B: return 'u';
	case 0x2533: return 'w';
	case 0x253B: return 'v';
	case 0x254B: return 'n';
	default:     return 0;
	}
}

/* Index: codepoint - 0x80. Value: Mac Roman byte, or 0 if unmapped. */
static const unsigned char latin1_to_macroman[128] = {
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,		/* 0x80-0x8F: C1 */
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,		/* 0x90-0x9F: C1 */
	0xCA,0xC1,0xA2,0xA3, 0xB4,0xB4,0x7C,0xA4,	/* 0xA0-0xA7 */
	0xAC,0xA9,0xBB,0xC7, 0xC2,0,0xA8,0xF8,	/* 0xA8-0xAF */
	0xA1,0xB1,0,0, 0xAB,0xB5,0xA6,0xE1,		/* 0xB0-0xB7 */
	0xFC,0,0xBC,0xC8, 0,0,0,0xC0,			/* 0xB8-0xBF */
	0xCB,0xE7,0xE5,0xCC, 0x80,0x81,0xAE,0x82,	/* 0xC0-0xC7 */
	0xE9,0x83,0xE6,0xE8, 0xED,0xEA,0xEB,0xEC,	/* 0xC8-0xCF */
	0,0x84,0xF1,0xEE, 0xEF,0xCD,0x85,0,		/* 0xD0-0xD7 */
	0xAF,0xF4,0xF2,0xF3, 0x86,0,0,0xA7,		/* 0xD8-0xDF */
	0x88,0x87,0x89,0x8B, 0x8A,0x8C,0xBE,0x8D,	/* 0xE0-0xE7 */
	0x8F,0x8E,0x90,0x91, 0x93,0x92,0x94,0x95,	/* 0xE8-0xEF */
	0,0x96,0x98,0x97, 0x99,0x9B,0x9A,0xD6,		/* 0xF0-0xF7 */
	0xBF,0x9D,0x9C,0x9E, 0x9F,0,0,0xD8,		/* 0xF8-0xFF */
};

/*
 * unicode_to_macroman - translate U+0080-U+00FF to Mac Roman
 */
static unsigned char
unicode_to_macroman(unsigned short cp)
{
	if (cp < 0x80 || cp > 0xFF)
		return 0;
	return latin1_to_macroman[cp - 0x80];
}

/*
 * unicode_symbol_to_macroman - translate common Unicode symbols to Mac Roman
 */
static unsigned char
unicode_symbol_to_macroman(unsigned short cp)
{
	switch (cp) {
	case 0x2013: return 0xD0;	/* en dash */
	case 0x2014: return 0xD1;	/* em dash */
	case 0x2018: return 0xD4;	/* left single quote */
	case 0x2019: return 0xD5;	/* right single quote */
	case 0x201C: return 0xD2;	/* left double quote */
	case 0x201D: return 0xD3;	/* right double quote */
	case 0x2022: return 0xA5;	/* bullet */
	case 0x2026: return 0xC9;	/* ellipsis */
	case 0x2122: return 0xAA;	/* trademark */
	case 0x20AC: return 0xDB;	/* euro sign */
	case 0x2260: return 0xAD;	/* not equal */
	case 0x2264: return 0xB2;	/* less or equal */
	case 0x2265: return 0xB3;	/* greater or equal */
	case 0x03C0: return 0xB9;	/* pi */
	case 0x2206: return 0xC6;	/* delta */
	case 0x2219: return 0xE1;	/* bullet operator -> middle dot */
	case 0x22C5: return 0xE1;	/* dot operator -> middle dot */
	case 0x0387: return 0xE1;	/* greek ano teleia -> middle dot */
	case 0x2027: return 0xE1;	/* hyphenation point -> middle dot */
	default:     return 0;
	}
}

/*
 * term_put_unicode - translate a Unicode codepoint and output to terminal
 */
static void
term_put_unicode(Terminal *term, long cp)
{
	unsigned char ch;

	/* Invisible/formatting Unicode: silently absorb */
	if (cp == 0x00AD ||			/* soft hyphen */
	    (cp >= 0x200B && cp <= 0x200F) ||	/* ZWSP,ZWNJ,ZWJ,LRM,RLM */
	    (cp >= 0x2028 && cp <= 0x202E) ||	/* line/para sep, bidi */
	    (cp >= 0x2060 && cp <= 0x2069) ||	/* word joiner, bidi iso */
	    (cp >= 0xFE00 && cp <= 0xFE0F) ||	/* variation selectors */
	    cp == 0xFEFF ||			/* BOM / ZWNBSP */
	    (cp >= 0xE0001 && cp <= 0xE007F))	/* tag characters */
		return;

	/* Emoji modifier absorption (skin tones, keycaps) */
	if (term->last_was_emoji) {
		if ((cp >= 0x1F3FB && cp <= 0x1F3FF) ||	/* skin tones */
		    cp == 0x20E3) {			/* enclosing keycap */
			return;		/* absorb silently */
		}
		term->last_was_emoji = 0;
	}

	/* Braille patterns: U+2800-U+28FF */
	if (cp >= 0x2800 && cp <= 0x28FF) {
		term_put_glyph(term, (unsigned char)(cp & 0xFF),
		    CELL_TYPE_BRAILLE);
		return;
	}

	/* Custom glyph lookup (symbols and emoji) */
	{
		short glyph_id;

		glyph_id = glyph_lookup(cp);
		if (glyph_id >= 0) {
			term_put_glyph(term, (unsigned char)glyph_id,
			    CELL_TYPE_GLYPH);
			if (glyph_id >= GLYPH_EMOJI_BASE)
				term->last_was_emoji = 1;
			return;
		}
	}

	/* Latin-1 Supplement: direct Mac Roman mapping */
	if (cp >= 0x80 && cp <= 0xFF) {
		ch = unicode_to_macroman((unsigned short)cp);
		if (ch)
			term_put_char(term, ch);
		else
			term_put_char(term, '?');	/* fallback */
		return;
	}

	/* Box-drawing: U+2500-U+257F -> DEC Special Graphics */
	if (cp >= 0x2500 && cp <= 0x257F) {
		ch = boxdraw_to_dec((unsigned short)cp);
		if (ch) {
			unsigned char save_g0 = term->g0_charset;
			unsigned char save_gl = term->gl_charset;
			term->g0_charset = '0';
			term->gl_charset = 0;
			term_put_char(term, ch);
			term->g0_charset = save_g0;
			term->gl_charset = save_gl;
		} else {
			term_put_char(term, '-');
		}
		return;
	}

	/* Unicode spaces -> regular ASCII space */
	if ((cp >= 0x2000 && cp <= 0x200A) ||	/* en/em/thin/hair space */
	    cp == 0x205F ||			/* medium math space */
	    cp == 0x3000) {			/* ideographic space */
		term_put_char(term, ' ');
		return;
	}

	/* Common Unicode symbols -> Mac Roman */
	if ((cp >= 0x2000 && cp <= 0x22FF) || cp == 0x03C0 ||
	    cp == 0x20AC || cp == 0x0387) {
		ch = unicode_symbol_to_macroman((unsigned short)cp);
		if (ch) {
			term_put_char(term, ch);
			return;
		}
	}

	/* Legacy Computing (U+1FB00-U+1FBFF): single-width fallback */
	if (cp >= 0x1FB00L && cp <= 0x1FBFFL) {
		term_put_char(term, '?');
		return;
	}

	/* Wide characters (CJK, emoji): two-cell placeholder */
	if ((cp >= 0x2E80 && cp <= 0x9FFF) ||		/* CJK */
	    (cp >= 0xF900 && cp <= 0xFAFF) ||		/* CJK compat */
	    (cp >= 0xFE30 && cp <= 0xFE6F) ||		/* CJK forms */
	    (cp >= 0x1F000)) {				/* emoji */
		term_put_char(term, '?');
		term_put_char(term, '?');
		if (cp >= 0x1F000)
			term->last_was_emoji = 1;
		return;
	}

	/* Everything else: fallback */
	term_put_char(term, '?');
}
