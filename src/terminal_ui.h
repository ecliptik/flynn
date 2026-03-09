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

/* Default cell metrics (Monaco 9pt) */
#define CELL_WIDTH		6
#define CELL_HEIGHT		12

/* Runtime cell dimensions (set by term_ui_set_font) */
extern short g_cell_width;
extern short g_cell_height;

/* Margins within the terminal window */
#define LEFT_MARGIN		2
#define TOP_MARGIN		2

/* Cursor blink interval in ticks (30 ticks ~ 0.5s) */
#define CURSOR_BLINK_TICKS	30

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

/* Initialize terminal UI (set font, store references) */
void term_ui_init(WindowPtr win, Terminal *term);

/* Set terminal font and update cell metrics via GetFontInfo */
void term_ui_set_font(WindowPtr win, short font_id, short font_size);

/* Draw terminal contents (only dirty rows + cursor) */
void term_ui_draw(WindowPtr win, Terminal *term);

/* Invalidate only dirty rows (call after terminal_process) */
void term_ui_invalidate(WindowPtr win, Terminal *term);

/* Invalidate entire terminal area (for full redraws) */
void term_ui_invalidate_all(WindowPtr win);

/* Update cursor blink state; call from event loop idle */
void term_ui_cursor_blink(WindowPtr win, Terminal *term);

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
short term_ui_sel_contains(short row, short col);
short term_ui_sel_check_double_click(unsigned long when, short row, short col);
void  term_ui_sel_dirty_rows(Terminal *term, short old_extent_row,
	    short new_extent_row);
void  term_ui_sel_dirty_all(Terminal *term);

/* Dark mode */
void term_ui_set_dark_mode(short enabled);

/* Per-session UI state for save/restore */
typedef struct {
	unsigned long	cursor_last_tick;
	short		cursor_visible;
	short		cursor_prev_row;
	short		cursor_prev_col;
	short		cursor_initialized;
	Selection	sel;
} UIState;

/* Save/load per-session UI state (cursor blink, selection) */
void term_ui_save_state(UIState *dst);
void term_ui_load_state(UIState *src);

#endif /* TERMINAL_UI_H */
