/*
 * session.c - Multi-session management for Flynn
 */

#include <Memory.h>
#include <Windows.h>
#include <string.h>
#include "session.h"
#include "main.h"
#include "terminal_ui.h"

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

	s->window = NewWindow(0L, &bounds, "\pFlynn", true,
	    documentProc, (WindowPtr)-1L, true, (long)s);
	if (s->window == 0L) {
		DisposePtr((Ptr)s);
		return 0L;
	}

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
