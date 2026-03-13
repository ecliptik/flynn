/*
 * input.c - Keyboard and mouse input handling for Flynn
 * Extracted from main.c
 */

#include <Quickdraw.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <ToolUtils.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "telnet.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "settings.h"
#include "macutil.h"
#include "menus.h"
#include "input.h"

/* External references to main.c globals */
extern FlynnPrefs prefs;
extern Session *active_session;

/* Forward declaration for local helper */
static void track_selection_drag(Session *s);

/* ---- Data-driven key mapping tables ---- */

/* Numpad application mode (DECKPAM): vkey → SS3 suffix char */
static const struct { unsigned char vkey, suffix; } numpad_app_map[] = {
	{0x52, 'p'}, {0x53, 'q'}, {0x54, 'r'}, {0x55, 's'},
	{0x56, 't'}, {0x57, 'u'}, {0x58, 'v'}, {0x59, 'w'},
	{0x5B, 'x'}, {0x5C, 'y'}, {0x41, 'n'}, {0x4C, 'M'},
	{0x45, 'k'}, {0x4E, 'm'}, {0x43, 'j'}
};
#define NUM_NUMPAD_APP (sizeof(numpad_app_map) / sizeof(numpad_app_map[0]))

/* Arrow keys: vkey → direction suffix (A=Up, B=Down, C=Right, D=Left) */
static const struct { unsigned char vkey, suffix; } arrow_map[] = {
	{0x7E, 'A'}, {0x7D, 'B'}, {0x7C, 'C'}, {0x7B, 'D'}
};
#define NUM_ARROWS (sizeof(arrow_map) / sizeof(arrow_map[0]))

/* Special keys: vkey → escape sequence (up to 5 bytes) */
static const struct { unsigned char vkey, len; char seq[6]; } special_key_map[] = {
	{0x73, 3, "\033[H"},	/* Home */
	{0x77, 3, "\033[F"},	/* End */
	{0x74, 4, "\033[5~"},	/* Page Up */
	{0x79, 4, "\033[6~"},	/* Page Down */
	{0x75, 4, "\033[3~"},	/* Forward Delete */
	/* Backspace (0x33) handled separately: DEL/BS per terminal type */
	{0x35, 1, {0x1B}},	/* Escape */
	{0x24, 1, {0x0D}},	/* Return */
	{0x4C, 1, {0x0D}},	/* Keypad Enter */
	{0x30, 1, {0x09}},	/* Tab */
	{0x47, 1, {0x1B}},	/* Clear/NumLock → Escape */
	{0x7A, 3, "\033OP"},	/* F1 */
	{0x78, 3, "\033OQ"},	/* F2 */
	{0x63, 3, "\033OR"},	/* F3 */
	{0x76, 3, "\033OS"},	/* F4 */
	{0x60, 5, "\033[15~"},	/* F5 */
	{0x61, 5, "\033[17~"},	/* F6 */
	{0x62, 5, "\033[18~"},	/* F7 */
	{0x64, 5, "\033[19~"},	/* F8 */
	{0x65, 5, "\033[20~"},	/* F9 */
	{0x6D, 5, "\033[21~"},	/* F10 */
	{0x67, 5, "\033[23~"},	/* F11 */
	{0x6F, 5, "\033[24~"}	/* F12 */
};
#define NUM_SPECIAL_KEYS (sizeof(special_key_map) / sizeof(special_key_map[0]))

void
buffer_key_send(Session *s, const char *data, short len)
{
	short i;

	for (i = 0; i < len && s->key_send_len < (short)sizeof(s->key_send_buf);
	    i++)
		s->key_send_buf[s->key_send_len++] = data[i];
}

void
flush_key_send(Session *s)
{
	if (s->key_send_len > 0 && s->conn.state == CONN_STATE_CONNECTED) {
		conn_send(&s->conn, s->key_send_buf, s->key_send_len);
		s->key_send_len = 0;
	} else {
		s->key_send_len = 0;
	}
}

/*
 * local_echo - Echo data locally when server isn't echoing.
 * Only active when local_echo pref is on AND server has WONT ECHO.
 */
static void
local_echo(Session *s, const unsigned char *data, short len)
{
	if (!prefs.local_echo)
		return;
	if (s->telnet.opts[TELOPT_ECHO] & OPTFLAG_REMOTE)
		return;
	terminal_process(&s->terminal, (unsigned char *)data, len);
}

void
handle_key_down(Session *s, EventRecord *event)
{
	char key;
	short vkey;

	key = event->message & charCodeMask;
	vkey = (event->message >> 8) & 0xFF;

	if (event->modifiers & cmdKey) {
		/* Cmd+Up/Down for scrollback navigation */
		if (vkey == 0x7E || vkey == 0x7D ||
		    key == 0x1E || key == 0x1F) {
			GrafPtr save;

			if (vkey == 0x7E || key == 0x1E) {
				if (event->modifiers & shiftKey)
					terminal_scroll_back(
					    &s->terminal,
					    s->terminal.active_rows);
				else
					terminal_scroll_back(
					    &s->terminal, 1);
			} else {
				if (event->modifiers & shiftKey)
					terminal_scroll_forward(
					    &s->terminal,
					    s->terminal.active_rows);
				else
					terminal_scroll_forward(
					    &s->terminal, 1);
			}

			GetPort(&save);
			SetPort(s->window);
			term_ui_draw(s->window, &s->terminal);

			if (s->terminal.scroll_offset > 0)
				set_wtitlef(s->window, "Flynn [-%d]",
				    s->terminal.scroll_offset);
			else
				SetWTitle(s->window, "\pFlynn");

			SetPort(save);
			return;
		}
		/* Cmd+. sends Escape (classic Mac convention) */
		if (key == '.' &&
		    s->conn.state == CONN_STATE_CONNECTED) {
			char esc = 0x1B;
			buffer_key_send(s, &esc, 1);
			return;
		}

		/* Cmd+1..0 -> F1-F10 for M0110 keyboards (table-driven) */
		if (s->conn.state == CONN_STATE_CONNECTED &&
		    key >= '0' && key <= '9') {
			/* Reuse special_key_map F-key entries via
			 * lookup: F1=0x7A..F10=0x6D. Map digit to
			 * same vkey the F-key table uses. */
			static const unsigned char digit_to_fvkey[10] = {
				0x6D,	/* '0' -> F10 */
				0x7A,	/* '1' -> F1 */
				0x78,	/* '2' -> F2 */
				0x63,	/* '3' -> F3 */
				0x76,	/* '4' -> F4 */
				0x60,	/* '5' -> F5 */
				0x61,	/* '6' -> F6 */
				0x62,	/* '7' -> F7 */
				0x64,	/* '8' -> F8 */
				0x65,	/* '9' -> F9 */
			};
			short ki, fvk;

			fvk = digit_to_fvkey[key - '0'];
			for (ki = 0; ki < (short)NUM_SPECIAL_KEYS; ki++) {
				if (special_key_map[ki].vkey == fvk) {
					buffer_key_send(s,
					    (char *)special_key_map[ki].seq,
					    special_key_map[ki].len);
					return;
				}
			}
		}

		update_menus();
		handle_menu(MenuKey(key));
		return;
	}

	/* Clear selection on any non-Cmd keypress */
	if (term_ui_sel_active()) {
		GrafPtr save;

		term_ui_sel_dirty_all(&s->terminal);
		term_ui_sel_clear();
		GetPort(&save);
		SetPort(s->window);
		term_ui_draw(s->window, &s->terminal);
		SetPort(save);
	}

	if (s->conn.state != CONN_STATE_CONNECTED)
		return;

	/* Application keypad mode (DECKPAM): numpad sends SS3 sequences */
	if (s->terminal.keypad_mode) {
		short ki;
		for (ki = 0; ki < (short)NUM_NUMPAD_APP; ki++) {
			if (numpad_app_map[ki].vkey == vkey) {
				char ss3[3];
				ss3[0] = '\033'; ss3[1] = 'O';
				ss3[2] = numpad_app_map[ki].suffix;
				buffer_key_send(s, ss3, 3);
				return;
			}
		}
	}

	/* Arrow keys: cursor key mode selects \033O vs \033[ prefix */
	{
		short ki;
		for (ki = 0; ki < (short)NUM_ARROWS; ki++) {
			if (arrow_map[ki].vkey == vkey) {
				char arr[3];
				arr[0] = '\033';
				arr[1] = s->terminal.cursor_key_mode ?
				    'O' : '[';
				arr[2] = arrow_map[ki].suffix;
				buffer_key_send(s, arr, 3);
				return;
			}
		}
	}

	/* Backspace: DEL or BS per user preference */
	if (vkey == 0x33) {
		char bs = prefs.backspace_bs ? 0x08 : 0x7F;
		buffer_key_send(s, &bs, 1);
		{
			static const unsigned char bs_echo[] =
			    { 0x08, 0x20, 0x08 };
			local_echo(s, bs_echo, 3);
		}
		return;
	}

	/* Special keys and function keys: table-driven lookup */
	{
		short ki;
		for (ki = 0; ki < (short)NUM_SPECIAL_KEYS; ki++) {
			if (special_key_map[ki].vkey == vkey) {
				buffer_key_send(s,
				    (char *)special_key_map[ki].seq,
				    special_key_map[ki].len);
				/* Local echo for Return/Enter */
				if (vkey == 0x24 || vkey == 0x4C) {
					static const unsigned char
					    crlf[] = { 0x0D, 0x0A };
					local_echo(s, crlf, 2);
				}
				return;
			}
		}
	}

	/* Arrow keys by charCode — catches M0110A and keyboards where
	 * vkey codes differ from ADB 0x7B-0x7E */
	if (key >= 0x1C && key <= 0x1F) {
		/* charCode 0x1C-0x1F = Left, Right, Up, Down */
		static const char charcode_arrow_suffix[4] = {
			'D', 'C', 'A', 'B'
		};
		char arr[3];

		arr[0] = '\033';
		arr[1] = s->terminal.cursor_key_mode ? 'O' : '[';
		arr[2] = charcode_arrow_suffix[key - 0x1C];
		buffer_key_send(s, arr, 3);
		return;
	}

	/* ESC character from any source (keyboard adapters, Clear key) */
	if (key == 0x1B) {
		buffer_key_send(s, &key, 1);
		return;
	}

	/* Option as Ctrl on M0110 keyboard (no physical Ctrl key).
	 * Mac OS remaps char codes when Option is held, so use the
	 * virtual keycode to recover the unmodified base key. */
	if (event->modifiers & optionKey) {
		static const char vkey_to_base[50] = {
			'a','s','d','f','h','g','z','x',   /* 0x00 */
			'c','v',  0,'b','q','w','e','r',   /* 0x08 */
			'y','t',  0,  0,  0,  0,  0,  0,   /* 0x10 */
			  0,  0,  0,  0,  0,  0,']','o',   /* 0x18 */
			'u','[','i','p',  0,'l','j',  0,   /* 0x20 */
			'k',  0,'\\', 0,'/','n','m',  0,   /* 0x28 */
			  0,' ',                            /* 0x30 */
		};

		if (vkey < 50 && vkey_to_base[vkey]) {
			/* Ctrl-/ is 0x1F but '/' & 0x1F = 0x0F;
			 * handle as special case */
			if (vkey_to_base[vkey] == '/')
				key = 0x1F;
			else
				key = vkey_to_base[vkey] & 0x1F;
			buffer_key_send(s, &key, 1);
			return;
		}
	}

	/* Ctrl+key with physical Control key (extended keyboards) */
	if (event->modifiers & ControlKey) {
		key = key & 0x1F;
		buffer_key_send(s, &key, 1);
		return;
	}

	/* Regular printable character */
	buffer_key_send(s, &key, 1);
	local_echo(s, (unsigned char *)&key, 1);
}

short
pixel_to_row(Session *s, short v)
{
	short r;

	r = (v - TOP_MARGIN) / g_cell_height;
	if (r < 0) r = 0;
	if (r >= s->terminal.active_rows)
		r = s->terminal.active_rows - 1;
	return r;
}

short
pixel_to_col(Session *s, short h)
{
	short c;

	c = (h - LEFT_MARGIN) / g_cell_width;
	if (c < 0) c = 0;
	if (c >= s->terminal.active_cols)
		c = s->terminal.active_cols - 1;
	return c;
}

void
handle_content_click(Session *s, EventRecord *event)
{
	Point local_pt;
	short row, col;
	GrafPtr save;

	GetPort(&save);
	SetPort(s->window);

	local_pt = event->where;
	GlobalToLocal(&local_pt);
	row = pixel_to_row(s, local_pt.v);
	col = pixel_to_col(s, local_pt.h);

	/* Clear any previous selection first */
	if (term_ui_sel_active()) {
		term_ui_sel_dirty_all(&s->terminal);
		term_ui_sel_clear();
	}

	if (term_ui_sel_check_double_click(event->when, row, col)) {
		/* Double-click: word selection */
		term_ui_sel_start_word(row, col,
		    s->terminal.scroll_offset, &s->terminal);
	} else if (event->modifiers & shiftKey) {
		/* Shift-click: extend from cursor to click */
		term_ui_sel_start(s->terminal.cur_row,
		    s->terminal.cur_col, s->terminal.scroll_offset);
		term_ui_sel_extend(row, col, &s->terminal);
	} else {
		/* New selection */
		term_ui_sel_start(row, col,
		    s->terminal.scroll_offset);
	}

	track_selection_drag(s);

	/* Dirty only the click row (for zero-width selection cleanup)
	 * and any active selection rows.  Reduces click redraw from
	 * 24 rows to typically 1 row. */
	s->terminal.dirty[row] = 1;
	term_ui_sel_dirty_all(&s->terminal);
	term_ui_draw(s->window, &s->terminal);

	SetPort(save);
}

static void
track_selection_drag(Session *s)
{
	Point pt;
	short row, col, prev_row, prev_col;

	prev_row = -1;
	prev_col = -1;

	while (StillDown()) {
		GetMouse(&pt);
		row = pixel_to_row(s, pt.v);
		col = pixel_to_col(s, pt.h);

		if (row == prev_row && col == prev_col)
			continue;

		term_ui_sel_dirty_rows(&s->terminal, prev_row < 0 ?
		    row : prev_row, row);
		term_ui_sel_extend(row, col, &s->terminal);
		term_ui_draw(s->window, &s->terminal);

		prev_row = row;
		prev_col = col;
	}

	term_ui_sel_finalize();
}
