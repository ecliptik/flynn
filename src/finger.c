/*
 * finger.c - Finger protocol (RFC 1288) client for Flynn
 *
 * Finger is a simple request/response protocol on TCP port 79.
 * The client connects, sends a query line, and the server responds
 * with user information then closes the connection.
 *
 * Query format: [/W ][username]\r\n
 *   - Empty query (just \r\n) lists online users
 *   - /W requests verbose ("long") output
 *
 * Data processing is done synchronously in finger_connect() —
 * the response is polled, rendered, and drawn before returning
 * to the event loop.  This avoids offscreen buffer conflicts
 * with the multi-session drain loop.
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <Dialogs.h>
#include <Memory.h>
#include <ToolUtils.h>
#include <OSUtils.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "finger.h"
#include "session.h"
#include "connection.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "settings.h"
#include "dialogs.h"
#include "macutil.h"
#include "menus.h"

/* External references */
extern FlynnPrefs prefs;
extern Session *active_session;
extern void session_save_font(Session *s);
extern void session_load_font(Session *s);
extern void do_window_resize(Session *s, short width, short height);

/*
 * finger_process_data - feed raw TCP data to terminal with
 * LF→CRLF conversion.  Finger servers send LF-only line
 * endings, but VT100 LF only moves the cursor down without
 * carriage return.
 */
static void
finger_process_data(Terminal *term, unsigned char *src,
    short slen)
{
	short i, start = 0;

	for (i = 0; i < slen; i++) {
		if (src[i] == '\n' &&
		    (i == 0 || src[i - 1] != '\r')) {
			if (i > start)
				terminal_process(term,
				    src + start,
				    i - start);
			terminal_process(term,
			    (unsigned char *)"\r\n", 2);
			start = i + 1;
		}
	}
	if (start < slen)
		terminal_process(term,
		    src + start, slen - start);
}

/* Finger dialog filter: Return=OK, Cmd+.=Cancel, Tab=cycle fields */
static pascal Boolean
finger_dlg_filter(DialogPtr dlg, EventRecord *evt, short *item)
{
	if (evt->what == keyDown) {
		char key = evt->message & charCodeMask;

		if (key == '\r' || key == '\n' || key == 0x03) {
			*item = FINGER_OK;
			return true;
		}
		if ((evt->modifiers & cmdKey) && key == '.') {
			*item = FINGER_CANCEL;
			return true;
		}
		if (key == '\t') {
			DialogPeek dp = (DialogPeek)dlg;
			short cur = dp->editField + 1;
			short next;

			if (cur == FINGER_HOST_FIELD)
				next = FINGER_USER_FIELD;
			else
				next = FINGER_HOST_FIELD;
			SelectDialogItemText(dlg, next, 0, 32767);
			*item = next;
			return true;
		}
	}
	return false;
}

/*
 * finger_connect - shared finger connect logic.
 * Creates session, connects to port 79, sends query,
 * synchronously polls for response and renders it.
 * Returns session or NULL on failure.
 */
static Session *
finger_connect(const char *host, const char *username,
    Boolean verbose, short bm_index)
{
	Session *s;
	WindowPtr sw;
	char smsg[80];
	char query[256];
	short qlen;
	GrafPtr save;

	/* Ensure font metrics initialized */
	if (g_cell_width == 0) {
		GrafPtr save_port;
		GrafPort temp_port;

		GetPort(&save_port);
		OpenPort(&temp_port);
		term_ui_set_font((WindowPtr)&temp_port,
		    prefs.font_id, prefs.font_size);
		ClosePort(&temp_port);
		SetPort(save_port);
	}

	s = session_new();
	if (!s) {
		ParamText("\pOut of memory",
		    "\p", "\p", "\p");
		StopAlert(128, 0L);
		return 0L;
	}
	session_init_from_prefs(s);
	s->conn.protocol = PROTO_FINGER;

	/* Connect to port 79 */
	snprintf(smsg, sizeof(smsg),
	    "Finger %.50s\311", host);
	sw = conn_status_show(smsg);
	if (!conn_connect(&s->conn, host, FINGER_PORT, sw)) {
		conn_status_close(sw);
		if (s == active_session)
			active_session = 0L;
		session_destroy(s);
		return 0L;
	}
	conn_status_close(sw);

	/* Store username for status bar display */
	if (username[0]) {
		strncpy(s->conn.username, username,
		    sizeof(s->conn.username) - 1);
		s->conn.username[
		    sizeof(s->conn.username) - 1] = '\0';
	}

	/* Build and send query: [/W ][username]\r\n */
	qlen = 0;
	if (verbose) {
		query[qlen++] = '/';
		query[qlen++] = 'W';
		if (username[0])
			query[qlen++] = ' ';
	}
	if (username[0]) {
		short ulen = strlen(username);

		if (ulen > (short)(sizeof(query) - qlen - 3))
			ulen = sizeof(query) - qlen - 3;
		memcpy(query + qlen, username, ulen);
		qlen += ulen;
	}
	query[qlen++] = '\r';
	query[qlen++] = '\n';
	conn_send(&s->conn, query, qlen);

	/* Set up terminal (no telnet negotiation) */
	terminal_reset(&s->terminal);
	term_ui_invalidate_offscreen();

	/* Synchronous poll: read response, convert LF→CRLF,
	 * process through terminal.  Finger is one-shot so
	 * we handle everything here before returning to the
	 * event loop. */
	GetPort(&save);
	SetPort(s->window);
	{
		unsigned long deadline = TickCount() + 10 * 60;

		while (s->conn.state == CONN_STATE_CONNECTED &&
		    TickCount() < deadline) {
			conn_idle(&s->conn);
			if (s->conn.read_len > 0) {
				finger_process_data(
				    &s->terminal,
				    (unsigned char *)
				    s->conn.read_buf,
				    s->conn.read_len);
				s->conn.read_len = 0;
			}
		}
	}

	/* Set window title */
	if (username[0])
		set_wtitlef(s->window,
		    "Flynn - %s@%s (finger)",
		    username, host);
	else
		set_wtitlef(s->window,
		    "Flynn - %s (finger)", host);

	/* Single draw after all data received — avoids
	 * shadow buffer / partial CopyBits interaction that
	 * causes missing rows when data arrives in chunks */
	term_dirty_all(&s->terminal);
	term_ui_draw(s->window, &s->terminal);
	if (prefs.show_status_bar)
		draw_status_bar(s->window, s);
	if (s->scrollbar) {
		short max_val = s->terminal.sb_count;

		SetControlMaximum(s->scrollbar, max_val);
		SetControlValue(s->scrollbar, max_val);
		HiliteControl(s->scrollbar,
		    max_val > 0 ? 0 : 255);
	}
	SetPort(save);

	term_ui_save_state(&s->ui);

	/* Track bookmark */
	if (bm_index >= 0) {
		add_recent_bookmark(bm_index);
		s->bookmark_index = bm_index;
	}

	return s;
}

void
do_finger(void)
{
	DialogPtr dlg;
	short item_hit;
	char host[128];
	char username[64];
	Boolean verbose;
	short item_type;
	Handle item_h;
	Rect item_rect;
	Session *s;

	dlg = GetNewDialog(DLOG_FINGER_ID, 0L,
	    (WindowPtr)-1L);
	if (!dlg) {
		SysBeep(10);
		return;
	}

	setup_default_button_outline(dlg,
	    FINGER_DEFAULT_BTN);

	/* Pre-fill from last finger query */
	if (prefs.finger_host[0])
		dlg_set_text(dlg, FINGER_HOST_FIELD,
		    prefs.finger_host);
	if (prefs.finger_user[0])
		dlg_set_text(dlg, FINGER_USER_FIELD,
		    prefs.finger_user);

	ShowWindow(dlg);

	for (;;) {
		ModalDialog(
		    (ModalFilterUPP)finger_dlg_filter,
		    &item_hit);

		if (item_hit == FINGER_CANCEL) {
			DisposeDialog(dlg);
			update_menus();
			return;
		}
		if (item_hit == FINGER_OK)
			break;

		/* Toggle verbose checkbox */
		if (item_hit == FINGER_VERBOSE_CHK) {
			short val;

			GetDialogItem(dlg,
			    FINGER_VERBOSE_CHK,
			    &item_type, &item_h,
			    &item_rect);
			val = GetControlValue(
			    (ControlHandle)item_h);
			SetControlValue(
			    (ControlHandle)item_h, !val);
		}
	}

	/* Extract fields */
	dlg_get_text(dlg, FINGER_HOST_FIELD,
	    host, sizeof(host));
	dlg_get_text(dlg, FINGER_USER_FIELD,
	    username, sizeof(username));
	GetDialogItem(dlg, FINGER_VERBOSE_CHK,
	    &item_type, &item_h, &item_rect);
	verbose = GetControlValue(
	    (ControlHandle)item_h) != 0;

	DisposeDialog(dlg);

	if (!host[0]) {
		update_menus();
		return;
	}

	s = finger_connect(host, username, verbose, -1);
	if (s) {
		/* Save last finger host/user to prefs */
		strncpy(prefs.finger_host, host,
		    sizeof(prefs.finger_host) - 1);
		prefs.finger_host[
		    sizeof(prefs.finger_host) - 1] = '\0';
		strncpy(prefs.finger_user, username,
		    sizeof(prefs.finger_user) - 1);
		prefs.finger_user[
		    sizeof(prefs.finger_user) - 1] = '\0';
		prefs_save(&prefs);

		if (active_session &&
		    active_session->conn.state ==
		    CONN_STATE_CONNECTED)
			SelectWindow(s->window);
		active_session = s;
	}
	update_menus();
}

void
do_finger_bookmark(short bm_idx)
{
	Bookmark *bm;
	Session *s;

	if (bm_idx < 0 || bm_idx >= prefs.bookmark_count)
		return;

	bm = &prefs.bookmarks[bm_idx];

	s = finger_connect(bm->host, bm->username,
	    true, bm_idx);
	if (s) {
		if (active_session &&
		    active_session->conn.state ==
		    CONN_STATE_CONNECTED)
			SelectWindow(s->window);
		active_session = s;
	}
	update_menus();
}
