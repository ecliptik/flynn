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

/* Monaco 9pt cell metrics */
#define CELL_WIDTH		6
#define CELL_HEIGHT		12

/* Margins within the terminal window */
#define LEFT_MARGIN		2
#define TOP_MARGIN		2

/* Cursor blink interval in ticks (30 ticks ~ 0.5s) */
#define CURSOR_BLINK_TICKS	30

/* Initialize terminal UI (set font, store references) */
void term_ui_init(WindowPtr win, Terminal *term);

/* Draw terminal contents (only dirty rows + cursor) */
void term_ui_draw(WindowPtr win, Terminal *term);

/* Invalidate only dirty rows (call after terminal_process) */
void term_ui_invalidate(WindowPtr win, Terminal *term);

/* Invalidate entire terminal area (for full redraws) */
void term_ui_invalidate_all(WindowPtr win);

/* Update cursor blink state; call from event loop idle */
void term_ui_cursor_blink(WindowPtr win, Terminal *term);

#endif /* TERMINAL_UI_H */
