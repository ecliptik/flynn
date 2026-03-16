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
	char connect_host[128];
	char forward_query[192];

	/* RFC 1288 forwarding: user@host1@gateway connects to
	 * gateway and sends "user@host1\r\n".  Also handles
	 * host field containing "host@gateway" with separate
	 * username.  Split on the LAST '@' in host. */
	strncpy(connect_host, host,
	    sizeof(connect_host) - 1);
	connect_host[sizeof(connect_host) - 1] = '\0';
	forward_query[0] = '\0';
	{
		char *last_at;
		short i, last_pos = -1;

		for (i = 0; connect_host[i]; i++) {
			if (connect_host[i] == '@')
				last_pos = i;
		}
		if (last_pos > 0) {
			/* Split: everything before last @ is
			 * the forwarded query, after is the
			 * connect host */
			connect_host[last_pos] = '\0';
			if (username[0])
				snprintf(forward_query,
				    sizeof(forward_query),
				    "%s@%s", username,
				    connect_host);
			else
				strncpy(forward_query,
				    connect_host,
				    sizeof(forward_query) - 1);
			forward_query[
			    sizeof(forward_query) - 1] = '\0';
			/* Shift connect_host to the part after @ */
			memmove(connect_host,
			    host + last_pos + 1,
			    strlen(host + last_pos + 1) + 1);
			/* Use forwarded query as the username
			 * for query building below */
			username = forward_query;
		}
	}

	/* Ensure font metrics initialized */
	term_ui_ensure_metrics(prefs.font_id, prefs.font_size);

	s = session_new();
	if (!s) {
		show_error_alert("Out of memory");
		return 0L;
	}
	session_init_from_prefs(s);
	s->conn.protocol = PROTO_FINGER;

	/* Connect to port 79 */
	snprintf(smsg, sizeof(smsg),
	    "Finger %.50s\311", connect_host);
	sw = conn_status_show(smsg);
	if (!conn_connect(&s->conn, connect_host, FINGER_PORT, sw)) {
		conn_status_close(sw);
		session_destroy_and_fixup(s);
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

	/* Set window title — show original host for forwarded
	 * queries (e.g. "user@host1@gateway") */
	if (forward_query[0])
		set_wtitlef(s->window,
		    "Flynn - %s@%s (finger)",
		    forward_query, connect_host);
	else if (username[0])
		set_wtitlef(s->window,
		    "Flynn - %s@%s (finger)",
		    username, connect_host);
	else
		set_wtitlef(s->window,
		    "Flynn - %s (finger)", connect_host);

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
