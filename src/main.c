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
#include "color.h"

/* Globals */
Boolean running = true;
Boolean g_suspended = false;
FlynnPrefs prefs;
static RgnHandle grow_clip_rgn = 0L;
Session *active_session = 0L;

/* Saved system key repeat settings (restored on quit) */
static short saved_key_thresh;
static short saved_key_rep_thresh;

/* Notification Manager */
static NMRec nm_rec;
static Boolean notification_posted = false;

/* Shared buffers for telnet/terminal processing (static to avoid stack) */
static unsigned char out_buf[TCP_READ_BUFSIZ];
static unsigned char send_buf[TCP_READ_BUFSIZ];

/* (Screen snapshot moved to Terminal struct — saved on full clear only) */

/* Forward declarations */
static void init_toolbox(void);
static void init_apple_events(void);
static void main_event_loop(void);
static void handle_mouse_down(EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void session_handle_disconnect(Session *sess);
static void session_poll_data(Session *sess);
static void session_process_data(Session *sess);
static void session_draw(Session *sess);
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
	color_detect();
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

	/* Save system key repeat and set fast repeat for terminal use.
	 * Default PRAM is often ~18 ticks (300ms) between repeats.
	 * We set 2 ticks (33ms) ≈ 30 cps for responsive typing. */
	saved_key_thresh = LMGetKeyThresh();
	saved_key_rep_thresh = LMGetKeyRepThresh();
	LMSetKeyThresh(12);		/* 200ms initial delay */
	LMSetKeyRepThresh(2);		/* 33ms repeat = ~30 cps */

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
	short was_ttype;

	/* Save session's terminal type before telnet_init() zeroes it.
	 * Needed to decide whether to restore snapshot (xterm/VT types
	 * clear screen on logout) or keep current screen (ANSI-BBS). */
	was_ttype = sess->telnet.preferred_ttype;

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

	/* Restore pre-clear screen content for xterm/VT types, which
	 * send ESC[2J during logout leaving a blank screen.
	 * For ANSI (BBS), keep the current screen — we now drain TCP
	 * data before closing, so goodbye screens are already rendered.
	 * Use saved ttype since telnet_init() resets to global pref. */
	if (sess->terminal.snap_valid && was_ttype != 4) {
		memcpy(sess->terminal.screen, sess->terminal.snap_screen,
		    sizeof(sess->terminal.screen));
	}
	sess->terminal.snap_valid = 0;

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
 * session_process_data - process TCP data through telnet and terminal
 * without drawing.  Used by drain loop to batch multiple reads before
 * a single draw pass.
 */
static void
session_process_data(Session *sess)
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
		short offset = 0;

		if (sess->terminal.scroll_offset > 0) {
			sess->terminal.scroll_offset = 0;
			term_dirty_all(&sess->terminal);
		}

		/* Set connection for immediate response flush */
		sess->terminal.resp_conn = &sess->conn;

		/*
		 * Process terminal data in chunks with intermediate
		 * draws.  This keeps scroll_count low so ScrollRect
		 * stays effective — shifting 18+ rows via blit and
		 * only redrawing 4-6 new rows, instead of falling
		 * back to a full 24-row redraw.
		 */
		while (offset < out_len) {
			short chunk = out_len - offset;
			if (chunk > 512)
				chunk = 512;
			terminal_process(&sess->terminal,
			    out_buf + offset, chunk);
			offset += chunk;

			/* Draw when scroll accumulates past half
			 * the screen.  Batches more scroll before
			 * drawing — ScrollRect handles large
			 * offsets as efficiently as small ones. */
			if (offset < out_len &&
			    sess->terminal.scroll_pending &&
			    sess->terminal.scroll_count >=
			    sess->terminal.active_rows / 2) {
				session_draw(sess);
			}
		}

		sess->terminal.resp_conn = 0L;

		/* Update window title */
		if (sess->terminal.title_changed)
			session_update_title(sess);
	}

	sess->conn.read_len = 0;
}

/*
 * session_draw - render dirty terminal rows to the session window.
 */
static void
session_draw(Session *sess)
{
	GrafPtr save;

	GetPort(&save);
	SetPort(sess->window);
	term_ui_draw(sess->window, &sess->terminal);
	SetPort(save);
}

/*
 * session_poll_data - process incoming TCP data and draw.
 * Convenience wrapper for single-read paths.
 */
static void
session_poll_data(Session *sess)
{
	session_process_data(sess);
	session_draw(sess);
}

static void
main_event_loop(void)
{
	EventRecord event;
	long wait_ticks;
	short had_data = 0;	/* nonzero = data was processed last tick */

	while (running) {
		if (g_suspended)
			wait_ticks = 60L;
		else if (had_data)
			wait_ticks = 0L;	/* don't sleep — more data likely */
		else
			wait_ticks = session_any_connected() ? 1L : 10L;
		had_data = 0;
		WaitNextEvent(everyEvent, &event, wait_ticks, 0L);

		switch (event.what) {
		case nullEvent:
		{
			short si;
			Session *sess;
			short prev_state;
			static unsigned short bg_tick = 0;

			/* Save user interaction state (selection, cursor)
			 * before cycling through sessions */
			if (active_session)
				term_ui_save_state(&active_session->ui);

			/* Fast path: single session skips save/load cycling */
			if (session_count() == 1 && active_session) {
				short drain;
				long draw_deadline = 0;

				/* Jump scroll: suppress draws while
				 * TCP data is still arriving.  Draw
				 * when stream pauses or after 4 ticks
				 * (68ms). */
			drain_jump:
				drain = 0;
				do {
					prev_state =
					    active_session->conn.state;
					conn_idle(&active_session->conn);

					if (prev_state ==
					    CONN_STATE_CONNECTED &&
					    active_session->conn.state ==
					    CONN_STATE_IDLE) {
						session_handle_disconnect(
						    active_session);
						break;
					}

					if (active_session->conn.read_len
					    == 0)
						break;

					session_process_data(
					    active_session);
					drain++;
				} while (drain < 16);

				if (drain > 0) {
					had_data = 1;
					/* Jump scroll: suppress draws
					 * while data arriving (mono
					 * only — color draws are too
					 * expensive without offscreen,
					 * would starve event loop) */
					if (!g_has_color_qd) {
						if (!draw_deadline)
							draw_deadline =
							    TickCount()
							    + 4;
						if (active_session
						    ->conn
						    .pending_data > 0
						    && TickCount() <
						    draw_deadline)
							goto
							    drain_jump;
					}
					session_draw(active_session);
					draw_deadline = 0;
				}

				if (!g_suspended &&
				    active_session->conn.state ==
				    CONN_STATE_CONNECTED) {
					GrafPtr save;

					GetPort(&save);
					SetPort(active_session->window);
					term_ui_cursor_blink(
					    active_session->window,
					    &active_session->terminal);
					SetPort(save);
				}
				break;
			}

			bg_tick++;
			for (si = 0; si < MAX_SESSIONS; si++) {
				sess = session_get(si);
				if (!sess)
					continue;

				/* Skip expensive UI/font swap for
				 * disconnected sessions — just run
				 * conn_idle for cleanup */
				if (sess->conn.state !=
				    CONN_STATE_CONNECTED) {
					conn_idle(&sess->conn);
					continue;
				}

				/* Background: skip 3/4 ticks when
				 * idle to reduce UI/font swap
				 * overhead (~0.6ms per session) */
				if (sess != active_session &&
				    (bg_tick & 3) != 0 &&
				    sess->conn.pending_data == 0)
					continue;

				/* Load this session's UI + font state */
				term_ui_load_state(&sess->ui);
				session_load_font(sess);

				{
					short drain;
					long draw_deadline = 0;

					/* Jump scroll: suppress draws
					 * while TCP data arriving.
					 * Draw when stream pauses or
					 * 4-tick deadline expires. */
				drain_bg:
					drain = 0;
					do {
						prev_state =
						    sess->conn.state;
						conn_idle(&sess->conn);

						if (prev_state ==
						    CONN_STATE_CONNECTED
						    && sess->conn.state
						    ==
						    CONN_STATE_IDLE) {
							session_handle_disconnect(
							    sess);
							break;
						}

						if (sess->conn.read_len
						    == 0)
							break;

						session_process_data(
						    sess);
						drain++;
					} while (drain < 8);

					if (drain > 0) {
						had_data = 1;
						if (!g_has_color_qd) {
							if (!draw_deadline)
								draw_deadline =
								    TickCount()
								    + 4;
							if (sess->conn
							    .pending_data
							    > 0
							    && TickCount()
							    <
							    draw_deadline)
								goto
								    drain_bg;
						}
						session_draw(sess);
						draw_deadline = 0;
					}
				}

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

				/* Echo poll: tight loop with 2-tick
				 * (33ms) budget to catch server echo
				 * on LAN-speed connections. */
				if (active_session->conn.state ==
				    CONN_STATE_CONNECTED) {
					unsigned long deadline;

					deadline = TickCount() + 2;
					do {
						conn_idle(
						    &active_session->conn);
						if (active_session->conn
						    .read_len > 0) {
							session_process_data(
							    active_session);
							break;
						}
					} while (TickCount() < deadline);
				}

				/* Draw locally-echoed or server-
				 * echoed chars.  No-op if no
				 * dirty rows. */
				session_draw(active_session);

				/* Next WNE returns immediately
				 * for echo data arriving after */
				had_data = 1;
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

	/* Restore system key repeat settings */
	LMSetKeyThresh(saved_key_thresh);
	LMSetKeyRepThresh(saved_key_rep_thresh);

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

	if (sess) {
		term_ui_load_state(&sess->ui);
		session_load_font(sess);

		if (sess->conn.state == CONN_STATE_CONNECTED &&
		    sess->terminal.window_title[0])
			set_wtitlef(sess->window, "Flynn - %s",
			    sess->terminal.window_title);
		else if (sess->conn.state == CONN_STATE_CONNECTED)
			set_wtitlef(sess->window, "Flynn - %s",
			    sess->conn.host);

		/* Fast path: blit from cached offscreen buffer.
		 * Avoids full clear_window_bg + redraw (~8x faster).
		 * Cursor is not in offscreen; cursor_blink() will
		 * redraw it on the next idle tick. */
		if (term_ui_has_offscreen(sess->window,
		    sess->terminal.active_cols,
		    sess->terminal.active_rows)) {
			term_ui_blit_offscreen(sess->window);
		} else {
			/* Fallback: redraw via offscreen.
			 * No clear_window_bg — term_ui_draw handles
			 * erase+draw in offscreen then blits.
			 * Use visRgn to only dirty exposed rows. */
			{
				Rect clip_box;
				short first_row, last_row, r;

				clip_box =
				    (**(win->visRgn)).rgnBBox;
				first_row =
				    (clip_box.top - TOP_MARGIN) /
				    g_cell_height;
				last_row =
				    (clip_box.bottom - TOP_MARGIN +
				    g_cell_height - 1) /
				    g_cell_height;

				if (first_row < 0)
					first_row = 0;
				if (last_row >=
				    sess->terminal.active_rows)
					last_row =
					    sess->terminal.active_rows
					    - 1;

				for (r = first_row; r <= last_row;
				    r++)
					sess->terminal.dirty[r] = 1;
			}
			term_ui_draw(sess->window,
			    &sess->terminal);
		}

		term_ui_save_state(&sess->ui);
	} else {
		clear_window_bg(win, prefs.dark_mode);
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
