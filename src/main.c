/*
 * main.c - Flynn: Telnet client for classic Macintosh
 * Targeting System 6.0.8 / Macintosh Plus with MacTCP 2.1
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Memory.h>
#include <SegLoad.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Resources.h>
#include <Multiverse.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "telnet.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "settings.h"
#include "dialogs.h"
#include "menus.h"
#include "input.h"
#include "clipboard.h"
#include "macutil.h"

/* Globals */
Boolean running = true;
Boolean g_suspended = false;
FlynnPrefs prefs;
static RgnHandle grow_clip_rgn = 0L;
Session *active_session = 0L;

/* Notification Manager */
static NMRec nm_rec;
static Boolean notification_posted = false;

/* Shared buffers for telnet/terminal processing (static to avoid stack) */
static unsigned char out_buf[TCP_READ_BUFSIZ];
static unsigned char send_buf[TCP_READ_BUFSIZ];

/* Forward declarations */
static void init_toolbox(void);
static void init_apple_events(void);
static void main_event_loop(void);
static void handle_mouse_down(EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void session_handle_disconnect(Session *sess);
static void session_poll_data(Session *sess);
static void session_update_title(Session *sess);

/*
 * Apple Events handlers for System 7 compatibility.
 * Required by Finder to properly launch/quit the app.
 */
static pascal OSErr
ae_open_app(const AppleEvent *evt, AppleEvent *reply, long refcon)
{
#pragma unused(evt, reply, refcon)
	return noErr;
}

static pascal OSErr
ae_quit_app(const AppleEvent *evt, AppleEvent *reply, long refcon)
{
#pragma unused(evt, reply, refcon)
	running = false;
	return noErr;
}

static pascal OSErr
ae_open_doc(const AppleEvent *evt, AppleEvent *reply, long refcon)
{
#pragma unused(evt, reply, refcon)
	return errAEEventNotHandled;
}

static pascal OSErr
ae_print_doc(const AppleEvent *evt, AppleEvent *reply, long refcon)
{
#pragma unused(evt, reply, refcon)
	return errAEEventNotHandled;
}

static void
init_apple_events(void)
{
	long resp;

	if (Gestalt(gestaltAppleEventsAttr, &resp) == noErr &&
	    (resp & 1)) {
		AEInstallEventHandler(kCoreEventClass,
		    kAEOpenApplication,
		    NewAEEventHandlerUPP(ae_open_app), 0L, false);
		AEInstallEventHandler(kCoreEventClass,
		    kAEQuitApplication,
		    NewAEEventHandlerUPP(ae_quit_app), 0L, false);
		AEInstallEventHandler(kCoreEventClass,
		    kAEOpenDocuments,
		    NewAEEventHandlerUPP(ae_open_doc), 0L, false);
		AEInstallEventHandler(kCoreEventClass,
		    kAEPrintDocuments,
		    NewAEEventHandlerUPP(ae_print_doc), 0L, false);
	}
}

int
main(void)
{
	init_toolbox();
	init_apple_events();
	init_menus();

	/* Load prefs and fast init before showing window */
	prefs_load(&prefs);
	term_ui_set_dark_mode(prefs.dark_mode);

	/* Launch directly into connect dialog (before heavy init).
	 * Font metrics and session creation are deferred until user
	 * clicks OK. MacTCP init is lazy (first conn_connect call). */
	do_connect();

	rebuild_file_menu();
	update_menus();
	update_prefs_menu();

	main_event_loop();
	return 0;
}

static void
init_toolbox(void)
{
	SetApplLimit(LMGetApplLimit() - (1024 * 8));
	InitGraf(&qd.thePort);
	InitFonts();
	FlushEvents(everyEvent, 0);
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(0L);
	InitCursor();
	MaxApplZone();
}

/*
 * session_handle_disconnect - handle remote disconnect for a session.
 * Resets terminal/telnet state, updates title, clears window, shows alert.
 */
static void
session_handle_disconnect(Session *sess)
{
	GrafPtr save;

	/* Reset only parser/protocol state, NOT screen content.
	 * terminal_reset() would wipe the screen buffer via
	 * term_clear_region(), leaving a blank window. */
	sess->terminal.parse_state = PARSE_NORMAL;
	sess->terminal.num_params = 0;
	sess->terminal.intermediate = 0;
	sess->terminal.utf8_len = 0;
	sess->terminal.utf8_expect = 0;
	sess->terminal.response_len = 0;
	sess->terminal.osc_len = 0;
	sess->terminal.scroll_offset = 0;

	telnet_init(&sess->telnet);
	sess->telnet.preferred_ttype = prefs.terminal_type;
	sess->key_send_len = 0;

	/* Set title to show disconnected state */
	if (sess->conn.host[0])
		set_wtitlef(sess->window,
		    "Flynn - %s (disconnected)",
		    sess->conn.host);
	else
		SetWTitle(sess->window, "\pFlynn");

	/* Redraw with existing terminal content preserved */
	GetPort(&save);
	SetPort(sess->window);
	term_dirty_all(&sess->terminal);
	term_ui_draw(sess->window, &sess->terminal);
	SetPort(save);

	if (sess == active_session)
		update_menus();

	term_ui_save_state(&sess->ui);

	/* Show alert or notification depending on foreground/background */
	if (g_suspended) {
		/* Post notification to alert user in background */
		if (!notification_posted) {
			memset(&nm_rec, 0, sizeof(nm_rec));
			nm_rec.qType = 8; /* nmType */
			nm_rec.nmMark = 1;
			nm_rec.nmSound = (Handle)-1;
			nm_rec.nmIcon = 0L;
			nm_rec.nmStr = 0L;
			nm_rec.nmResp = (ProcPtr)-1;
			NMInstall(&nm_rec);
			notification_posted = true;
		}
	} else if (sess == active_session) {
		ParamText(
		    "\pConnection closed by remote host",
		    "\p", "\p", "\p");
		NoteAlert(128, 0L);
	}
}

/*
 * session_update_title - apply OSC title change from terminal to window.
 */
static void
session_update_title(Session *sess)
{
	if (sess->terminal.window_title[0])
		set_wtitlef(sess->window, "Flynn - %s",
		    sess->terminal.window_title);
	else
		SetWTitle(sess->window, "\pFlynn");
	sess->terminal.title_changed = 0;
	update_window_menu();
}

/*
 * session_poll_data - process incoming TCP data through telnet and terminal.
 * Uses file-scope out_buf/send_buf to avoid stack allocation.
 */
static void
session_poll_data(Session *sess)
{
	short out_len = 0;
	short send_len = 0;

	telnet_process(&sess->telnet,
	    (unsigned char *)sess->conn.read_buf,
	    sess->conn.read_len,
	    out_buf, &out_len,
	    send_buf, &send_len);

	if (send_len > 0)
		conn_send(&sess->conn, (char *)send_buf, send_len);

	if (out_len > 0) {
		if (sess->terminal.scroll_offset > 0) {
			sess->terminal.scroll_offset = 0;
			term_dirty_all(&sess->terminal);
		}

		terminal_process(&sess->terminal, out_buf, out_len);

		/* Send terminal responses */
		if (sess->terminal.response_len > 0) {
			conn_send(&sess->conn, sess->terminal.response,
			    sess->terminal.response_len);
			sess->terminal.response_len = 0;
		}

		/* Update window title */
		if (sess->terminal.title_changed)
			session_update_title(sess);

		{
			GrafPtr save;

			GetPort(&save);
			SetPort(sess->window);
			term_ui_draw(sess->window, &sess->terminal);
			SetPort(save);
		}
	}

	sess->conn.read_len = 0;
}

static void
main_event_loop(void)
{
	EventRecord event;
	long wait_ticks;

	while (running) {
		if (g_suspended)
			wait_ticks = 60L;
		else
			wait_ticks = session_any_connected() ? 1L : 30L;
		WaitNextEvent(everyEvent, &event, wait_ticks, 0L);

		switch (event.what) {
		case nullEvent:
		{
			short si;
			Session *sess;
			short prev_state;

			/* Save user interaction state (selection, cursor)
			 * before cycling through sessions */
			if (active_session)
				term_ui_save_state(&active_session->ui);

			for (si = 0; si < MAX_SESSIONS; si++) {
				sess = session_get(si);
				if (!sess)
					continue;

				/* Load this session's UI + font state */
				term_ui_load_state(&sess->ui);
				session_load_font(sess);

				prev_state = sess->conn.state;
				conn_idle(&sess->conn);

				/* Detect remote disconnect */
				if (prev_state == CONN_STATE_CONNECTED &&
				    sess->conn.state == CONN_STATE_IDLE) {
					session_handle_disconnect(sess);
					continue;
				}

				/* Process incoming data */
				if (sess->conn.read_len > 0)
					session_poll_data(sess);

				/* Cursor blink only for front session,
				 * skip when suspended in background */
				if (sess == active_session &&
				    !g_suspended &&
				    sess->conn.state ==
				    CONN_STATE_CONNECTED) {
					GrafPtr save;

					GetPort(&save);
					SetPort(sess->window);
					term_ui_cursor_blink(sess->window,
					    &sess->terminal);
					SetPort(save);
				}

				/* Save state back */
				term_ui_save_state(&sess->ui);
			}

			/* Restore active session's state so globals
			 * are correct between events */
			if (active_session) {
				term_ui_load_state(&active_session->ui);
				session_load_font(active_session);
			}

			break;
		}
		case keyDown:
		case autoKey:
			if (active_session) {
				EventRecord pending;

				handle_key_down(active_session, &event);
				while (GetNextEvent(keyDownMask |
				    autoKeyMask, &pending))
					handle_key_down(active_session,
					    &pending);
				flush_key_send(active_session);
			}
			break;
		case mouseDown:
			handle_mouse_down(&event);
			break;
		case updateEvt:
			handle_update(&event);
			break;
		case activateEvt:
			handle_activate(&event);
			break;
		case app4Evt:
			if (HiWord(event.message) & (1 << 8)) {
				/* MultiFinder suspend/resume */
				if (event.message & 1) {
					/* Resume */
					g_suspended = false;
					if (notification_posted) {
						NMRemove(&nm_rec);
						notification_posted = false;
					}
				} else {
					/* Suspend */
					g_suspended = true;
				}
			}
			break;
		case kHighLevelEvent:
			AEProcessAppleEvent(&event);
			break;
		}
	}

	/* Remove any pending notification */
	if (notification_posted) {
		NMRemove(&nm_rec);
		notification_posted = false;
	}

	/* Clean up all sessions before quit */
	{
		short si;
		Session *sess;

		for (si = MAX_SESSIONS - 1; si >= 0; si--) {
			sess = session_get(si);
			if (sess)
				session_destroy(sess);
		}
	}
	active_session = 0L;

	if (grow_clip_rgn)
		DisposeRgn(grow_clip_rgn);

	ExitToShell();
}

static void
handle_mouse_down(EventRecord *event)
{
	WindowPtr win;
	short part;
	Session *sess;

	part = FindWindow(event->where, &win);

	switch (part) {
	case inMenuBar:
		update_menus();
		handle_menu(MenuSelect(event->where));
		break;
	case inSysWindow:
		SystemClick(event, win);
		break;
	case inDrag:
		DragWindow(win, event->where, &qd.screenBits.bounds);
		break;
	case inGoAway:
		if (TrackGoAway(win, event->where)) {
			sess = session_from_window(win);
			if (sess) {
				if (sess->conn.state ==
				    CONN_STATE_CONNECTED) {
					ParamText(
					    "\pDisconnect and close "
					    "window?",
					    "\p", "\p", "\p");
					if (CautionAlert(128, 0L) != 1)
						break;
				}
				term_ui_load_state(&sess->ui);
				if (sess == active_session)
					active_session = 0L;
				session_destroy(sess);
				/* Update active to new front window */
				if (!active_session) {
					WindowPtr front = FrontWindow();
					active_session =
					    session_from_window(front);
				}
				update_menus();
			}
		}
		break;
	case inGrow: {
		long new_size;
		Rect limit_rect;
		short min_w, min_h, max_w, max_h;

		sess = session_from_window(win);
		if (!sess)
			break;
		session_load_font(sess);

		min_w = LEFT_MARGIN * 2 + MIN_WIN_COLS * g_cell_width;
		min_h = TOP_MARGIN * 2 + MIN_WIN_ROWS * g_cell_height;
		max_w = qd.screenBits.bounds.right - 10;
		max_h = qd.screenBits.bounds.bottom - 10;

		SetRect(&limit_rect, min_w, min_h, max_w, max_h);
		new_size = GrowWindow(win, event->where, &limit_rect);
		if (new_size != 0)
			do_window_resize(sess, LoWord(new_size),
			    HiWord(new_size));
		break;
	}
	case inContent:
		sess = session_from_window(win);
		if (win != FrontWindow()) {
			SelectWindow(win);
			if (sess) {
				if (active_session)
					term_ui_save_state(
					    &active_session->ui);
				active_session = sess;
				term_ui_load_state(&sess->ui);
				session_load_font(sess);
			}
			update_menus();
		} else if (sess) {
			handle_content_click(sess, event);
		}
		break;
	}
}

static void
handle_update(EventRecord *event)
{
	WindowPtr win;
	GrafPtr old_port;
	Session *sess;

	win = (WindowPtr)event->message;
	sess = session_from_window(win);

	GetPort(&old_port);
	SetPort(win);
	BeginUpdate(win);

	clear_window_bg(win, prefs.dark_mode);

	if (sess) {
		term_ui_load_state(&sess->ui);
		session_load_font(sess);

		if (sess->conn.state == CONN_STATE_CONNECTED) {
			if (sess->terminal.window_title[0])
				set_wtitlef(sess->window, "Flynn - %s",
				    sess->terminal.window_title);
			else
				set_wtitlef(sess->window, "Flynn - %s",
				    sess->conn.host);

			/* Mark all rows dirty for full repaints */
			term_dirty_all(&sess->terminal);
			term_ui_draw(sess->window, &sess->terminal);
		} else {
			/* Redraw preserved terminal content
			 * (screen buffer kept on disconnect) */
			term_dirty_all(&sess->terminal);
			term_ui_draw(sess->window, &sess->terminal);
		}

		term_ui_save_state(&sess->ui);
	}

	/* Draw grow icon (clipped to avoid scroll bar track lines) */
	{
		Rect clip_r;

		if (!grow_clip_rgn)
			grow_clip_rgn = NewRgn();
		GetClip(grow_clip_rgn);
		SetRect(&clip_r,
		    win->portRect.right - 15,
		    win->portRect.bottom - 15,
		    win->portRect.right,
		    win->portRect.bottom);
		ClipRect(&clip_r);
		DrawGrowIcon(win);
		SetClip(grow_clip_rgn);
	}

	EndUpdate(win);
	SetPort(old_port);
}

static void
handle_activate(EventRecord *event)
{
	WindowPtr win;
	Session *sess;

	win = (WindowPtr)event->message;
	sess = session_from_window(win);

	if (event->modifiers & activeFlag) {
		if (sess) {
			/* Save outgoing session's UI state (selection, cursor) */
			if (active_session)
				term_ui_save_state(&active_session->ui);
			active_session = sess;
			/* Load incoming session's UI + font state */
			term_ui_load_state(&sess->ui);
			session_load_font(sess);
			update_menus();
		}
	}
}
