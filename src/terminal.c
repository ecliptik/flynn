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
#include "terminal.h"

/* Forward declarations for internal functions */
static void term_clear_region(Terminal *term, short r1, short c1,
    short r2, short c2);
static void term_scroll_up(Terminal *term, short top, short bottom,
    short count);
static void term_scroll_down(Terminal *term, short top, short bottom,
    short count);
static void term_put_char(Terminal *term, unsigned char ch);
static void term_newline(Terminal *term);
static void term_carriage_return(Terminal *term);
static void term_process_esc(Terminal *term, unsigned char ch);
static void term_process_csi(Terminal *term, unsigned char ch);
static void term_execute_csi(Terminal *term, unsigned char cmd);
static void term_set_attr(Terminal *term, short param);
static void term_dirty_row(Terminal *term, short row);
static void term_dirty_range(Terminal *term, short r1, short r2);
static void term_dirty_all(Terminal *term);
static short term_clamp(short val, short lo, short hi);

#include <OSUtils.h>

/*
 * terminal_init - set up terminal to power-on defaults
 */
void
terminal_init(Terminal *term)
{
	memset(term, 0, sizeof(Terminal));
	term->scroll_top = 0;
	term->scroll_bottom = TERM_ROWS - 1;
	term->cur_attr = ATTR_NORMAL;
	term->parse_state = PARSE_NORMAL;

	/* Fill screen with spaces */
	term_clear_region(term, 0, 0, TERM_ROWS - 1, TERM_COLS - 1);
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
			if (ch == 0x1B) {
				/* ESC */
				term->parse_state = PARSE_ESC;
			} else if (ch == '\r') {
				term_carriage_return(term);
			} else if (ch == '\n' || ch == 0x0B || ch == 0x0C) {
				/* LF, VT, FF all treated as newline */
				term_newline(term);
			} else if (ch == '\b') {
				/* Backspace: move cursor left one */
				if (term->cur_col > 0) {
					term->cur_col--;
					term->wrap_pending = 0;
				}
			} else if (ch == '\t') {
				/* Tab: advance to next 8-column tab stop */
				term->wrap_pending = 0;
				term->cur_col = (term->cur_col + 8) & ~7;
				if (term->cur_col >= TERM_COLS)
					term->cur_col = TERM_COLS - 1;
			} else if (ch == 0x07) {
				/* Bell */
				SysBeep(5);
			} else if (ch >= 0x20) {
				/* Printable character */
				term_put_char(term, ch);
			}
			/* Characters 0x00-0x06, 0x0E-0x1A, 0x1C-0x1F ignored */
			break;

		case PARSE_ESC:
			term_process_esc(term, ch);
			break;

		case PARSE_CSI:
		case PARSE_CSI_PARAM:
		case PARSE_CSI_INTERMEDIATE:
			term_process_csi(term, ch);
			break;
		}
	}
}

/*
 * terminal_get_cell - return pointer to cell at (row, col)
 */
TermCell *
terminal_get_cell(Terminal *term, short row, short col)
{
	if (row < 0 || row >= TERM_ROWS || col < 0 || col >= TERM_COLS)
		return &term->screen[0][0];

	return &term->screen[row][col];
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
	if (row >= 0 && row < TERM_ROWS)
		term->dirty[row] = 1;
}

/*
 * term_dirty_range - mark a range of rows dirty (inclusive)
 */
static void
term_dirty_range(Terminal *term, short r1, short r2)
{
	short r;

	r1 = term_clamp(r1, 0, TERM_ROWS - 1);
	r2 = term_clamp(r2, 0, TERM_ROWS - 1);
	for (r = r1; r <= r2; r++)
		term->dirty[r] = 1;
}

/*
 * term_dirty_all - mark entire screen dirty
 */
static void
term_dirty_all(Terminal *term)
{
	term_dirty_range(term, 0, TERM_ROWS - 1);
}

/*
 * term_clear_region - fill a rectangular region with spaces and
 *                     current attributes
 */
static void
term_clear_region(Terminal *term, short r1, short c1, short r2, short c2)
{
	short r, c;

	r1 = term_clamp(r1, 0, TERM_ROWS - 1);
	r2 = term_clamp(r2, 0, TERM_ROWS - 1);
	c1 = term_clamp(c1, 0, TERM_COLS - 1);
	c2 = term_clamp(c2, 0, TERM_COLS - 1);

	for (r = r1; r <= r2; r++) {
		for (c = c1; c <= c2; c++) {
			term->screen[r][c].ch = ' ';
			term->screen[r][c].attr = ATTR_NORMAL;
		}
		term_dirty_row(term, r);
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
	    TERM_COLS * sizeof(TermCell));

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

	/* Save lines scrolled off top into scrollback */
	if (top == 0) {
		for (r = 0; r < count; r++)
			term_save_scrollback(term, r);
	}

	/* Move lines up */
	for (r = top; r <= bottom - count; r++) {
		memcpy(term->screen[r], term->screen[r + count],
		    TERM_COLS * sizeof(TermCell));
	}

	/* Clear newly exposed lines at bottom */
	for (r = bottom - count + 1; r <= bottom; r++) {
		for (c = 0; c < TERM_COLS; c++) {
			term->screen[r][c].ch = ' ';
			term->screen[r][c].attr = ATTR_NORMAL;
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

	/* Move lines down */
	for (r = bottom; r >= top + count; r--) {
		memcpy(term->screen[r], term->screen[r - count],
		    TERM_COLS * sizeof(TermCell));
	}

	/* Clear newly exposed lines at top */
	for (r = top; r < top + count; r++) {
		for (c = 0; c < TERM_COLS; c++) {
			term->screen[r][c].ch = ' ';
			term->screen[r][c].attr = ATTR_NORMAL;
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
	/* If wrap is pending, do the wrap now */
	if (term->wrap_pending) {
		term->wrap_pending = 0;
		term_carriage_return(term);
		term_newline(term);
	}

	term->screen[term->cur_row][term->cur_col].ch = ch;
	term->screen[term->cur_row][term->cur_col].attr = term->cur_attr;
	term_dirty_row(term, term->cur_row);

	if (term->cur_col < TERM_COLS - 1) {
		term->cur_col++;
	} else {
		/* At right margin: set wrap pending for auto-wrap */
		term->wrap_pending = 1;
	}
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
	} else if (term->cur_row < TERM_ROWS - 1) {
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
		/* Save cursor position and attributes */
		term->saved_row = term->cur_row;
		term->saved_col = term->cur_col;
		term->saved_attr = term->cur_attr;
		break;

	case '8':
		/* Restore cursor position and attributes */
		term->cur_row = term_clamp(term->saved_row, 0, TERM_ROWS - 1);
		term->cur_col = term_clamp(term->saved_col, 0, TERM_COLS - 1);
		term->cur_attr = term->saved_attr;
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

	case 'c':
		/* Full reset (RIS) */
		terminal_reset(term);
		return;

	case '(':
	case ')':
	case '#':
		/*
		 * Character set designation / DEC double-width etc.
		 * These consume one more byte which we simply discard.
		 * Stay in ESC state to eat that byte, then return to
		 * NORMAL.  We use intermediate to track this.
		 */
		term->intermediate = ch;
		/* Stay in PARSE_ESC; next byte will fall through default */
		return;

	default:
		/*
		 * If we stored an intermediate from ESC ( / ) / # above,
		 * this is the consumed follow-up byte.  Otherwise it is
		 * an unrecognised sequence.  Either way, return to normal.
		 */
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
			if (term->num_params > 0) {
				short idx = term->num_params - 1;
				term->params[idx] = term->params[idx] * 10 +
				    (ch - '0');
			}
		}
		/* Digits after intermediate are invalid; ignore */
		return;
	}

	if (ch == ';') {
		/* Parameter separator */
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

	if (ch == '?') {
		/* Private mode introducer (e.g. ESC[?25h for cursor) */
		term->intermediate = '?';
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
		if (term->cur_row >= TERM_ROWS)
			term->cur_row = TERM_ROWS - 1;
		break;

	case 'C':
		/* CUF - cursor forward (right) */
		term->wrap_pending = 0;
		term->cur_col += p1;
		if (term->cur_col >= TERM_COLS)
			term->cur_col = TERM_COLS - 1;
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
		term->cur_row = term_clamp(p1 - 1, 0, TERM_ROWS - 1);
		term->cur_col = term_clamp(p2 - 1, 0, TERM_COLS - 1);
		break;

	case 'J':
		/* ED - erase in display */
		p1 = (term->num_params >= 1) ? term->params[0] : 0;
		switch (p1) {
		case 0:
			/* Cursor to end of screen */
			term_clear_region(term, term->cur_row, term->cur_col,
			    term->cur_row, TERM_COLS - 1);
			if (term->cur_row < TERM_ROWS - 1)
				term_clear_region(term, term->cur_row + 1, 0,
				    TERM_ROWS - 1, TERM_COLS - 1);
			break;
		case 1:
			/* Start of screen to cursor */
			if (term->cur_row > 0)
				term_clear_region(term, 0, 0,
				    term->cur_row - 1, TERM_COLS - 1);
			term_clear_region(term, term->cur_row, 0,
			    term->cur_row, term->cur_col);
			break;
		case 2:
			/* Entire screen */
			term_clear_region(term, 0, 0,
			    TERM_ROWS - 1, TERM_COLS - 1);
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
			    term->cur_row, TERM_COLS - 1);
			break;
		case 1:
			/* Start of line to cursor */
			term_clear_region(term, term->cur_row, 0,
			    term->cur_row, term->cur_col);
			break;
		case 2:
			/* Entire line */
			term_clear_region(term, term->cur_row, 0,
			    term->cur_row, TERM_COLS - 1);
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
		p1 = term_clamp(p1, 1, TERM_COLS - term->cur_col);
		for (c = TERM_COLS - 1; c >= term->cur_col + p1; c--) {
			term->screen[term->cur_row][c] =
			    term->screen[term->cur_row][c - p1];
		}
		for (c = term->cur_col; c < term->cur_col + p1; c++) {
			term->screen[term->cur_row][c].ch = ' ';
			term->screen[term->cur_row][c].attr = ATTR_NORMAL;
		}
		term_dirty_row(term, term->cur_row);
		break;

	case 'P':
		/* DCH - delete characters at cursor, shifting left */
		p1 = term_clamp(p1, 1, TERM_COLS - term->cur_col);
		for (c = term->cur_col; c < TERM_COLS - p1; c++) {
			term->screen[term->cur_row][c] =
			    term->screen[term->cur_row][c + p1];
		}
		for (c = TERM_COLS - p1; c < TERM_COLS; c++) {
			term->screen[term->cur_row][c].ch = ' ';
			term->screen[term->cur_row][c].attr = ATTR_NORMAL;
		}
		term_dirty_row(term, term->cur_row);
		break;

	case 'X':
		/* ECH - erase characters at cursor (don't move cursor) */
		p1 = term_clamp(p1, 1, TERM_COLS - term->cur_col);
		for (c = term->cur_col; c < term->cur_col + p1; c++) {
			term->screen[term->cur_row][c].ch = ' ';
			term->screen[term->cur_row][c].attr = ATTR_NORMAL;
		}
		term_dirty_row(term, term->cur_row);
		break;

	case 'r':
		/* DECSTBM - set scroll region (top;bottom, 1-based) */
		p1 = (term->num_params >= 1 && term->params[0] > 0) ?
		    term->params[0] : 1;
		p2 = (term->num_params >= 2 && term->params[1] > 0) ?
		    term->params[1] : TERM_ROWS;
		term->scroll_top = term_clamp(p1 - 1, 0, TERM_ROWS - 1);
		term->scroll_bottom = term_clamp(p2 - 1, 0, TERM_ROWS - 1);
		if (term->scroll_top >= term->scroll_bottom) {
			/* Invalid: reset to full screen */
			term->scroll_top = 0;
			term->scroll_bottom = TERM_ROWS - 1;
		}
		/* Cursor moves to home after setting scroll region */
		term->cur_row = 0;
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
			for (i = 0; i < term->num_params; i++)
				term_set_attr(term, term->params[i]);
		}
		break;

	case 'd':
		/* VPA - line position absolute (row, 1-based) */
		term->wrap_pending = 0;
		term->cur_row = term_clamp(p1 - 1, 0, TERM_ROWS - 1);
		break;

	case 'G':
	case '`':
		/* CHA / HPA - cursor character absolute (column, 1-based) */
		term->wrap_pending = 0;
		term->cur_col = term_clamp(p1 - 1, 0, TERM_COLS - 1);
		break;

	case 'E':
		/* CNL - cursor next line */
		term->wrap_pending = 0;
		term->cur_row += p1;
		if (term->cur_row >= TERM_ROWS)
			term->cur_row = TERM_ROWS - 1;
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
		/* SM - set mode (we handle ?25h to show cursor, etc.) */
		/* Currently no modes tracked; placeholder for future use */
		break;

	case 'l':
		/* RM - reset mode (we handle ?25l to hide cursor, etc.) */
		/* Currently no modes tracked; placeholder for future use */
		break;

	case 'c':
		/* DA - device attributes request; ignore for now */
		/* A proper response would be sent back over the connection */
		break;

	case 'n':
		/* DSR - device status report; ignore for now */
		/* Response (cursor position report) requires send path */
		break;

	default:
		/* Unrecognised CSI sequence: silently ignore */
		break;
	}
}

/*
 * term_set_attr - apply a single SGR parameter
 */
static void
term_set_attr(Terminal *term, short param)
{
	switch (param) {
	case 0:
		/* Reset all attributes */
		term->cur_attr = ATTR_NORMAL;
		break;
	case 1:
		/* Bold */
		term->cur_attr |= ATTR_BOLD;
		break;
	case 4:
		/* Underline */
		term->cur_attr |= ATTR_UNDERLINE;
		break;
	case 7:
		/* Inverse / reverse video */
		term->cur_attr |= ATTR_INVERSE;
		break;
	case 22:
		/* Normal intensity (un-bold) */
		term->cur_attr &= ~ATTR_BOLD;
		break;
	case 24:
		/* Not underlined */
		term->cur_attr &= ~ATTR_UNDERLINE;
		break;
	case 27:
		/* Not inverse */
		term->cur_attr &= ~ATTR_INVERSE;
		break;
	default:
		/*
		 * Ignore colour codes (30-37, 40-47, 38, 48, 90-97,
		 * 100-107) and anything else we don't support.
		 * This is a monochrome terminal.
		 */
		break;
	}
}
