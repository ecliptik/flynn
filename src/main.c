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
#include <stdlib.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "telnet.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "settings.h"
#include "dialogs.h"
#include "menus.h"
#include "favorites.h"
#include "input.h"
#include "clipboard.h"
#include "macutil.h"
#include "color.h"
#include "finger.h"
#include "logging.h"
#include "printing.h"
#include "theme.h"

/* Globals */
Boolean running = true;
Boolean g_suspended = false;
FlynnPrefs prefs;
Session *active_session = 0L;

/* Saved system key repeat settings (restored on quit) */
static short saved_key_thresh;
static short saved_key_rep_thresh;

/* Notification Manager */
static NMRec nm_rec;
static Boolean notification_posted = false;

#if FLYNN_SCROLLBACK_LINES > 0
/* Pre-allocated scroll bar action UPP (avoid leak on every click) */
static pascal void scrollbar_action(ControlHandle control, short part);
static ControlActionUPP g_scroll_action_upp = 0L;
#endif

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
static void adjust_cursor(Point mouse_pt);

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

/*
 * GURL Apple Event handler for telnet:// URLs.
 * Allows System 7 web browsers to hand off telnet:// links to Flynn.
 * URL format: telnet://host[:port]
 */
static pascal OSErr
ae_get_url(const AppleEvent *evt, AppleEvent *reply, long refcon)
{
	AEDesc url_desc;
	char url[512];
	long url_len;
	OSErr err;
	char host[256];
	short port = DEFAULT_PORT;
	char *p, *host_start, *port_start;
	Session *s;
	WindowPtr sw;
	char smsg[80];
	short hlen;

#pragma unused(reply, refcon)

	err = AEGetParamDesc(evt, keyDirectObject,
	    typeChar, &url_desc);
	if (err != noErr)
		return err;

	url_len = GetHandleSize(url_desc.dataHandle);
	if (url_len <= 0 || url_len >= (long)sizeof(url)) {
		AEDisposeDesc(&url_desc);
		return errAEEventNotHandled;
	}

	HLock(url_desc.dataHandle);
	memcpy(url, *url_desc.dataHandle, url_len);
	url[url_len] = '\0';
	HUnlock(url_desc.dataHandle);
	AEDisposeDesc(&url_desc);

	/* Parse telnet://host[:port] */
	p = url;
	if (strncmp(p, "telnet://", 9) == 0)
		p += 9;
	else
		return errAEEventNotHandled;

	/* Skip optional user@ (not used) */
	{
		char *at = strchr(p, '@');
		if (at)
			p = at + 1;
	}

	host_start = p;

	/* Find port separator or end */
	port_start = strchr(p, ':');
	if (port_start) {
		hlen = port_start - host_start;
		port = (short)atoi(port_start + 1);
		if (port <= 0)
			port = DEFAULT_PORT;
	} else {
		/* Strip trailing slash */
		hlen = strlen(host_start);
		if (hlen > 0 && host_start[hlen - 1] == '/')
			hlen--;
	}

	if (hlen <= 0 || hlen >= (short)sizeof(host))
		return errAEEventNotHandled;

	memcpy(host, host_start, hlen);
	host[hlen] = '\0';

	/* Create session and connect */
	term_ui_ensure_metrics(prefs.font_id, prefs.font_size);
	s = session_new();
	if (!s)
		return errAEEventNotHandled;
	session_init_from_prefs(s);
	if (active_session &&
	    active_session->conn.state == CONN_STATE_CONNECTED)
		SelectWindow(s->window);
	active_session = s;

	snprintf(smsg, sizeof(smsg), "Connecting to %.50s\311",
	    host);
	sw = conn_status_show(smsg);
	if (conn_connect(&s->conn, host, port, sw)) {
		conn_status_close(sw);
		telnet_init(&s->telnet);
		s->telnet.preferred_ttype = prefs.terminal_type;
		s->telnet.cols = s->terminal.active_cols;
		s->telnet.rows = s->terminal.active_rows;
		terminal_reset(&s->terminal);
		set_wtitlef(s->window, "Flynn - %s", host);
	} else {
		conn_status_close(sw);
		session_destroy_and_fixup(s);
	}
	update_menus();
	return noErr;
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

		/* GURL/GURL: handle telnet:// URLs from browsers */
		AEInstallEventHandler('GURL', 'GURL',
		    NewAEEventHandlerUPP(ae_get_url), 0L, false);
	}
}

int
main(void)
{
	init_toolbox();
	init_apple_events();
	color_detect();
	init_menus();
#if FLYNN_SCROLLBACK_LINES > 0
	g_scroll_action_upp = NewControlActionUPP(scrollbar_action);
#endif

	/* Load prefs and fast init before showing window */
	prefs_load(&prefs);
	theme_init(prefs.theme_id);
	term_ui_set_dark_mode(prefs.dark_mode);

	/* Launch directly into connect dialog (before heavy init).
	 * Font metrics and session creation are deferred until user
	 * clicks OK. MacTCP init is lazy (first conn_connect call). */
	do_connect();

	favorites_rebuild_menu();
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

	/* Auto-stop logging on disconnect */
	log_stop_if_active(sess);

#ifdef FLYNN_FINGER
	/* Finger: server closure is expected.  Skip telnet reset,
	 * snapshot restore, and full redraw — the screen content
	 * was already drawn by session_draw() in the drain loop.
	 * Just update scrollbar, status bar, and menus. */
	if (sess->conn.protocol == PROTO_FINGER) {
#if FLYNN_SCROLLBACK_LINES > 0
		/* Update scrollbar for scrollback access */
		if (sess->scrollbar) {
			short max_val =
			    sess->terminal.sb_count;
			SetControlMaximum(sess->scrollbar,
			    max_val);
			SetControlValue(sess->scrollbar,
			    max_val);
			HiliteControl(sess->scrollbar,
			    max_val > 0 ? 0 : 255);
		}
#endif
		if (prefs.show_status_bar) {
			GrafPtr save;

			GetPort(&save);
			SetPort(sess->window);
			draw_status_bar(sess->window, sess);
			SetPort(save);
		}
		if (sess == active_session)
			update_menus();
		term_ui_save_state(&sess->ui);
		return;
	}
#endif /* FLYNN_FINGER */

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
	if (sess->terminal.snap_valid && was_ttype != 4 &&
	    sess->terminal.snap_screen) {
		short snap_cols = sess->terminal.snap_cols;
		short snap_rows = sess->terminal.snap_rows;
		short r, cols;

		/* Restore row-by-row: snapshot may differ from current
		 * active dimensions if window was resized after snapshot */
		cols = snap_cols < sess->terminal.active_cols ?
		    snap_cols : sess->terminal.active_cols;
		HLock(sess->terminal.snap_screen);
		for (r = 0; r < snap_rows &&
		    r < sess->terminal.active_rows; r++)
			memcpy(sess->terminal.screen_rows[r],
			    *sess->terminal.snap_screen +
			    (long)r * snap_cols * sizeof(TermCell),
			    cols * sizeof(TermCell));
		HUnlock(sess->terminal.snap_screen);
		terminal_normalize_rows(&sess->terminal);

		/* Restore saved color data */
		if (sess->terminal.snap_has_color &&
		    sess->terminal.snap_color &&
		    sess->terminal.has_color &&
		    sess->terminal.screen_color) {
			HLock(sess->terminal.snap_color);
			for (r = 0; r < snap_rows &&
			    r < sess->terminal.active_rows; r++)
				memcpy(
				    sess->terminal.screen_color_rows[r],
				    *sess->terminal.snap_color +
				    (long)r * snap_cols *
				    sizeof(CellColor),
				    cols * sizeof(CellColor));
			HUnlock(sess->terminal.snap_color);
		}
	}

	/* Free snapshot handles — no longer needed after restore */
	sess->terminal.snap_valid = 0;
	if (sess->terminal.snap_screen) {
		DisposeHandle(sess->terminal.snap_screen);
		sess->terminal.snap_screen = 0L;
	}
	if (sess->terminal.snap_color) {
		DisposeHandle(sess->terminal.snap_color);
		sess->terminal.snap_color = 0L;
	}
	sess->terminal.snap_has_color = 0;

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
	if (prefs.show_status_bar)
		draw_status_bar(sess->window, sess);
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
		/* Show disconnect alert with Reconnect option
		 * if the session has a host to reconnect to */
		if (sess->conn.host[0]) {
			short alert_item;

			ParamText(
			    "\pConnection closed by remote host",
			    "\p", "\p", "\p");
			alert_item = NoteAlert(DLOG_DISCONN_ID,
			    0L);
			if (alert_item == 2) {
				/* Reconnect */
				do_reconnect();
			}
		} else {
			ParamText(
			    "\pConnection closed by remote host",
			    "\p", "\p", "\p");
			NoteAlert(128, 0L);
		}
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

#ifdef FLYNN_FINGER
	/* Finger: data is handled synchronously in finger_connect(),
	 * so no data should arrive here.  Safety drain only. */
	if (sess->conn.protocol == PROTO_FINGER) {
		sess->conn.read_len = 0;
		return;
	}
#endif

	telnet_process(&sess->telnet,
	    (unsigned char *)sess->conn.read_buf,
	    sess->conn.read_len,
	    out_buf, &out_len,
	    send_buf, &send_len);

	if (send_len > 0)
		conn_send(&sess->conn, (char *)send_buf, send_len);

	if (out_len > 0) {
		short offset = 0;

		/* Log raw terminal output before processing */
		log_write_data(sess, out_buf, out_len);

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

			/* Draw when scroll accumulates past quarter
			 * screen, or when significant data processed.
			 * Smaller batches = smoother visual output
			 * with 2x more CopyBits but fewer dirty rows
			 * per blit (net render time unchanged). */
			if (offset < out_len &&
			    sess->terminal.scroll_pending &&
			    (sess->terminal.scroll_count >=
			    sess->terminal.active_rows / 4 ||
			    offset >= 2048)) {
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
	session_update_scrollbar(sess);
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

/*
 * adjust_cursor - set cursor to iBeam over terminal content, arrow elsewhere
 */
static void
adjust_cursor(Point mouse_pt)
{
	WindowPtr win;
	short part;

	if (g_suspended) {
		InitCursor();
		return;
	}

	win = FrontWindow();
	if (!win) {
		InitCursor();
		return;
	}

	part = FindWindow(mouse_pt, &win);
	if (part == inContent && win == FrontWindow()) {
		Session *sess;

		sess = session_from_window(win);
		if (sess) {
			Point local_pt;
			GrafPtr save;
			Rect content_r;

			GetPort(&save);
			SetPort(win);
			local_pt = mouse_pt;
			GlobalToLocal(&local_pt);
			SetPort(save);

			/* Text area excludes scroll bar */
			content_r = win->portRect;
			content_r.right -= SCROLLBAR_WIDTH;

			if (PtInRect(local_pt, &content_r)) {
				SetCursor(*GetCursor(iBeamCursor));
				return;
			}
		}
	}

	InitCursor();
}

/*
 * drain_one - Run one iteration of the data drain loop for a session.
 * Calls conn_idle, processes any received data, and detects disconnect.
 * Returns: 1=data processed, 0=no data, -1=disconnected
 */
static short
drain_one(Session *sess)
{
	short prev_state;
	short had_data = 0;

	prev_state = sess->conn.state;
	conn_idle(&sess->conn);

	/* Process any data read before checking for disconnect —
	 * server may close after sending final data */
	if (sess->conn.read_len > 0) {
		session_process_data(sess);
		had_data = 1;
	}

	if (prev_state == CONN_STATE_CONNECTED &&
	    sess->conn.state == CONN_STATE_IDLE) {
		session_handle_disconnect(sess);
		return -1;
	}

	return had_data;
}

/*
 * check_keepalive - Send IAC NOP if session has been idle > 120s (7200 ticks).
 */
static void
check_keepalive(Session *sess)
{
	unsigned char nop_buf[4];
	short nop_len = 0;

	if (sess->conn.state != CONN_STATE_CONNECTED)
		return;
	if (sess->conn.last_send_tick <= 0)
		return;
	if ((TickCount() - sess->conn.last_send_tick) < 7200)
		return;

	telnet_send_nop(nop_buf, &nop_len);
	conn_send(&sess->conn, (char *)nop_buf, nop_len);
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
			static unsigned short bg_tick = 0;
			static Point last_mouse_pt = { -1, -1 };

			/* Save user interaction state (selection, cursor)
			 * before cycling through sessions */
			if (active_session)
				term_ui_save_state(&active_session->ui);

			/* Fast path: single session skips save/load cycling */
			if (session_count() == 1 && active_session) {
				short drain, rc;
				long draw_deadline = 0;

				check_keepalive(active_session);

				/* Jump scroll: suppress draws while
				 * TCP data is still arriving.  Draw
				 * when stream pauses or after 4 ticks
				 * (68ms). */
			drain_jump:
				drain = 0;
				do {
					rc = drain_one(active_session);
					if (rc == 1)
						drain++;
					if (rc != 1) /* no data or disconnect */
						break;
				} while (drain < 8);

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
							    + 2;
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
				if (event.where.h != last_mouse_pt.h ||
				    event.where.v != last_mouse_pt.v) {
					last_mouse_pt = event.where;
					adjust_cursor(event.where);
				}
				break;
			}

			bg_tick++;

			/* NOP keep-alive: send IAC NOP on
			 * connected sessions idle > 120s. */
			for (si = 0; si < MAX_SESSIONS; si++) {
				sess = session_get(si);
				if (!sess)
					continue;
				check_keepalive(sess);
			}

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

				/* Load this session's UI + font + theme */
				term_ui_load_state(&sess->ui);
				session_load_font(sess);
				session_load_settings(sess);

				{
					short drain, rc;
					long draw_deadline = 0;

					/* Jump scroll: suppress draws
					 * while TCP data arriving.
					 * Draw when stream pauses or
					 * 4-tick deadline expires. */
				drain_bg:
					drain = 0;
					do {
						rc = drain_one(sess);
						if (rc == 1)
							drain++;
						if (rc != 1)
							break;
					} while (drain < 8);

					if (drain > 0) {
						had_data = 1;
						if (!g_has_color_qd) {
							if (!draw_deadline)
								draw_deadline =
								    TickCount()
								    + 2;
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
				session_load_settings(active_session);
			}

			if (event.where.h != last_mouse_pt.h ||
			    event.where.v != last_mouse_pt.v) {
				last_mouse_pt = event.where;
				adjust_cursor(event.where);
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
	session_destroy_all();
	active_session = 0L;

	/* Restore system key repeat settings */
	LMSetKeyThresh(saved_key_thresh);
	LMSetKeyRepThresh(saved_key_rep_thresh);

	term_ui_cleanup();
	ExitToShell();
}

#if FLYNN_SCROLLBACK_LINES > 0
/*
 * scrollbar_action - TrackControl action proc for scroll bar arrows
 * and page areas.  Called repeatedly while mouse is held down.
 */
static pascal void
scrollbar_action(ControlHandle control, short part)
{
	Session *sess;
	Terminal *term;
	short new_offset;

	sess = active_session;
	if (!sess || control != sess->scrollbar)
		return;
	term = &sess->terminal;

	new_offset = term->scroll_offset;

	switch (part) {
	case inUpButton:
		new_offset++;
		break;
	case inDownButton:
		new_offset--;
		break;
	case inPageUp:
		new_offset += term->active_rows;
		break;
	case inPageDown:
		new_offset -= term->active_rows;
		break;
	default:
		return;
	}

	if (new_offset < 0)
		new_offset = 0;
	if (new_offset > term->sb_count)
		new_offset = term->sb_count;

	if (new_offset != term->scroll_offset) {
		short delta = new_offset - term->scroll_offset;
		short new_row;

		term->scroll_offset = new_offset;
		SetControlValue(control,
		    term->sb_count - new_offset);

		/* Single-line scroll: ScrollRect shifts screen
		 * pixels, memmove shifts offscreen to match,
		 * then only the 1 new row needs rendering.
		 * ~24x faster than full 24-row redraw. */
		if ((delta == 1 || delta == -1) &&
		    (new_row = term_ui_scroll_offscreen(
		    sess->window, delta,
		    term->active_rows)) >= 0) {
			GrafPtr save;
			Rect content_r;
			RgnHandle upd_rgn;
			short shift;

			GetPort(&save);
			SetPort(sess->window);

			/* Shift screen pixels via ScrollRect */
			shift = (delta > 0) ?
			    g_cell_height : -g_cell_height;
			SetRect(&content_r, 0, 0,
			    sess->window->portRect.right -
			    SCROLLBAR_WIDTH,
			    term->active_rows *
			    g_cell_height);
			upd_rgn = NewRgn();
			ScrollRect(&content_r, 0, shift,
			    upd_rgn);
			DisposeRgn(upd_rgn);

			SetPort(save);

			/* Render only the 1 exposed row */
			term->dirty[new_row] = 1;
			term_ui_draw(sess->window, term);
		} else {
			/* Page scroll or no offscreen: full
			 * redraw */
			term_dirty_all(term);
			term_ui_draw(sess->window, term);
		}
	}
}
#endif /* FLYNN_SCROLLBACK_LINES > 0 */

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
			if (win == clipboard_window_ptr()) {
				clipboard_window_close();
				break;
			}
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
				session_destroy_and_fixup(sess);
				update_menus();
			}
		}
		break;
	case inZoomIn:
	case inZoomOut:
		sess = session_from_window(win);
		if (sess && TrackBox(win, event->where, part)) {
			GrafPtr save;
			short std_w, std_h;

			session_load_font(sess);

			/* Set standard state to default 80x24 grid */
			std_w = LEFT_MARGIN * 2 +
			    TERM_DEFAULT_COLS * g_cell_width +
			    SCROLLBAR_WIDTH;
			std_h = status_bar_height() +
			    TERM_DEFAULT_ROWS * g_cell_height;
			{
				WStateData **wstate;
				wstate = (WStateData **)
				    ((WindowPeek)win)->dataHandle;
				if (wstate) {
					(**wstate).stdState.left = 2;
					(**wstate).stdState.top = 40;
					(**wstate).stdState.right =
					    2 + std_w;
					(**wstate).stdState.bottom =
					    40 + std_h;
				}
			}

			GetPort(&save);
			SetPort(win);
			EraseRect(&win->portRect);
			ZoomWindow(win, part, false);
			do_window_resize(sess,
			    win->portRect.right - win->portRect.left,
			    win->portRect.bottom - win->portRect.top);
			SetPort(save);
		}
		break;
	case inGrow: {
		long new_size;
		Rect limit_rect;
		short min_w, min_h, max_w, max_h;

		if (win == clipboard_window_ptr()) {
			clipboard_window_grow(win, event->where);
			break;
		}
		sess = session_from_window(win);
		if (!sess)
			break;
		session_load_font(sess);

		min_w = LEFT_MARGIN * 2 + MIN_WIN_COLS * g_cell_width +
		    SCROLLBAR_WIDTH;
		min_h = status_bar_height() +
		    MIN_WIN_ROWS * g_cell_height;
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
		if (win == clipboard_window_ptr()) {
			if (win != FrontWindow())
				SelectWindow(win);
			else
				clipboard_window_click(win, event->where);
			break;
		}
		sess = session_from_window(win);
		if (win != FrontWindow()) {
			SelectWindow(win);
			if (sess) {
				if (active_session) {
					term_ui_save_state(
					    &active_session->ui);
					session_save_settings(
					    active_session);
				}
				active_session = sess;
				term_ui_load_state(&sess->ui);
				session_load_font(sess);
				session_load_settings(sess);
			}
			update_menus();
		} else if (sess) {
			/* Check if click is on scroll bar control */
			ControlHandle hit_ctl;
			Point local_pt;
			short ctl_part;
			GrafPtr save;

			GetPort(&save);
			SetPort(win);
			local_pt = event->where;
			GlobalToLocal(&local_pt);
			ctl_part = FindControl(local_pt, win,
			    &hit_ctl);
			SetPort(save);

#if FLYNN_SCROLLBACK_LINES > 0
			if (ctl_part && hit_ctl == sess->scrollbar) {
				SetPort(win);
				if (ctl_part == inThumb) {
					TrackControl(hit_ctl,
					    local_pt, 0L);
					sess->terminal.scroll_offset =
					    sess->terminal.sb_count -
					    GetControlValue(
					    hit_ctl);
					term_dirty_all(
					    &sess->terminal);
					term_ui_draw(sess->window,
					    &sess->terminal);
				} else {
					TrackControl(hit_ctl,
					    local_pt,
					    g_scroll_action_upp);
				}
				SetPort(save);
			} else
#endif
			{
				handle_content_click(sess, event);
			}
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

	/* Clipboard viewer window */
	if (win == clipboard_window_ptr()) {
		GetPort(&old_port);
		SetPort(win);
		BeginUpdate(win);
		clipboard_window_update(win);
		EndUpdate(win);
		SetPort(old_port);
		return;
	}

	sess = session_from_window(win);

	GetPort(&old_port);
	SetPort(win);
	BeginUpdate(win);

	if (sess) {
		term_ui_load_state(&sess->ui);
		session_load_font(sess);
		session_load_settings(sess);

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
			/* Fallback: recreate offscreen from scratch.
			 * Dirty ALL rows so the entire offscreen is
			 * fully populated — partial dirty leaves
			 * white holes that corrupt future fast-path
			 * blits. */
			clear_window_bg(win, theme_is_dark());
			term_ui_invalidate_offscreen();
			term_dirty_all(&sess->terminal);
			term_ui_draw(sess->window,
			    &sess->terminal);
		}

		if (prefs.show_status_bar)
			draw_status_bar(win, sess);
		term_ui_save_state(&sess->ui);
	} else {
		clear_window_bg(win, prefs.dark_mode);
	}

	/* Draw scroll bar column and grow box.
	 * First erase the entire right column to eliminate
	 * stray pixels between content, scroll bar, and
	 * grow box.  Then draw controls on top. */
	{
		Rect clip_r;
		RgnHandle save_clip;

		/* Restore port to standard black/white before drawing
		 * system controls — themed backColor leaks into
		 * scrollbar and grow box rendering otherwise. */
#ifdef FLYNN_COLOR
		if (g_has_color_qd) {
			RGBColor black_c = {0, 0, 0};
			RGBColor white_c = {0xFFFF, 0xFFFF, 0xFFFF};
			RGBForeColor(&black_c);
			RGBBackColor(&white_c);
		}
#endif

		/* Erase entire scroll bar + grow box column */
		SetRect(&clip_r,
		    win->portRect.right - SCROLLBAR_WIDTH,
		    0,
		    win->portRect.right,
		    win->portRect.bottom);
		if (sess && theme_is_dark())
			PaintRect(&clip_r);
		else
			EraseRect(&clip_r);

		/* Draw grow icon clipped to bottom-right corner */
		save_clip = NewRgn();
		GetClip(save_clip);
		SetRect(&clip_r,
		    win->portRect.right - SCROLLBAR_WIDTH,
		    win->portRect.bottom - SCROLLBAR_WIDTH,
		    win->portRect.right + 1,
		    win->portRect.bottom + 1);
		ClipRect(&clip_r);
		DrawGrowIcon(win);
		SetClip(save_clip);
		DisposeRgn(save_clip);

		/* Draw scroll bar on top */
		DrawControls(win);
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
			/* Save outgoing session's UI + settings state */
			if (active_session) {
				term_ui_save_state(&active_session->ui);
				session_save_settings(active_session);
			}
			active_session = sess;
			/* Load incoming session's UI + font + settings */
			term_ui_load_state(&sess->ui);
			session_load_font(sess);
			session_load_settings(sess);
			update_menus();
#if FLYNN_SCROLLBACK_LINES > 0
			/* Activate scroll bar */
			if (sess->scrollbar)
				HiliteControl(sess->scrollbar,
				    sess->terminal.sb_count > 0 ?
				    0 : 255);
#endif
		}
	} else {
#if FLYNN_SCROLLBACK_LINES > 0
		/* Deactivate scroll bar */
		if (sess && sess->scrollbar)
			HiliteControl(sess->scrollbar, 255);
#endif
	}
}
