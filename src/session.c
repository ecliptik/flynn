/*
 * session.c - Multi-session management for Flynn
 */

#include <Quickdraw.h>
#include <Memory.h>
#include <Windows.h>
#include <string.h>
#include <stdio.h>

#include "session.h"
#include "main.h"
#include "connection.h"
#include "telnet.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "settings.h"
#include "macutil.h"
#include "tcp.h"
#include "color.h"

static Session *sessions[MAX_SESSIONS];
static short num_sessions = 0;

Session *
session_new(void)
{
	Session *s;
	short i, slot;
	Rect bounds;
	short offset, win_w, win_h;

	/* Find empty slot */
	slot = -1;
	for (i = 0; i < MAX_SESSIONS; i++) {
		if (sessions[i] == 0L) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return 0L;

	/* Allocate */
	s = (Session *)NewPtr(sizeof(Session));
	if (s == 0L)
		return 0L;

	memset(s, 0, sizeof(Session));
	s->id = slot;
	s->bookmark_index = -1;

	/* Initialize components */
	terminal_init(&s->terminal);
	telnet_init(&s->telnet);

	/* Default grid dimensions */
	s->terminal.active_cols = TERM_DEFAULT_COLS;
	s->terminal.active_rows = TERM_DEFAULT_ROWS;

	/* Create window with cascading offset.
	 * Use 30px steps so 4 windows (0,30,60,90) all stay visible
	 * on the 512x342 Mac Plus screen (usable area ~512x322). */
	offset = slot * 30;
	win_w = LEFT_MARGIN * 2 + g_cell_width * TERM_DEFAULT_COLS;
	win_h = TOP_MARGIN * 2 + g_cell_height * TERM_DEFAULT_ROWS;
	SetRect(&bounds, 2 + offset, 40 + offset,
	    2 + offset + win_w, 40 + offset + win_h);

	/* Use NewCWindow on System 7 for color, NewWindow on System 6 */
	if (g_has_color_qd) {
		s->window = NewCWindow(0L, &bounds, "\pFlynn", true,
		    documentProc, (WindowPtr)-1L, true, (long)s);
	} else {
		s->window = NewWindow(0L, &bounds, "\pFlynn", true,
		    documentProc, (WindowPtr)-1L, true, (long)s);
	}
	if (s->window == 0L) {
		DisposePtr((Ptr)s);
		return 0L;
	}

	/*
	 * Note: Palette attachment removed — NSetPalette with
	 * pmTolerant was interfering with RGBForeColor for text.
	 * RGBForeColor/RGBBackColor work directly on color
	 * displays without a custom palette.
	 */

	sessions[slot] = s;
	num_sessions++;
	return s;
}

void
session_destroy(Session *s)
{
	if (s == 0L)
		return;

	if (s->conn.state != CONN_STATE_IDLE)
		conn_close(&s->conn);

	/* Free color arrays (System 7 only) */
	if (s->terminal.screen_color)
		DisposePtr((Ptr)s->terminal.screen_color);
	if (s->terminal.alt_color)
		DisposePtr((Ptr)s->terminal.alt_color);
	if (s->terminal.sb_color)
		DisposePtr((Ptr)s->terminal.sb_color);

	if (s->window)
		DisposeWindow(s->window);

	if (s->id >= 0 && s->id < MAX_SESSIONS)
		sessions[s->id] = 0L;
	num_sessions--;

	DisposePtr((Ptr)s);
}

Session *
session_from_window(WindowPtr win)
{
	short i;

	if (win == 0L)
		return 0L;

	/*
	 * Validate that this window belongs to us by checking
	 * against all session windows. This avoids treating
	 * DA windows or dialog windows as sessions.
	 */
	for (i = 0; i < MAX_SESSIONS; i++) {
		if (sessions[i] && sessions[i]->window == win)
			return sessions[i];
	}
	return 0L;
}

short
session_count(void)
{
	return num_sessions;
}

Session *
session_get(short index)
{
	if (index < 0 || index >= MAX_SESSIONS)
		return 0L;
	return sessions[index];
}

Boolean
session_any_connected(void)
{
	short i;

	for (i = 0; i < MAX_SESSIONS; i++) {
		if (sessions[i] &&
		    sessions[i]->conn.state == CONN_STATE_CONNECTED)
			return true;
	}
	return false;
}

/* External references to main.c globals */
extern FlynnPrefs prefs;
extern Session *active_session;

/*
 * session_init_from_prefs - Initialize a new session's font, UI state,
 * DNS server, and dark mode from global preferences.
 * Call after session_new(), before connecting.
 */
void
session_init_from_prefs(Session *s)
{
	SetPort(s->window);
	s->font_id = prefs.font_id;
	s->font_size = prefs.font_size;
	term_ui_set_font(s->window, s->font_id, s->font_size);
	session_save_font(s);
	term_ui_init(s->window, &s->terminal);
	term_ui_save_state(&s->ui);
	s->conn.dns_server = ip2long(prefs.dns_server);
	if (prefs.dark_mode)
		PaintRect(&s->window->portRect);
}

/* Font preset table for bookmark font cycling */
FontPreset font_presets[] = {
	{ 4, 9, "Monaco 9" },
	{ 4, 12, "Monaco 12" },
	{ 22, 10, "Courier 10" },
	{ 0, 12, "Chicago 12" },
	{ 3, 9, "Geneva 9" },
	{ 3, 10, "Geneva 10" }
};

void
ttype_to_str(short ttype, char *buf, short buflen)
{
	const char *str;
	switch (ttype) {
	case 0:  str = "xterm"; break;
	case 1:  str = "VT220"; break;
	case 2:  str = "VT100"; break;
	case 3:  str = "xterm-256color"; break;
	default: str = "Default"; break;
	}
	strncpy(buf, str, buflen - 1);
	buf[buflen - 1] = '\0';
}

void
font_to_str(short font_id, short font_size, char *buf, short buflen)
{
	short i;

	if (font_id == 0 && font_size == 0) {
		strncpy(buf, "Default", buflen - 1);
		buf[buflen - 1] = '\0';
		return;
	}
	for (i = 0; i < NUM_FONT_PRESETS; i++) {
		if (font_presets[i].font_id == font_id &&
		    font_presets[i].font_size == font_size) {
			strncpy(buf, font_presets[i].name, buflen - 1);
			buf[buflen - 1] = '\0';
			return;
		}
	}
	snprintf(buf, buflen, "Font %d/%d", font_id, font_size);
}

/* Save current font metrics into session */
void
session_save_font(Session *s)
{
	s->cell_width = g_cell_width;
	s->cell_height = g_cell_height;
	s->cell_baseline = g_cell_baseline;
}

/* Restore session's font metrics into globals */
void
session_load_font(Session *s)
{
	g_cell_width = s->cell_width;
	g_cell_height = s->cell_height;
	g_cell_baseline = s->cell_baseline;
	g_font_id = s->font_id;
	g_font_size = s->font_size;
}

void
do_font_change(short font_id, short font_size)
{
	short win_w, win_h;

	if (!active_session)
		return;

	if (font_id == active_session->font_id &&
	    font_size == active_session->font_size)
		return;

	/* Update active session only */
	active_session->font_id = font_id;
	active_session->font_size = font_size;
	term_ui_set_font(active_session->window, font_id,
	    font_size);
	session_save_font(active_session);

	/* Compute window size for default grid */
	win_w = LEFT_MARGIN * 2 +
	    TERM_DEFAULT_COLS * g_cell_width;
	win_h = TOP_MARGIN * 2 +
	    TERM_DEFAULT_ROWS * g_cell_height;
	if (win_w > MAX_WIN_WIDTH)
		win_w = MAX_WIN_WIDTH;
	if (win_h > MAX_WIN_HEIGHT)
		win_h = MAX_WIN_HEIGHT;

	do_window_resize(active_session, win_w, win_h);

	/* Auto-save to originating bookmark */
	if (active_session->bookmark_index >= 0 &&
	    active_session->bookmark_index < prefs.bookmark_count) {
		prefs.bookmarks[active_session->bookmark_index].font_id =
		    font_id;
		prefs.bookmarks[active_session->bookmark_index].font_size =
		    font_size;
	}

	/* Also update global default for new sessions */
	prefs.font_id = font_id;
	prefs.font_size = font_size;
	prefs_save(&prefs);

	{
		extern void update_prefs_menu(void);
		update_prefs_menu();
	}
}

void
do_window_resize(Session *s, short width, short height)
{
	short new_cols, new_rows;
	GrafPtr save;

	/* Ensure we use this session's font metrics */
	session_load_font(s);

	/* Compute grid from pixel dimensions */
	new_cols = (width - LEFT_MARGIN * 2) / g_cell_width;
	new_rows = (height - TOP_MARGIN * 2) / g_cell_height;

	/* Clamp to buffer limits */
	if (new_cols > TERM_COLS)
		new_cols = TERM_COLS;
	if (new_cols < MIN_WIN_COLS)
		new_cols = MIN_WIN_COLS;
	if (new_rows > TERM_ROWS)
		new_rows = TERM_ROWS;
	if (new_rows < MIN_WIN_ROWS)
		new_rows = MIN_WIN_ROWS;

	s->terminal.active_cols = new_cols;
	s->terminal.active_rows = new_rows;
	s->terminal.scroll_bottom = new_rows - 1;
	s->terminal.scroll_top = 0;
	if (s->terminal.cur_col >= new_cols)
		s->terminal.cur_col = new_cols - 1;
	if (s->terminal.cur_row >= new_rows)
		s->terminal.cur_row = new_rows - 1;

	/* Snap window to grid boundaries */
	SizeWindow(s->window,
	    LEFT_MARGIN * 2 + new_cols * g_cell_width,
	    TOP_MARGIN * 2 + new_rows * g_cell_height, true);

	/* Clear and redraw */
	GetPort(&save);
	SetPort(s->window);
	clear_window_bg(s->window, prefs.dark_mode);
	term_dirty_all(&s->terminal);
	term_ui_draw(s->window, &s->terminal);
	SetPort(save);

	/* Send NAWS if connected */
	if (s->conn.state == CONN_STATE_CONNECTED) {
		unsigned char naws_buf[32];
		short naws_len = 0;

		s->telnet.cols = new_cols;
		s->telnet.rows = new_rows;
		telnet_send_naws(&s->telnet, naws_buf, &naws_len,
		    new_cols, new_rows);
		if (naws_len > 0)
			conn_send(&s->conn, (char *)naws_buf,
			    naws_len);
	}
}
