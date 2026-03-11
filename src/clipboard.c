/*
 * clipboard.c - Clipboard operations for Flynn
 * Extracted from main.c
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Memory.h>
#include <Multiverse.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "glyphs.h"
#include "clipboard.h"

/* External references to main.c globals */
extern Session *active_session;

void
do_copy(void)
{
	long buf_size;
	char *buf;
	short row, col, len, last_nonspace;
	TermCell *cell;
	Session *s = active_session;

	if (!s)
		return;

	/* Ensure global sel reflects this session's selection */
	term_ui_load_state(&s->ui);

	buf_size = (long)s->terminal.active_rows *
	    (s->terminal.active_cols + 1);
	buf = (char *)NewPtr(buf_size);
	if (!buf)
		return;

	if (!term_ui_sel_active()) {
		DisposePtr((Ptr)buf);
		return;
	}

	{
		short sr, sc, er, ec;
		short c_start, c_end;

		term_ui_sel_get_range(&sr, &sc, &er, &ec);

		len = 0;
		for (row = sr; row <= er; row++) {
			if (sr == er) {
				c_start = sc;
				c_end = ec;
			} else if (row == sr) {
				c_start = sc;
				c_end = s->terminal.active_cols - 1;
			} else if (row == er) {
				c_start = 0;
				c_end = ec;
			} else {
				c_start = 0;
				c_end = s->terminal.active_cols - 1;
			}

			last_nonspace = -1;
			for (col = c_start; col <= c_end; col++) {
				char cc;
				const GlyphInfo *gi;

				cell = terminal_get_display_cell(
				    &s->terminal, row, col);
				if (CELL_IS_GLYPH(cell->attr) &&
				    cell->ch == GLYPH_WIDE_SPACER) {
					buf[len + (col - c_start)] = ' ';
					continue;
				}
				if (CELL_IS_GLYPH(cell->attr)) {
					gi = glyph_get_info(cell->ch);
					cc = gi ? gi->copy_char : '?';
				} else if (CELL_IS_BRAILLE(cell->attr)) {
					cc = '.';
				} else {
					cc = cell->ch;
				}
				buf[len + (col - c_start)] = cc;
				if (cc != ' ')
					last_nonspace = col - c_start;
			}
			len += last_nonspace + 1;
			if (row < er)
				buf[len++] = '\r';
		}
	}

	ZeroScrap();
	PutScrap(len, 'TEXT', buf);
	DisposePtr((Ptr)buf);
}

void
do_paste(void)
{
	Handle h;
	long offset, len;
	Session *s = active_session;

	if (!s || s->conn.state != CONN_STATE_CONNECTED)
		return;

	h = NewHandle(0);
	if (!h)
		return;

	len = GetScrap(h, 'TEXT', &offset);
	if (len > 0) {
		char *p;
		long sent;

		HLock(h);
		p = *h;

		if (s->terminal.bracketed_paste)
			conn_send(&s->conn, "\033[200~", 6);

		sent = 0;
		while (sent < len) {
			short chunk;

			chunk = len - sent;
			if (chunk > 256)
				chunk = 256;
			conn_send(&s->conn, p + sent, chunk);
			sent += chunk;
		}

		if (s->terminal.bracketed_paste)
			conn_send(&s->conn, "\033[201~", 6);

		HUnlock(h);
	}
	DisposeHandle(h);
}

void
do_select_all(void)
{
	Session *s = active_session;

	if (!s || s->conn.state != CONN_STATE_CONNECTED)
		return;

	term_ui_sel_start(0, 0, 0);
	term_ui_sel_extend(s->terminal.active_rows - 1,
	    s->terminal.active_cols - 1, &s->terminal);
	term_ui_sel_finalize();
	term_ui_sel_dirty_all(&s->terminal);

	{
		GrafPtr save;

		GetPort(&save);
		SetPort(s->window);
		term_ui_draw(s->window, &s->terminal);
		SetPort(save);
	}

	/* Need to update menus to reflect selection state.
	 * Call through extern since menus.c owns this. */
	{
		extern void update_menus(void);
		update_menus();
	}
}
