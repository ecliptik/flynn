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

#include "color.h"


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
#define ATTR_ITALIC		0x20	/* true italic rendering */
#define ATTR_HAS_COLOR		0x40	/* cell has non-default color */
#define ATTR_STRIKETHROUGH	0x80	/* strikethrough rendering */

/* Cell type encoding (bits 3-4): mutually exclusive */
#define CELL_TYPE_MASK		0x18	/* bits 3-4 */
#define CELL_TYPE_NORMAL	0x00	/* 00: regular character */
#define CELL_TYPE_DEC		0x08	/* 01: DEC Special Graphics */
#define CELL_TYPE_GLYPH		0x10	/* 10: glyph index */
#define CELL_TYPE_BRAILLE	0x18	/* 11: braille dot pattern */

#define CELL_IS_NORMAL(a)	(((a) & CELL_TYPE_MASK) == CELL_TYPE_NORMAL)
#define CELL_IS_DEC(a)		(((a) & CELL_TYPE_MASK) == CELL_TYPE_DEC)
#define CELL_IS_GLYPH(a)	(((a) & CELL_TYPE_MASK) == CELL_TYPE_GLYPH)
#define CELL_IS_BRAILLE(a)	(((a) & CELL_TYPE_MASK) == CELL_TYPE_BRAILLE)

/* Line attributes (per-row) */
#define LINE_ATTR_NORMAL    0
#define LINE_ATTR_DBLW      1   /* DECDWL: double-width */
#define LINE_ATTR_DBLH_TOP  2   /* DECDHL top half */
#define LINE_ATTR_DBLH_BOT  3   /* DECDHL bottom half */

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

	/* Scroll hint for ScrollRect optimization */
	short		scroll_pending;	/* 1 if hint is valid */
	short		scroll_dir;	/* 1=up, -1=down */
	short		scroll_count;	/* accumulated line count */
	short		scroll_rgn_top;	/* scroll region top for hint */
	short		scroll_rgn_bot;	/* scroll region bottom for hint */

	/* Dirty flags: one per row, nonzero means row needs redraw */
	unsigned char	dirty[TERM_ROWS];

	/* Line attributes: one per row */
	unsigned char	line_attr[TERM_ROWS];	/* LINE_ATTR_* per row */

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
	unsigned char	cp437_mode;		/* 0=UTF-8/VT220, 1=CP437/ANSI-BBS */
	unsigned char	tab_stops[TERM_COLS];	/* custom tab stops, 1=set */

	/* Cursor visibility (DECTCEM: ESC[?25h / ESC[?25l) */
	unsigned char	cursor_visible;
	unsigned char	cursor_style;		/* DECSCUSR: 0-1=blink block, 2=steady block, 3=blink underline, 4=steady underline, 5=blink bar, 6=steady bar */

	/* Alternate screen state */
	short		alt_cur_row;
	short		alt_cur_col;
	unsigned char	alt_cur_attr;
	unsigned char	alt_active;
	unsigned char	alt_line_attr[TERM_ROWS];	/* saved line attrs for main screen */
	unsigned char	alt_cur_fg;	/* alt screen saved fg */
	unsigned char	alt_cur_bg;	/* alt screen saved bg */

	/* Color state (System 7 only, zero-cost on System 6) */
	unsigned char	has_color;	/* runtime: Color QD available + alloc'd */
	unsigned char	cur_fg;		/* current fg (COLOR_DEFAULT = default) */
	unsigned char	cur_bg;		/* current bg (COLOR_DEFAULT = default) */
	unsigned char	pre_dim_fg;	/* saved fg before SGR 2 dim */
	unsigned char	dark_mode;	/* mirror of prefs.dark_mode for OSC queries */

	/* Saved cursor (ESC 7 / ESC 8) */
	short		saved_row;
	short		saved_col;
	unsigned char	saved_attr;
	unsigned char	saved_fg;	/* saved cursor fg color */
	unsigned char	saved_bg;	/* saved cursor bg color */
	unsigned char	saved_g0_charset;
	unsigned char	saved_g1_charset;
	unsigned char	saved_gl_charset;
	unsigned char	saved_origin_mode;
	unsigned char	saved_autowrap;

	/* Response buffer for DA/DSR replies */
	char		response[256];
	short		response_len;

	/* Connection for immediate response flush (NULL if disconnected) */
	void		*resp_conn;

	/* OSC buffer */
	char		osc_buf[512];
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

	/* Screen snapshot for disconnect recovery (saved on full clear) */
	short		snap_valid;	/* 1 if snap_screen has data */
	short		snap_rows;	/* active rows at snapshot time */
	short		snap_cols;	/* active cols at snapshot time */

	/* --- Color arrays: dynamically allocated on System 7 only --- */
	CellColor	*screen_color;	/* NULL on System 6 */
	CellColor	*alt_color;	/* NULL on System 6, lazy alloc */
	CellColor	*sb_color;	/* NULL on System 6, lazy alloc */

	/* --- Large arrays LAST: pushed beyond hot-path offsets --- */

	/* Screen buffer: rows x cols (13,200 bytes) */
	TermCell	screen[TERM_ROWS][TERM_COLS];

	/* Alternate screen buffer (13,200 bytes) */
	TermCell	alt_screen[TERM_ROWS][TERM_COLS];

	/* Scrollback ring buffer (25,344 bytes) */
	TermCell	scrollback[TERM_SCROLLBACK_LINES][TERM_COLS];

	/* Disconnect recovery snapshot (13,200 bytes) */
	TermCell	snap_screen[TERM_ROWS][TERM_COLS];
} Terminal;

/* Initialize terminal to default state */
void terminal_init(Terminal *term);

/* Feed raw bytes into the terminal engine */
void terminal_process(Terminal *term, unsigned char *data, short len);

/* Reset terminal to initial state */
void terminal_reset(Terminal *term);

/* Get display cell accounting for scroll_offset (for rendering) */
TermCell *terminal_get_display_cell(Terminal *term, short row, short col);

/* Get display color accounting for scroll_offset (NULL if no color) */
CellColor *terminal_get_display_color(Terminal *term, short row, short col);

/* Scroll back N lines into scrollback buffer */
void terminal_scroll_back(Terminal *term, short lines);

/* Scroll forward N lines toward live view */
void terminal_scroll_forward(Terminal *term, short lines);

/* Check if a row is dirty (needs redraw) */
short terminal_is_row_dirty(Terminal *term, short row);

/* Clear all dirty flags */
void terminal_clear_dirty(Terminal *term);

/* Mark all rows dirty (full redraw) */
void term_dirty_all(Terminal *term);

/* Get current cursor position */
void terminal_get_cursor(Terminal *term, short *row, short *col);

#endif /* TERMINAL_H */
