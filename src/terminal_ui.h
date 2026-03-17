/*
 * terminal_ui.h - Terminal rendering for classic Macintosh
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

#ifndef TERMINAL_UI_H
#define TERMINAL_UI_H

#include <Quickdraw.h>
#include <Windows.h>
#include "terminal.h"

struct Session;  /* forward declaration */

/* Default cell metrics (Monaco 9pt) */
#define CELL_WIDTH		6
#define CELL_HEIGHT		11

/* Runtime cell dimensions (set by term_ui_set_font) */
extern short g_cell_width;
extern short g_cell_height;
extern short g_cell_baseline;
extern short g_font_id;
extern short g_font_size;

/* Margins within the terminal window */
#define LEFT_MARGIN		0
#define TOP_MARGIN		0
#define STATUSBAR_MARGIN	0
#define SCROLLBAR_WIDTH		16

/* Cursor blink interval in ticks (30 ticks ~ 0.5s) */
#define CURSOR_BLINK_TICKS	30

#ifdef FLYNN_CLIPBOARD
/* Text selection state */
typedef struct {
	short		active;
	short		selecting;
	short		anchor_row;
	short		anchor_col;
	short		extent_row;
	short		extent_col;
	short		scroll_offset;
	short		word_mode;
	short		word_anchor_start;
	short		word_anchor_end;
	unsigned long	last_click_ticks;
	short		last_click_row;
	short		last_click_col;
} Selection;
#endif

/* Initialize terminal UI (set font, store references) */
void term_ui_init(WindowPtr win, Terminal *term);

/* Set terminal font and update cell metrics via GetFontInfo */
void term_ui_set_font(WindowPtr win, short font_id, short font_size);

/* Ensure font metrics are initialized (safe to call multiple times) */
void term_ui_ensure_metrics(short font_id, short font_size);

/* Draw terminal contents (only dirty rows + cursor) */
void term_ui_draw(WindowPtr win, Terminal *term);

/* Update cursor blink state; call from event loop idle */
void term_ui_cursor_blink(WindowPtr win, Terminal *term);

#ifdef FLYNN_CLIPBOARD
/* Text selection API */
void  term_ui_sel_start(short row, short col, short scroll_offset);
void  term_ui_sel_start_word(short row, short col, short scroll_offset,
	    Terminal *term);
void  term_ui_sel_extend(short row, short col, Terminal *term);
void  term_ui_sel_clear(void);
void  term_ui_sel_finalize(void);
short term_ui_sel_active(void);
void  term_ui_sel_get_range(short *start_row, short *start_col,
	    short *end_row, short *end_col);
short term_ui_sel_check_double_click(unsigned long when, short row, short col);
void  term_ui_sel_dirty_rows(Terminal *term, short old_extent_row,
	    short new_extent_row);
void  term_ui_sel_dirty_all(Terminal *term);
#else
#define term_ui_sel_active()      0
#define term_ui_sel_clear()       ((void)0)
#define term_ui_sel_dirty_all(t)  ((void)0)
#endif

/* Status bar */
#ifdef FLYNN_STATUS_BAR
void draw_status_bar(WindowPtr win, struct Session *s);
#else
#define draw_status_bar(w, s) ((void)0)
#endif
short status_bar_height(void);  /* STATUSBAR_MARGIN + SCROLLBAR_WIDTH when on, 0 when off */

/* Dark mode */
#ifdef FLYNN_DARK_MODE
void term_ui_set_dark_mode(short enabled);
#else
#define term_ui_set_dark_mode(e) ((void)0)
#endif

#ifdef FLYNN_OFFSCREEN
/* Offscreen double buffer accessors */
short term_ui_has_offscreen(WindowPtr win, short cols, short rows);
void  term_ui_blit_offscreen(WindowPtr win);
void  term_ui_invalidate_offscreen(void);
short term_ui_scroll_offscreen(WindowPtr win, short direction,
	    short active_rows);
void  term_ui_cleanup(void);
#else
#define term_ui_has_offscreen(w, c, r)       0
#define term_ui_blit_offscreen(w)            ((void)0)
#define term_ui_invalidate_offscreen()       ((void)0)
#define term_ui_scroll_offscreen(w, d, r)    (-1)
#define term_ui_cleanup()                    ((void)0)
#endif

/* Per-session UI state for save/restore */
typedef struct {
	unsigned long	cursor_last_tick;
	short		cursor_visible;
	short		cursor_prev_row;
	short		cursor_prev_col;
	short		cursor_initialized;
#ifdef FLYNN_CLIPBOARD
	Selection	sel;
#endif
} UIState;

/* Save/load per-session UI state (cursor blink, selection) */
void term_ui_save_state(UIState *dst);
void term_ui_load_state(UIState *src);

#endif /* TERMINAL_UI_H */
