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
#include "glyphs.h"

/* Globals */
static MenuHandle apple_menu, file_menu, edit_menu, prefs_menu, ctrl_menu;
static MenuHandle window_menu;
static Boolean running = true;
static FlynnPrefs prefs;
static RgnHandle grow_clip_rgn = 0L;
static Session *active_session = 0L;

static void
buffer_key_send(Session *s, const char *data, short len)
{
	short i;

	for (i = 0; i < len && s->key_send_len < (short)sizeof(s->key_send_buf);
	    i++)
		s->key_send_buf[s->key_send_len++] = data[i];
}

static void
flush_key_send(Session *s)
{
	if (s->key_send_len > 0 && s->conn.state == CONN_STATE_CONNECTED) {
		conn_send(&s->conn, s->key_send_buf, s->key_send_len);
		s->key_send_len = 0;
	} else {
		s->key_send_len = 0;
	}
}

/* Forward declarations */
static void init_toolbox(void);
static void init_menus(void);
static void update_menus(void);
static void update_window_menu(void);
static void update_prefs_menu(void);
static void rebuild_bookmark_menu(void);
static void main_event_loop(void);
static Boolean handle_menu(long menu_id);
static void handle_mouse_down(EventRecord *event);
static void handle_key_down(Session *s, EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void do_about(void);
static void do_connect(void);
static void do_connect_bookmark(short index);
static void do_disconnect(void);
static void do_bookmarks(void);
static void do_font_change(short font_id, short font_size);
static void do_window_resize(Session *s, short width, short height);
static void do_copy(void);
static void do_paste(void);
static void do_select_all(void);
static void do_dns_server_dialog(void);
static short pixel_to_row(Session *s, short v);
static short pixel_to_col(Session *s, short h);
static void handle_content_click(Session *s, EventRecord *event);
static void track_selection_drag(Session *s);

int
main(void)
{
	init_toolbox();
	init_menus();

	/* Load prefs and fast init before showing window */
	prefs_load(&prefs);
	term_ui_set_dark_mode(prefs.dark_mode);

	/* Create first session */
	active_session = session_new();
	if (!active_session) {
		SysBeep(10);
		ExitToShell();
	}

	SetPort(active_session->window);
	term_ui_set_font(active_session->window, prefs.font_id,
	    prefs.font_size);

	/* Compute grid and size window to fit */
	active_session->terminal.active_cols =
	    (MAX_WIN_WIDTH - LEFT_MARGIN * 2) / g_cell_width;
	active_session->terminal.active_rows =
	    (MAX_WIN_HEIGHT - TOP_MARGIN * 2) / g_cell_height;
	if (active_session->terminal.active_cols > TERM_DEFAULT_COLS)
		active_session->terminal.active_cols = TERM_DEFAULT_COLS;
	if (active_session->terminal.active_rows > TERM_DEFAULT_ROWS)
		active_session->terminal.active_rows = TERM_DEFAULT_ROWS;
	active_session->terminal.scroll_bottom =
	    active_session->terminal.active_rows - 1;

	SizeWindow(active_session->window,
	    LEFT_MARGIN * 2 +
	    active_session->terminal.active_cols * g_cell_width,
	    TOP_MARGIN * 2 +
	    active_session->terminal.active_rows * g_cell_height, true);

	term_ui_init(active_session->window, &active_session->terminal);

	active_session->telnet.preferred_ttype = prefs.terminal_type;
	active_session->telnet.cols = active_session->terminal.active_cols;
	active_session->telnet.rows = active_session->terminal.active_rows;

	if (prefs.dark_mode)
		PaintRect(&active_session->window->portRect);

	rebuild_bookmark_menu();
	update_menus();
	update_prefs_menu();

	/* Slow MacTCP init — window is already visible */
	conn_init();
	active_session->conn.dns_server = ip2long(prefs.dns_server);

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

static void
init_menus(void)
{
	Handle mbar;

	mbar = GetNewMBar(MBAR_ID);
	if (!mbar) {
		SysBeep(10);
		ExitToShell();
	}

	SetMenuBar(mbar);
	DisposeHandle(mbar);

	apple_menu = GetMenuHandle(APPLE_MENU_ID);
	if (apple_menu)
		AppendResMenu(apple_menu, 'DRVR');

	file_menu = GetMenuHandle(FILE_MENU_ID);
	edit_menu = GetMenuHandle(EDIT_MENU_ID);
	prefs_menu = GetMenuHandle(PREFS_MENU_ID);
	ctrl_menu = GetMenuHandle(CTRL_MENU_ID);
	window_menu = GetMenuHandle(WINDOW_MENU_ID);

	/* Disable section header items */
	if (prefs_menu) {
		DisableItem(prefs_menu, PREFS_FONTS_HDR);
		DisableItem(prefs_menu, PREFS_TTYPE_HDR);
		DisableItem(prefs_menu, PREFS_NET_HDR);
		DisableItem(prefs_menu, PREFS_MISC_HDR);
	}

	DrawMenuBar();
}

static void
update_menus(void)
{
	Boolean connected;
	short i;

	connected = (active_session &&
	    active_session->conn.state == CONN_STATE_CONNECTED);

	/* Session menu: Connect vs Disconnect */
	if (connected) {
		/* When connected, Connect opens a new session window */
		EnableItem(file_menu, FILE_MENU_CONNECT_ID);
		EnableItem(file_menu, FILE_MENU_DISCONNECT_ID);
	} else {
		EnableItem(file_menu, FILE_MENU_CONNECT_ID);
		DisableItem(file_menu, FILE_MENU_DISCONNECT_ID);
	}

	/* Edit menu: Copy when selection active, Paste when connected */
	if (active_session)
		term_ui_load_state(&active_session->ui);
	if (term_ui_sel_active())
		EnableItem(edit_menu, EDIT_MENU_COPY_ID);
	else
		DisableItem(edit_menu, EDIT_MENU_COPY_ID);
	if (connected)
		EnableItem(edit_menu, EDIT_MENU_PASTE_ID);
	else
		DisableItem(edit_menu, EDIT_MENU_PASTE_ID);
	if (connected)
		EnableItem(edit_menu, EDIT_MENU_SELALL_ID);
	else
		DisableItem(edit_menu, EDIT_MENU_SELALL_ID);

	/* Control menu: enable only when connected */
	if (ctrl_menu) {
		short ci;

		for (ci = CTRL_MENU_CTRLC; ci <= CTRL_MENU_BREAK; ci++) {
			if (connected)
				EnableItem(ctrl_menu, ci);
			else
				DisableItem(ctrl_menu, ci);
		}
	}

	/* Bookmark menu items: enable when active session is disconnected */
	for (i = 0; i < prefs.bookmark_count; i++) {
		if (connected)
			DisableItem(file_menu,
			    FILE_MENU_BM_BASE + i);
		else
			EnableItem(file_menu,
			    FILE_MENU_BM_BASE + i);
	}

	/* Window menu: Close only when there's an active session */
	if (window_menu) {
		if (active_session)
			EnableItem(window_menu, WIN_MENU_CLOSE);
		else
			DisableItem(window_menu, WIN_MENU_CLOSE);

	}

	update_window_menu();
}

static void
update_window_menu(void)
{
	short count, i, item;
	Str255 title;
	Session *s;

	if (!window_menu)
		return;

	/* Remove dynamic items (after separator at position 3) */
	count = CountMItems(window_menu);
	while (count > WIN_MENU_FIRST_WIN - 1) {
		DeleteMenuItem(window_menu, count);
		count--;
	}

	/* Append one item per session */
	for (i = 0; i < MAX_SESSIONS; i++) {
		s = session_get(i);
		if (!s)
			continue;
		GetWTitle(s->window, title);
		AppendMenu(window_menu, "\p ");
		item = CountMItems(window_menu);
		SetMenuItemText(window_menu, item, title);
		if (s == active_session)
			CheckItem(window_menu, item, true);
	}
}

static void
rebuild_bookmark_menu(void)
{
	short count, i;
	Str255 item_str;
	short len;

	/* Remove old bookmark items (items after Quit) */
	count = CountMItems(file_menu);
	while (count > FILE_MENU_QUIT_ID) {
		DeleteMenuItem(file_menu, count);
		count--;
	}

	/* Append bookmark entries */
	for (i = 0; i < prefs.bookmark_count; i++) {
		/* AppendMenu with placeholder, then SetMenuItemText
		 * to avoid metacharacter interpretation */
		AppendMenu(file_menu, "\p ");
		len = strlen(prefs.bookmarks[i].name);
		if (len > 255) len = 255;
		item_str[0] = len;
		memcpy(&item_str[1], prefs.bookmarks[i].name, len);
		SetMenuItemText(file_menu,
		    FILE_MENU_BM_BASE + i, item_str);
	}
}

static void
main_event_loop(void)
{
	EventRecord event;
	long wait_ticks;
	static unsigned char out_buf[TCP_READ_BUFSIZ];
	static unsigned char send_buf[TCP_READ_BUFSIZ];

	while (running) {
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

				/* Load this session's UI state */
				term_ui_load_state(&sess->ui);

				prev_state = sess->conn.state;
				conn_idle(&sess->conn);

				/* Detect remote disconnect */
				if (prev_state == CONN_STATE_CONNECTED &&
				    sess->conn.state == CONN_STATE_IDLE) {
					short ri;
					GrafPtr save;

					terminal_reset(&sess->terminal);
					telnet_init(&sess->telnet);
					sess->telnet.preferred_ttype =
					    prefs.terminal_type;
					sess->key_send_len = 0;
					SetWTitle(sess->window, "\pFlynn");

					GetPort(&save);
					SetPort(sess->window);
					if (prefs.dark_mode)
						PaintRect(
						    &sess->window->portRect);
					else
						EraseRect(
						    &sess->window->portRect);
					for (ri = 0;
					    ri < sess->terminal.active_rows;
					    ri++)
						sess->terminal.dirty[ri] = 1;
					term_ui_draw(sess->window,
					    &sess->terminal);
					SetPort(save);

					if (sess == active_session)
						update_menus();

					term_ui_save_state(&sess->ui);

					/* Show alert only for active session */
					if (sess == active_session) {
						ParamText(
						    "\pConnection closed "
						    "by remote host",
						    "\p", "\p", "\p");
						Alert(128, 0L);
					}
					continue;
				}

				/* Process incoming data */
				if (sess->conn.read_len > 0) {
					short out_len = 0;
					short send_len = 0;

					telnet_process(&sess->telnet,
					    (unsigned char *)
					    sess->conn.read_buf,
					    sess->conn.read_len,
					    out_buf, &out_len,
					    send_buf, &send_len);

					if (send_len > 0)
						conn_send(&sess->conn,
						    (char *)send_buf,
						    send_len);

					if (out_len > 0) {

						if (sess->terminal.
						    scroll_offset > 0) {
							short ri2;

							sess->terminal.
							    scroll_offset = 0;
							for (ri2 = 0;
							    ri2 < sess->
							    terminal.
							    active_rows;
							    ri2++)
								sess->
								    terminal.
								    dirty
								    [ri2] = 1;
						}

						terminal_process(
						    &sess->terminal,
						    out_buf, out_len);

						/* Send terminal responses */
						if (sess->terminal.
						    response_len > 0) {
							conn_send(&sess->conn,
							    sess->terminal.
							    response,
							    sess->terminal.
							    response_len);
							sess->terminal.
							    response_len = 0;
						}

						/* Update window title */
						if (sess->terminal.
						    title_changed) {
							if (sess->terminal.
							    window_title[0]) {
								char tmp[80];
								Str255 pt;
								short tl, ti;

								tl = sprintf(
								    tmp,
								    "Flynn"
								    " - %s",
								    sess->
								    terminal.
								    window_title
								    );
								if (tl > 254)
									tl =
									    254;
								pt[0] = tl;
								for (ti = 0;
								    ti < tl;
								    ti++)
									pt[ti
									    +1]
									    =
									    tmp
									    [ti
									    ];
								SetWTitle(
								    sess->
								    window,
								    pt);
							} else {
								SetWTitle(
								    sess->
								    window,
								    "\pFlynn"
								    );
							}
							sess->terminal.
							    title_changed = 0;
							update_window_menu();
						}

						{
							GrafPtr save;

							GetPort(&save);
							SetPort(
							    sess->window);
							term_ui_draw(
							    sess->window,
							    &sess->terminal);
							SetPort(save);
						}
					}

					sess->conn.read_len = 0;
				}

				/* Cursor blink only for front session */
				if (sess == active_session &&
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
			if (active_session)
				term_ui_load_state(&active_session->ui);

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
			/* MultiFinder suspend/resume - handle later */
			break;
		}
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

			if (s->terminal.scroll_offset > 0) {
				Str255 title;
				char tmp[32];
				short i, len;

				len = sprintf(tmp, "Flynn [-%d]",
				    s->terminal.scroll_offset);
				title[0] = len;
				for (i = 0; i < len; i++)
					title[i + 1] = tmp[i];
				SetWTitle(s->window, title);
			} else {
				SetWTitle(s->window, "\pFlynn");
			}

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

		/* Cmd+1..0 -> F1-F10 for M0110 keyboards */
		if (s->conn.state == CONN_STATE_CONNECTED &&
		    key >= '0' && key <= '9') {
			switch (key) {
			case '1': buffer_key_send(s, "\033OP", 3); return;
			case '2': buffer_key_send(s, "\033OQ", 3); return;
			case '3': buffer_key_send(s, "\033OR", 3); return;
			case '4': buffer_key_send(s, "\033OS", 3); return;
			case '5': buffer_key_send(s, "\033[15~", 5); return;
			case '6': buffer_key_send(s, "\033[17~", 5); return;
			case '7': buffer_key_send(s, "\033[18~", 5); return;
			case '8': buffer_key_send(s, "\033[19~", 5); return;
			case '9': buffer_key_send(s, "\033[20~", 5); return;
			case '0': buffer_key_send(s, "\033[21~", 5); return;
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
		switch (vkey) {
		case 0x52: buffer_key_send(s, "\033Op", 3); return;
		case 0x53: buffer_key_send(s, "\033Oq", 3); return;
		case 0x54: buffer_key_send(s, "\033Or", 3); return;
		case 0x55: buffer_key_send(s, "\033Os", 3); return;
		case 0x56: buffer_key_send(s, "\033Ot", 3); return;
		case 0x57: buffer_key_send(s, "\033Ou", 3); return;
		case 0x58: buffer_key_send(s, "\033Ov", 3); return;
		case 0x59: buffer_key_send(s, "\033Ow", 3); return;
		case 0x5B: buffer_key_send(s, "\033Ox", 3); return;
		case 0x5C: buffer_key_send(s, "\033Oy", 3); return;
		case 0x41: buffer_key_send(s, "\033On", 3); return;
		case 0x4C: buffer_key_send(s, "\033OM", 3); return;
		case 0x45: buffer_key_send(s, "\033Ok", 3); return;
		case 0x4E: buffer_key_send(s, "\033Om", 3); return;
		case 0x43: buffer_key_send(s, "\033Oj", 3); return;
		}
	}

	/* Map special keys to escape sequences */
	switch (vkey) {
	case 0x7E:	/* Up arrow */
		if (s->terminal.cursor_key_mode)
			buffer_key_send(s, "\033OA", 3);
		else
			buffer_key_send(s, "\033[A", 3);
		return;
	case 0x7D:	/* Down arrow */
		if (s->terminal.cursor_key_mode)
			buffer_key_send(s, "\033OB", 3);
		else
			buffer_key_send(s, "\033[B", 3);
		return;
	case 0x7C:	/* Right arrow */
		if (s->terminal.cursor_key_mode)
			buffer_key_send(s, "\033OC", 3);
		else
			buffer_key_send(s, "\033[C", 3);
		return;
	case 0x7B:	/* Left arrow */
		if (s->terminal.cursor_key_mode)
			buffer_key_send(s, "\033OD", 3);
		else
			buffer_key_send(s, "\033[D", 3);
		return;
	case 0x73:	/* Home */
		buffer_key_send(s, "\033[H", 3);
		return;
	case 0x77:	/* End */
		buffer_key_send(s, "\033[F", 3);
		return;
	case 0x74:	/* Page Up */
		buffer_key_send(s, "\033[5~", 4);
		return;
	case 0x79:	/* Page Down */
		buffer_key_send(s, "\033[6~", 4);
		return;
	case 0x75:	/* Forward Delete */
		buffer_key_send(s, "\033[3~", 4);
		return;
	case 0x33:	/* Delete/Backspace -> DEL */
		key = 0x7F;
		buffer_key_send(s, &key, 1);
		return;
	case 0x35:	/* Escape */
		key = 0x1B;
		buffer_key_send(s, &key, 1);
		return;
	case 0x24:	/* Return */
	case 0x4C:	/* Keypad Enter */
		key = 0x0D;
		buffer_key_send(s, &key, 1);
		return;
	case 0x30:	/* Tab */
		key = 0x09;
		buffer_key_send(s, &key, 1);
		return;
	case 0x47:	/* Clear/NumLock -> Escape (M0110A keypad) */
		key = 0x1B;
		buffer_key_send(s, &key, 1);
		return;
	/* Function keys F1-F12 (ADB extended keyboards) */
	case 0x7A:	/* F1 */
		buffer_key_send(s, "\033OP", 3);
		return;
	case 0x78:	/* F2 */
		buffer_key_send(s, "\033OQ", 3);
		return;
	case 0x63:	/* F3 */
		buffer_key_send(s, "\033OR", 3);
		return;
	case 0x76:	/* F4 */
		buffer_key_send(s, "\033OS", 3);
		return;
	case 0x60:	/* F5 */
		buffer_key_send(s, "\033[15~", 5);
		return;
	case 0x61:	/* F6 */
		buffer_key_send(s, "\033[17~", 5);
		return;
	case 0x62:	/* F7 */
		buffer_key_send(s, "\033[18~", 5);
		return;
	case 0x64:	/* F8 */
		buffer_key_send(s, "\033[19~", 5);
		return;
	case 0x65:	/* F9 */
		buffer_key_send(s, "\033[20~", 5);
		return;
	case 0x6D:	/* F10 */
		buffer_key_send(s, "\033[21~", 5);
		return;
	case 0x67:	/* F11 */
		buffer_key_send(s, "\033[23~", 5);
		return;
	case 0x6F:	/* F12 */
		buffer_key_send(s, "\033[24~", 5);
		return;
	}

	/* Arrow keys by charCode — catches M0110A and keyboards where
	 * vkey codes differ from ADB 0x7B-0x7E */
	if (key >= 0x1C && key <= 0x1F) {
		const char *seq;

		if (s->terminal.cursor_key_mode) {
			switch (key) {
			case 0x1C: seq = "\033OD"; break; /* Left */
			case 0x1D: seq = "\033OC"; break; /* Right */
			case 0x1E: seq = "\033OA"; break; /* Up */
			case 0x1F: seq = "\033OB"; break; /* Down */
			}
		} else {
			switch (key) {
			case 0x1C: seq = "\033[D"; break;
			case 0x1D: seq = "\033[C"; break;
			case 0x1E: seq = "\033[A"; break;
			case 0x1F: seq = "\033[B"; break;
			}
		}
		buffer_key_send(s, (char *)seq, 3);
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
		static const char vkey_to_base[48] = {
			'a','s','d','f','h','g','z','x',   /* 0x00 */
			'c','v',  0,'b','q','w','e','r',   /* 0x08 */
			'y','t',  0,  0,  0,  0,  0,  0,   /* 0x10 */
			  0,  0,  0,  0,  0,  0,']','o',   /* 0x18 */
			'u','[','i','p',  0,'l','j',  0,   /* 0x20 */
			'k',  0,'\\', 0,  0,'n','m',  0,   /* 0x28 */
		};

		if (vkey < 48 && vkey_to_base[vkey]) {
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
				if (session_count() == 0)
					running = false;
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

	if (prefs.dark_mode)
		PaintRect(&win->portRect);
	else
		EraseRect(&win->portRect);

	if (sess) {
		term_ui_load_state(&sess->ui);

		if (sess->conn.state == CONN_STATE_CONNECTED) {
			char title[270];
			Str255 ptitle;
			short i, len;

			if (sess->terminal.window_title[0])
				len = sprintf(title, "Flynn - %s",
				    sess->terminal.window_title);
			else
				len = sprintf(title, "Flynn - %s",
				    sess->conn.host);
			if (len > 254) len = 254;
			ptitle[0] = len;
			for (i = 0; i < len; i++)
				ptitle[i + 1] = title[i];
			SetWTitle(sess->window, ptitle);

			/* Mark all rows dirty for full repaints */
			for (i = 0; i < sess->terminal.active_rows; i++)
				sess->terminal.dirty[i] = 1;
			term_ui_draw(sess->window, &sess->terminal);
		} else {
			SetWTitle(sess->window, "\pFlynn");
			if (prefs.dark_mode) {
				short i;

				for (i = 0;
				    i < sess->terminal.active_rows; i++)
					sess->terminal.dirty[i] = 1;
				term_ui_draw(sess->window,
				    &sess->terminal);
			}
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
			/* Load incoming session's UI state */
			term_ui_load_state(&sess->ui);
			update_menus();
		}
	}
}

static Boolean
handle_menu(long menu_id)
{
	short menu, item;
	Boolean handled = true;

	menu = HiWord(menu_id);
	item = LoWord(menu_id);

	switch (menu) {
	case APPLE_MENU_ID:
		if (item == APPLE_MENU_ABOUT_ID) {
			do_about();
		} else {
			Str255 da_name;
			GrafPtr save_port;

			GetMenuItemText(apple_menu, item, da_name);
			GetPort(&save_port);
			OpenDeskAcc(da_name);
			SetPort(save_port);
		}
		break;
	case FILE_MENU_ID:
		if (item >= FILE_MENU_BM_BASE &&
		    item < FILE_MENU_BM_BASE + prefs.bookmark_count) {
			do_connect_bookmark(item - FILE_MENU_BM_BASE);
			break;
		}
		switch (item) {
		case FILE_MENU_CONNECT_ID:
			do_connect();
			break;
		case FILE_MENU_DISCONNECT_ID:
			do_disconnect();
			break;
		case FILE_MENU_BOOKMARKS_ID:
			do_bookmarks();
			break;
		case FILE_MENU_QUIT_ID:
			if (session_any_connected()) {
				ParamText(
				    "\pDisconnect all sessions "
				    "and quit?",
				    "\p", "\p", "\p");
				if (CautionAlert(128, 0L) != 1)
					break;
			}
			/* Destroy all sessions */
			{
				short si;
				Session *sess;

				for (si = MAX_SESSIONS - 1; si >= 0;
				    si--) {
					sess = session_get(si);
					if (sess)
						session_destroy(sess);
				}
			}
			active_session = 0L;
			running = false;
			break;
		}
		break;
	case EDIT_MENU_ID:
		if (!SystemEdit(item - 1)) {
			switch (item) {
			case EDIT_MENU_COPY_ID:
				do_copy();
				break;
			case EDIT_MENU_PASTE_ID:
				do_paste();
				break;
			case EDIT_MENU_SELALL_ID:
				do_select_all();
				break;
			}
		}
		break;
	case CTRL_MENU_ID:
		if (active_session &&
		    active_session->conn.state == CONN_STATE_CONNECTED) {
			char ctrl_byte;

			switch (item) {
			case CTRL_MENU_CTRLC:
				ctrl_byte = 0x03;
				conn_send(&active_session->conn,
				    &ctrl_byte, 1);
				break;
			case CTRL_MENU_CTRLD:
				ctrl_byte = 0x04;
				conn_send(&active_session->conn,
				    &ctrl_byte, 1);
				break;
			case CTRL_MENU_CTRLZ:
				ctrl_byte = 0x1A;
				conn_send(&active_session->conn,
				    &ctrl_byte, 1);
				break;
			case CTRL_MENU_ESC:
				ctrl_byte = 0x1B;
				conn_send(&active_session->conn,
				    &ctrl_byte, 1);
				break;
			case CTRL_MENU_CTRLL:
				ctrl_byte = 0x0C;
				conn_send(&active_session->conn,
				    &ctrl_byte, 1);
				break;
			case CTRL_MENU_BREAK: {
				char brk_seq[2];

				brk_seq[0] = (char)0xFF;  /* IAC */
				brk_seq[1] = (char)0xF3;  /* BRK */
				conn_send(&active_session->conn,
				    brk_seq, 2);
				break;
			}
			}
		}
		break;
	case PREFS_MENU_ID:
		switch (item) {
		case PREFS_FONT9_ID:
			do_font_change(4, 9);
			break;
		case PREFS_FONT12_ID:
			do_font_change(4, 12);
			break;
		case PREFS_FONT_C10:
			do_font_change(22, 10);
			break;
		case PREFS_FONT_CH12:
			do_font_change(0, 12);
			break;
		case PREFS_FONT_G9:
			do_font_change(3, 9);
			break;
		case PREFS_FONT_G10:
			do_font_change(3, 10);
			break;
		case PREFS_XTERM_ID:
		case PREFS_VT220_ID:
		case PREFS_VT100_ID:
			prefs.terminal_type = item - PREFS_XTERM_ID;
			if (active_session)
				active_session->telnet.preferred_ttype =
				    prefs.terminal_type;
			prefs_save(&prefs);
			update_prefs_menu();
			if (active_session &&
			    active_session->conn.state ==
			    CONN_STATE_CONNECTED) {
				ParamText(
				    "\pTerminal type change takes "
				    "effect on next connection.",
				    "\p", "\p", "\p");
				Alert(128, 0L);
			}
			break;
		case PREFS_DARK_ID:
			prefs.dark_mode = !prefs.dark_mode;
			term_ui_set_dark_mode(prefs.dark_mode);
			prefs_save(&prefs);
			update_prefs_menu();
			{
				short si;
				Session *sess;
				GrafPtr save;

				GetPort(&save);
				for (si = 0; si < MAX_SESSIONS; si++) {
					sess = session_get(si);
					if (!sess)
						continue;
					SetPort(sess->window);
					if (prefs.dark_mode)
						PaintRect(
						    &sess->window->
						    portRect);
					else
						EraseRect(
						    &sess->window->
						    portRect);
					{
						short ri;

						for (ri = 0;
						    ri < sess->terminal.
						    active_rows; ri++)
							sess->terminal.
							    dirty[ri] = 1;
					}
					InvalRect(
					    &sess->window->portRect);
				}
				SetPort(save);
			}
			break;
		case PREFS_DNS_ID:
			do_dns_server_dialog();
			break;
		}
		break;
	case WINDOW_MENU_ID:
		switch (item) {
		case WIN_MENU_CLOSE:
		{
			if (active_session) {
				if (active_session->conn.state ==
				    CONN_STATE_CONNECTED) {
					ParamText(
					    "\pDisconnect and close "
					    "window?",
					    "\p", "\p", "\p");
					if (CautionAlert(128, 0L) != 1)
						break;
				}
				term_ui_load_state(&active_session->ui);
				session_destroy(active_session);
				active_session =
				    session_from_window(
				    FrontWindow());
				if (session_count() == 0)
					running = false;
			}
			break;
		}
		default:
		{
			/* Window list item */
			short win_idx = item - WIN_MENU_FIRST_WIN;
			short count = 0, si;

			for (si = 0; si < MAX_SESSIONS; si++) {
				Session *ws = session_get(si);
				if (!ws)
					continue;
				if (count == win_idx) {
					SelectWindow(ws->window);
					if (active_session)
						term_ui_save_state(
						    &active_session->ui);
					active_session = ws;
					term_ui_load_state(&ws->ui);
					break;
				}
				count++;
			}
			break;
		}
		}
		update_menus();
		break;
	default:
		handled = false;
		break;
	}

	HiliteMenu(0);
	return handled;
}

static void
do_about(void)
{
	DialogPtr dlg;
	short item;

	dlg = GetNewDialog(DLOG_ABOUT_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return;

	ModalDialog(0L, &item);
	DisposeDialog(dlg);
}

static void
do_connect(void)
{
	Session *s = active_session;

	/* Create session if none exists */
	if (!s) {
		s = session_new();
		if (!s) {
			ParamText("\pOut of memory", "\p", "\p", "\p");
			Alert(128, 0L);
			return;
		}
		SetPort(s->window);
		term_ui_set_font(s->window, prefs.font_id, prefs.font_size);
		term_ui_init(s->window, &s->terminal);
		s->conn.dns_server = ip2long(prefs.dns_server);
		if (prefs.dark_mode)
			PaintRect(&s->window->portRect);
		active_session = s;
	}

	/* If active session is already connected, create a new one */
	if (s->conn.state == CONN_STATE_CONNECTED) {
		s = session_new();
		if (!s) {
			ParamText("\pMaximum sessions reached",
			    "\p", "\p", "\p");
			Alert(128, 0L);
			update_menus();
			return;
		}
		SetPort(s->window);
		term_ui_set_font(s->window, prefs.font_id, prefs.font_size);
		term_ui_init(s->window, &s->terminal);
		s->conn.dns_server = ip2long(prefs.dns_server);
		if (prefs.dark_mode)
			PaintRect(&s->window->portRect);
		SelectWindow(s->window);
		active_session = s;
	}

	/* Pre-fill from saved prefs */
	if (!s->conn.host[0] && prefs.host[0]) {
		strncpy(s->conn.host, prefs.host,
		    sizeof(s->conn.host) - 1);
		s->conn.host[sizeof(s->conn.host) - 1] = '\0';
		s->conn.port = prefs.port;
	}
	if (!s->conn.username[0] && prefs.username[0]) {
		strncpy(s->conn.username, prefs.username,
		    sizeof(s->conn.username) - 1);
		s->conn.username[sizeof(s->conn.username) - 1] = '\0';
	}

	s->telnet.preferred_ttype = prefs.terminal_type;

	if (conn_open_dialog(&s->conn)) {
		telnet_init(&s->telnet);
		s->telnet.preferred_ttype = prefs.terminal_type;
		s->telnet.cols = s->terminal.active_cols;
		s->telnet.rows = s->terminal.active_rows;
		terminal_reset(&s->terminal);

		/* Save last-used host/port/username */
		strncpy(prefs.host, s->conn.host,
		    sizeof(prefs.host) - 1);
		prefs.host[sizeof(prefs.host) - 1] = '\0';
		prefs.port = s->conn.port;
		strncpy(prefs.username, s->conn.username,
		    sizeof(prefs.username) - 1);
		prefs.username[sizeof(prefs.username) - 1] = '\0';
		prefs_save(&prefs);

		/* Auto-send username after connect */
		if (s->conn.username[0]) {
			conn_send(&s->conn, s->conn.username,
			    strlen(s->conn.username));
			conn_send(&s->conn, "\r", 1);
		}

		/* Update window title */
		{
			char tmp[270];
			Str255 title;
			short tlen, ti;

			tlen = sprintf(tmp, "Flynn - %s", s->conn.host);
			if (tlen > 254) tlen = 254;
			title[0] = tlen;
			for (ti = 0; ti < tlen; ti++)
				title[ti + 1] = tmp[ti];
			SetWTitle(s->window, title);
		}
	} else if (s->conn.state == CONN_STATE_IDLE &&
	    session_count() > 1) {
		/* User cancelled on a fresh session — if it was just
		 * created (never connected, no host set), destroy it */
		if (!s->conn.host[0]) {
			if (s == active_session)
				active_session = 0L;
			session_destroy(s);
			if (!active_session) {
				WindowPtr front = FrontWindow();
				active_session =
				    session_from_window(front);
			}
		}
	}
	update_menus();
}

static void
do_connect_bookmark(short index)
{
	Bookmark *bm;
	Session *s = active_session;

	if (index < 0 || index >= prefs.bookmark_count)
		return;

	/* Create session if none exists */
	if (!s) {
		s = session_new();
		if (!s) {
			ParamText("\pOut of memory", "\p", "\p", "\p");
			Alert(128, 0L);
			return;
		}
		SetPort(s->window);
		term_ui_set_font(s->window, prefs.font_id, prefs.font_size);
		term_ui_init(s->window, &s->terminal);
		s->conn.dns_server = ip2long(prefs.dns_server);
		if (prefs.dark_mode)
			PaintRect(&s->window->portRect);
		active_session = s;
	}

	/* If active session is already connected, create a new one */
	if (s->conn.state == CONN_STATE_CONNECTED) {
		s = session_new();
		if (!s) {
			ParamText("\pMaximum sessions reached",
			    "\p", "\p", "\p");
			Alert(128, 0L);
			update_menus();
			return;
		}
		SetPort(s->window);
		term_ui_set_font(s->window, prefs.font_id, prefs.font_size);
		term_ui_init(s->window, &s->terminal);
		s->conn.dns_server = ip2long(prefs.dns_server);
		if (prefs.dark_mode)
			PaintRect(&s->window->portRect);
		SelectWindow(s->window);
		active_session = s;
	}

	if (s->conn.state != CONN_STATE_IDLE) {
		update_menus();
		return;
	}

	bm = &prefs.bookmarks[index];
	if (conn_connect(&s->conn, bm->host, bm->port)) {
		telnet_init(&s->telnet);
		s->telnet.preferred_ttype = prefs.terminal_type;
		s->telnet.cols = s->terminal.active_cols;
		s->telnet.rows = s->terminal.active_rows;
		terminal_reset(&s->terminal);

		strncpy(prefs.host, s->conn.host,
		    sizeof(prefs.host) - 1);
		prefs.host[sizeof(prefs.host) - 1] = '\0';
		prefs.port = s->conn.port;
		prefs_save(&prefs);

		/* Update window title */
		{
			char tmp[270];
			Str255 title;
			short tlen, ti;

			tlen = sprintf(tmp, "Flynn - %s", s->conn.host);
			if (tlen > 254) tlen = 254;
			title[0] = tlen;
			for (ti = 0; ti < tlen; ti++)
				title[ti + 1] = tmp[ti];
			SetWTitle(s->window, title);
		}
	}
	update_menus();
}

/* Bookmark manager state */
static short bm_selection = -1;
static FlynnPrefs *bm_prefs_ptr;
static Rect bm_list_rect;

static pascal void
bm_list_draw(WindowPtr win, short item)
{
	short i, y;
	Rect r;
	char line[180];
	short len, tmpType;
	Handle tmpH;
	FlynnPrefs *p = bm_prefs_ptr;

	GetDialogItem((DialogPtr)win, item, &tmpType, &tmpH, &r);
	bm_list_rect = r;

	EraseRect(&r);
	FrameRect(&r);
	InsetRect(&r, 1, 1);

	for (i = 0; i < p->bookmark_count; i++) {
		y = r.top + 2 + i * 16;
		if (y + 14 > r.bottom)
			break;

		MoveTo(r.left + 4, y + 12);
		if (p->bookmarks[i].port != 23)
			len = sprintf(line, "%s - %s:%u",
			    p->bookmarks[i].name,
			    p->bookmarks[i].host,
			    p->bookmarks[i].port);
		else
			len = sprintf(line, "%s - %s",
			    p->bookmarks[i].name,
			    p->bookmarks[i].host);
		DrawText(line, 0, len);

		if (i == bm_selection) {
			Rect sel_r;
			SetRect(&sel_r, r.left, y - 1,
			    r.right, y + 15);
			InvertRect(&sel_r);
		}
	}
}

static Boolean
bm_edit_dialog(Bookmark *bm, Boolean is_new)
{
	DialogPtr dlg;
	short item_hit;
	Handle item_h;
	short item_type;
	Rect item_rect;
	Str255 str;
	long num;
	short i;

	dlg = GetNewDialog(DLOG_BM_EDIT_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return false;

	/* Pre-fill fields */
	if (!is_new) {
		GetDialogItem(dlg, BME_NAME_FIELD, &item_type,
		    &item_h, &item_rect);
		str[0] = strlen(bm->name);
		memcpy(&str[1], bm->name, str[0]);
		SetDialogItemText(item_h, str);

		GetDialogItem(dlg, BME_HOST_FIELD, &item_type,
		    &item_h, &item_rect);
		str[0] = strlen(bm->host);
		memcpy(&str[1], bm->host, str[0]);
		SetDialogItemText(item_h, str);

		GetDialogItem(dlg, BME_PORT_FIELD, &item_type,
		    &item_h, &item_rect);
		sprintf((char *)&str[1], "%u", bm->port);
		str[0] = strlen((char *)&str[1]);
		SetDialogItemText(item_h, str);
	}

	ShowWindow(dlg);

	for (;;) {
		ModalDialog(0L, &item_hit);
		if (item_hit == BME_CANCEL) {
			DisposeDialog(dlg);
			return false;
		}
		if (item_hit == BME_OK)
			break;
	}

	/* Extract name */
	GetDialogItem(dlg, BME_NAME_FIELD, &item_type,
	    &item_h, &item_rect);
	GetDialogItemText(item_h, str);
	if (str[0] == 0) {
		DisposeDialog(dlg);
		return false;
	}
	if (str[0] > 31) str[0] = 31;
	for (i = 0; i < str[0]; i++)
		bm->name[i] = str[i + 1];
	bm->name[i] = '\0';

	/* Extract host */
	GetDialogItem(dlg, BME_HOST_FIELD, &item_type,
	    &item_h, &item_rect);
	GetDialogItemText(item_h, str);
	if (str[0] == 0) {
		DisposeDialog(dlg);
		return false;
	}
	if (str[0] > 127) str[0] = 127;
	for (i = 0; i < str[0]; i++)
		bm->host[i] = str[i + 1];
	bm->host[i] = '\0';

	/* Extract port */
	GetDialogItem(dlg, BME_PORT_FIELD, &item_type,
	    &item_h, &item_rect);
	GetDialogItemText(item_h, str);
	if (str[0] > 0) {
		str[str[0] + 1] = '\0';
		StringToNum(str, &num);
		bm->port = (unsigned short)num;
	} else {
		bm->port = 23;
	}

	DisposeDialog(dlg);
	return true;
}

static pascal Boolean
bm_filter(DialogPtr dlg, EventRecord *event, short *item)
{
	Point pt;
	short i;

	if (event->what == mouseDown) {
		pt = event->where;
		GlobalToLocal(&pt);
		if (PtInRect(pt, &bm_list_rect)) {
			InsetRect(&bm_list_rect, 1, 1);
			i = (pt.v - bm_list_rect.top - 2) / 16;
			InsetRect(&bm_list_rect, -1, -1);
			if (i >= 0 && i < bm_prefs_ptr->bookmark_count)
				bm_selection = i;
			else
				bm_selection = -1;
			/* Redraw list */
			bm_list_draw((WindowPtr)dlg, BM_LIST);
			*item = BM_LIST;
			return true;
		}
	}
	return false;
}

static void
do_bookmarks(void)
{
	DialogPtr dlg;
	short item_hit;
	Handle item_h;
	short item_type;
	Rect item_rect;
	Boolean changed = false;

	dlg = GetNewDialog(DLOG_BOOKMARKS_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return;

	bm_prefs_ptr = &prefs;
	bm_selection = -1;

	/* Set up UserItem draw proc for list */
	GetDialogItem(dlg, BM_LIST, &item_type, &item_h, &item_rect);
	bm_list_rect = item_rect;
	SetDialogItem(dlg, BM_LIST, item_type,
	    (Handle)bm_list_draw, &item_rect);

	ShowWindow(dlg);

	for (;;) {
		ModalDialog((ModalFilterProcPtr)bm_filter,
		    &item_hit);

		if (item_hit == BM_DONE)
			break;

		if (item_hit == BM_ADD) {
			if (prefs.bookmark_count >= MAX_BOOKMARKS) {
				SysBeep(10);
				continue;
			}
			memset(&prefs.bookmarks[prefs.bookmark_count],
			    0, sizeof(Bookmark));
			prefs.bookmarks[prefs.bookmark_count].port = 23;
			if (bm_edit_dialog(
			    &prefs.bookmarks[prefs.bookmark_count],
			    true)) {
				prefs.bookmark_count++;
				bm_selection = prefs.bookmark_count - 1;
				changed = true;
			}
			/* Redraw list */
			SetPort(dlg);
			bm_list_draw((WindowPtr)dlg, BM_LIST);
		}

		if (item_hit == BM_EDIT) {
			if (bm_selection < 0 ||
			    bm_selection >= prefs.bookmark_count) {
				SysBeep(10);
				continue;
			}
			if (bm_edit_dialog(
			    &prefs.bookmarks[bm_selection], false))
				changed = true;
			SetPort(dlg);
			bm_list_draw((WindowPtr)dlg, BM_LIST);
		}

		if (item_hit == BM_DELETE) {
			short j;

			if (bm_selection < 0 ||
			    bm_selection >= prefs.bookmark_count) {
				SysBeep(10);
				continue;
			}
			for (j = bm_selection;
			    j < prefs.bookmark_count - 1; j++)
				prefs.bookmarks[j] =
				    prefs.bookmarks[j + 1];
			prefs.bookmark_count--;
			if (bm_selection >= prefs.bookmark_count)
				bm_selection = prefs.bookmark_count - 1;
			changed = true;
			SetPort(dlg);
			bm_list_draw((WindowPtr)dlg, BM_LIST);
		}

		if (item_hit == BM_CONNECT) {
			if (bm_selection < 0 ||
			    bm_selection >= prefs.bookmark_count) {
				SysBeep(10);
				continue;
			}
			DisposeDialog(dlg);
			if (changed) {
				prefs_save(&prefs);
				rebuild_bookmark_menu();
			}
			do_connect_bookmark(bm_selection);
			return;
		}
	}

	DisposeDialog(dlg);
	if (changed) {
		prefs_save(&prefs);
		rebuild_bookmark_menu();
	}
}

static void
do_disconnect(void)
{
	Session *s = active_session;
	short i;

	if (!s || s->conn.state != CONN_STATE_CONNECTED)
		return;

	conn_close(&s->conn);
	terminal_reset(&s->terminal);
	telnet_init(&s->telnet);
	s->telnet.preferred_ttype = prefs.terminal_type;
	s->key_send_len = 0;
	SetWTitle(s->window, "\pFlynn");

	{
		GrafPtr save;

		GetPort(&save);
		SetPort(s->window);
		if (prefs.dark_mode)
			PaintRect(&s->window->portRect);
		else
			EraseRect(&s->window->portRect);
		for (i = 0; i < s->terminal.active_rows; i++)
			s->terminal.dirty[i] = 1;
		term_ui_draw(s->window, &s->terminal);
		SetPort(save);
	}

	update_menus();
}

static void
do_font_change(short font_id, short font_size)
{
	short si;
	Session *sess;
	short win_w, win_h;

	if (font_id == prefs.font_id && font_size == prefs.font_size)
		return;

	/* Save preference first — term_ui_set_font uses global metrics */
	prefs.font_id = font_id;
	prefs.font_size = font_size;
	prefs_save(&prefs);

	/* Apply to all sessions */
	for (si = 0; si < MAX_SESSIONS; si++) {
		sess = session_get(si);
		if (!sess)
			continue;

		term_ui_set_font(sess->window, font_id, font_size);

		/* Compute window size for default grid */
		win_w = LEFT_MARGIN * 2 +
		    TERM_DEFAULT_COLS * g_cell_width;
		win_h = TOP_MARGIN * 2 +
		    TERM_DEFAULT_ROWS * g_cell_height;
		if (win_w > MAX_WIN_WIDTH)
			win_w = MAX_WIN_WIDTH;
		if (win_h > MAX_WIN_HEIGHT)
			win_h = MAX_WIN_HEIGHT;

		do_window_resize(sess, win_w, win_h);
	}

	update_prefs_menu();
}

static void
do_window_resize(Session *s, short width, short height)
{
	short new_cols, new_rows;
	GrafPtr save;
	short i;

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
	if (prefs.dark_mode)
		PaintRect(&s->window->portRect);
	else
		EraseRect(&s->window->portRect);
	for (i = 0; i < new_rows; i++)
		s->terminal.dirty[i] = 1;
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

static void
update_prefs_menu(void)
{
	if (!prefs_menu)
		return;
	CheckItem(prefs_menu, PREFS_FONT9_ID,
	    prefs.font_id == 4 && prefs.font_size == 9);
	CheckItem(prefs_menu, PREFS_FONT12_ID,
	    prefs.font_id == 4 && prefs.font_size == 12);
	CheckItem(prefs_menu, PREFS_FONT_C10,
	    prefs.font_id == 22 && prefs.font_size == 10);
	CheckItem(prefs_menu, PREFS_FONT_CH12,
	    prefs.font_id == 0 && prefs.font_size == 12);
	CheckItem(prefs_menu, PREFS_FONT_G9,
	    prefs.font_id == 3 && prefs.font_size == 9);
	CheckItem(prefs_menu, PREFS_FONT_G10,
	    prefs.font_id == 3 && prefs.font_size == 10);
	CheckItem(prefs_menu, PREFS_XTERM_ID,
	    prefs.terminal_type == 0);
	CheckItem(prefs_menu, PREFS_VT220_ID,
	    prefs.terminal_type == 1);
	CheckItem(prefs_menu, PREFS_VT100_ID,
	    prefs.terminal_type == 2);
	CheckItem(prefs_menu, PREFS_DARK_ID,
	    prefs.dark_mode != 0);
}

static void
do_dns_server_dialog(void)
{
	DialogPtr dlg;
	short item_hit;
	Handle item_h;
	short item_type;
	Rect item_rect;
	Str255 ip_str;
	char ip_cstr[16];
	short i, len;
	unsigned long ip;

	dlg = GetNewDialog(DLOG_DNS_ID, 0L, (WindowPtr)-1L);
	if (!dlg) {
		SysBeep(10);
		return;
	}

	/* Pre-fill with current DNS server */
	GetDialogItem(dlg, 4, &item_type, &item_h, &item_rect);
	len = strlen(prefs.dns_server);
	ip_str[0] = len;
	for (i = 0; i < len; i++)
		ip_str[i + 1] = prefs.dns_server[i];
	SetDialogItemText(item_h, ip_str);

	ShowWindow(dlg);

	for (;;) {
		ModalDialog(0L, &item_hit);

		if (item_hit == 2) {  /* Cancel */
			DisposeDialog(dlg);
			return;
		}
		if (item_hit == 1)  /* OK */
			break;
	}

	/* Extract and validate IP */
	GetDialogItem(dlg, 4, &item_type, &item_h, &item_rect);
	GetDialogItemText(item_h, ip_str);
	DisposeDialog(dlg);

	len = ip_str[0];
	if (len == 0 || len > 15)
		return;
	for (i = 0; i < len; i++)
		ip_cstr[i] = ip_str[i + 1];
	ip_cstr[len] = '\0';

	ip = ip2long(ip_cstr);
	if (ip == 0) {
		ParamText("\pInvalid DNS server IP address",
		    "\p", "\p", "\p");
		Alert(128, 0L);
		return;
	}

	strncpy(prefs.dns_server, ip_cstr, sizeof(prefs.dns_server) - 1);
	prefs.dns_server[sizeof(prefs.dns_server) - 1] = '\0';
	prefs_save(&prefs);

	/* Update DNS server for all sessions */
	{
		short si;
		Session *sess;

		for (si = 0; si < MAX_SESSIONS; si++) {
			sess = session_get(si);
			if (sess)
				sess->conn.dns_server = ip;
		}
	}
}

static void
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
				if ((cell->attr & ATTR_GLYPH) &&
				    cell->ch == GLYPH_WIDE_SPACER) {
					buf[len + (col - c_start)] = ' ';
					continue;
				}
				if (cell->attr & ATTR_GLYPH) {
					gi = glyph_get_info(cell->ch);
					cc = gi ? gi->copy_char : '?';
				} else if (cell->attr & ATTR_BRAILLE) {
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

static void
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

static void
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

	update_menus();
}

static short
pixel_to_row(Session *s, short v)
{
	short r;

	r = (v - TOP_MARGIN) / g_cell_height;
	if (r < 0) r = 0;
	if (r >= s->terminal.active_rows)
		r = s->terminal.active_rows - 1;
	return r;
}

static short
pixel_to_col(Session *s, short h)
{
	short c;

	c = (h - LEFT_MARGIN) / g_cell_width;
	if (c < 0) c = 0;
	if (c >= s->terminal.active_cols)
		c = s->terminal.active_cols - 1;
	return c;
}

static void
handle_content_click(Session *s, EventRecord *event)
{
	Point local_pt;
	short row, col;
	GrafPtr save;
	short i;

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

	/* Redraw all rows to show final selection state */
	for (i = 0; i < s->terminal.active_rows; i++)
		s->terminal.dirty[i] = 1;
	term_ui_draw(s->window, &s->terminal);

	SetPort(save);
}

static void
track_selection_drag(Session *s)
{
	Point pt;
	short row, col, prev_row, prev_col;
	short old_extent_row;

	prev_row = -1;
	prev_col = -1;

	while (StillDown()) {
		GetMouse(&pt);
		row = pixel_to_row(s, pt.v);
		col = pixel_to_col(s, pt.h);

		if (row == prev_row && col == prev_col)
			continue;

		old_extent_row = row;  /* will be used after extend */
		term_ui_sel_dirty_rows(&s->terminal, prev_row < 0 ?
		    row : prev_row, row);
		term_ui_sel_extend(row, col, &s->terminal);
		term_ui_draw(s->window, &s->terminal);

		prev_row = row;
		prev_col = col;
	}

	term_ui_sel_finalize();
}
