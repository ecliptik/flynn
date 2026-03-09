/*
 * terminal.h - VT100 terminal emulation engine
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

#ifndef TERMINAL_H
#define TERMINAL_H

/* Maximum screen dimensions (buffer size) */
#define TERM_COLS		132
#define TERM_ROWS		50

/* Default active dimensions */
#define TERM_DEFAULT_COLS	80
#define TERM_DEFAULT_ROWS	24

/* Scrollback: 4 pages of default rows */
#define TERM_SCROLLBACK_LINES	(TERM_DEFAULT_ROWS * 4)

/* Maximum CSI parameters per sequence */
#define TERM_MAX_PARAMS		16

/* Character attributes (packed into unsigned char) */
#define ATTR_NORMAL		0x00
#define ATTR_BOLD		0x01
#define ATTR_UNDERLINE		0x02
#define ATTR_INVERSE		0x04
#define ATTR_DEC_GRAPHICS	0x08
#define ATTR_GLYPH		0x10	/* ch is a glyph index */
#define ATTR_BRAILLE		0x20	/* ch is braille dot pattern */

/* Parser states */
#define PARSE_NORMAL		0
#define PARSE_ESC		1	/* got ESC */
#define PARSE_CSI		2	/* got ESC[ */
#define PARSE_CSI_PARAM		3	/* reading CSI numeric params */
#define PARSE_CSI_INTERMEDIATE	4	/* got intermediate byte(s) */
#define PARSE_OSC		5	/* got ESC] */
#define PARSE_OSC_ESC		6	/* got ESC inside OSC, looking for \ */
#define PARSE_DCS		7	/* got ESC P, consume until ST */
#define PARSE_DCS_ESC		8	/* got ESC inside DCS, looking for \ */

/* A single terminal cell */
typedef struct {
	unsigned char	ch;		/* character */
	unsigned char	attr;		/* attribute flags */
} TermCell;

/*
 * Terminal state
 *
 * Struct layout is optimized for 68000 addressing modes:
 * frequently-accessed fields are placed FIRST (within 16-bit
 * displacement of the base pointer), large arrays are LAST.
 * The 68000 uses faster 16-bit displacement for offsets < 32768
 * vs slower 32-bit displacement for larger offsets.
 */
typedef struct {
	/* --- Hot fields: cursor, attributes, parser (offset < 200) --- */

	/* Cursor position */
	short		cur_row;
	short		cur_col;
	unsigned char	cur_attr;

	/* Active grid dimensions (may be < max when using larger font) */
	short		active_cols;
	short		active_rows;

	/* Parser state */
	unsigned char	parse_state;
	short		num_params;
	short		params[TERM_MAX_PARAMS];
	unsigned char	intermediate;	/* intermediate byte (e.g. '?' or '#') */

	/* Auto-wrap state */
	unsigned char	wrap_pending;	/* cursor at right margin, next char wraps */

	/* Character sets */
	unsigned char	g0_charset;	/* 'B' = US ASCII, '0' = DEC Special Graphics */
	unsigned char	g1_charset;	/* 'B' = US ASCII, '0' = DEC Special Graphics */
	unsigned char	gl_charset;	/* GL points to: 0 = G0, 1 = G1 */

	/* Scroll region (top and bottom, inclusive, 0-based) */
	short		scroll_top;
	short		scroll_bottom;

	/* Dirty flags: one per row, nonzero means row needs redraw */
	unsigned char	dirty[TERM_ROWS];

	/* Scrollback state */
	short		sb_head;	/* next line to write in ring */
	short		sb_count;	/* number of valid lines */
	short		scroll_offset;	/* 0 = live, >0 = scrolled back N lines */

	/* --- Warm fields: modes, saved state (offset < 400) --- */

	/* DEC modes */
	unsigned char	cursor_key_mode;	/* DECCKM: 0=normal, 1=application */
	unsigned char	keypad_mode;		/* DECKPAM: 0=normal, 1=application */
	unsigned char	autowrap;		/* DECAWM: 1=on (default), 0=off */
	unsigned char	origin_mode;		/* DECOM: 0=absolute, 1=relative */
	unsigned char	insert_mode;		/* IRM: 0=replace, 1=insert */
	unsigned char	bracketed_paste;	/* 0=off, 1=on */

	/* Cursor visibility (DECTCEM: ESC[?25h / ESC[?25l) */
	unsigned char	cursor_visible;

	/* Alternate screen state */
	short		alt_cur_row;
	short		alt_cur_col;
	unsigned char	alt_cur_attr;
	unsigned char	alt_active;

	/* Saved cursor (ESC 7 / ESC 8) */
	short		saved_row;
	short		saved_col;
	unsigned char	saved_attr;
	unsigned char	saved_g0_charset;
	unsigned char	saved_g1_charset;
	unsigned char	saved_gl_charset;
	unsigned char	saved_origin_mode;
	unsigned char	saved_autowrap;

	/* Response buffer for DA/DSR replies */
	char		response[32];
	short		response_len;

	/* OSC buffer */
	char		osc_buf[128];
	short		osc_len;
	short		osc_param;

	/* OSC window title */
	unsigned char	title_changed;
	char		window_title[64];

	/* UTF-8 decoder state */
	unsigned char	utf8_buf[4];	/* accumulator for multi-byte sequence */
	unsigned char	utf8_len;	/* bytes accumulated so far */
	unsigned char	utf8_expect;	/* total bytes expected (2, 3, or 4) */
	unsigned char	last_was_emoji;	/* absorb following modifiers */

	/* --- Large arrays LAST: pushed beyond hot-path offsets --- */

	/* Screen buffer: rows x cols (13,200 bytes) */
	TermCell	screen[TERM_ROWS][TERM_COLS];

	/* Alternate screen buffer (13,200 bytes) */
	TermCell	alt_screen[TERM_ROWS][TERM_COLS];

	/* Scrollback ring buffer (25,344 bytes) */
	TermCell	scrollback[TERM_SCROLLBACK_LINES][TERM_COLS];
} Terminal;

/* Initialize terminal to default state */
void terminal_init(Terminal *term);

/* Feed raw bytes into the terminal engine */
void terminal_process(Terminal *term, unsigned char *data, short len);

/* Reset terminal to initial state */
void terminal_reset(Terminal *term);

/* Get cell at given position (returns pointer into screen buffer) */
TermCell *terminal_get_cell(Terminal *term, short row, short col);

/* Get display cell accounting for scroll_offset (for rendering) */
TermCell *terminal_get_display_cell(Terminal *term, short row, short col);

/* Scroll back N lines into scrollback buffer */
void terminal_scroll_back(Terminal *term, short lines);

/* Scroll forward N lines toward live view */
void terminal_scroll_forward(Terminal *term, short lines);

/* Check if a row is dirty (needs redraw) */
short terminal_is_row_dirty(Terminal *term, short row);

/* Clear all dirty flags */
void terminal_clear_dirty(Terminal *term);

/* Get current cursor position */
void terminal_get_cursor(Terminal *term, short *row, short *col);

#endif /* TERMINAL_H */
