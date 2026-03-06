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
#include "connection.h"
#include "telnet.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "settings.h"

/* Globals */
static MenuHandle apple_menu, file_menu, edit_menu;
static WindowPtr term_window;
static Boolean running = true;
static Connection conn;
static TelnetState telnet;
static Terminal terminal;
static FlynnPrefs prefs;

/* Forward declarations */
static void init_toolbox(void);
static void init_menus(void);
static void init_window(void);
static void update_menus(void);
static void main_event_loop(void);
static Boolean handle_menu(long menu_id);
static void handle_mouse_down(EventRecord *event);
static void handle_key_down(EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void do_about(void);
static void do_connect(void);
static void do_disconnect(void);
static void do_copy(void);
static void do_paste(void);

int
main(void)
{
	init_toolbox();
	init_menus();
	init_window();

	memset(&conn, 0, sizeof(conn));
	conn_init();
	telnet_init(&telnet);
	terminal_init(&terminal);
	prefs_load(&prefs);
	update_menus();

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

	apple_menu = GetMenuHandle(APPLE_MENU_ID);
	if (apple_menu)
		AppendResMenu(apple_menu, 'DRVR');

	file_menu = GetMenuHandle(FILE_MENU_ID);
	edit_menu = GetMenuHandle(EDIT_MENU_ID);

	DrawMenuBar();
}

static void
init_window(void)
{
	Rect bounds;

	SetRect(&bounds, 10, 40, 10 + TERM_WIN_WIDTH, 40 + TERM_WIN_HEIGHT);
	term_window = NewWindow(0L, &bounds, "\pFlynn", true,
	    documentProc, (WindowPtr)-1L, true, 0L);

	if (term_window) {
		SetPort(term_window);
		term_ui_init(term_window, &terminal);
	}
}

static void
update_menus(void)
{
	Boolean connected;

	connected = (conn.state == CONN_STATE_CONNECTED);

	/* File menu: Connect vs Disconnect */
	if (connected) {
		DisableItem(file_menu, FILE_MENU_CONNECT_ID);
		EnableItem(file_menu, FILE_MENU_DISCONNECT_ID);
	} else {
		EnableItem(file_menu, FILE_MENU_CONNECT_ID);
		DisableItem(file_menu, FILE_MENU_DISCONNECT_ID);
	}

	/* Edit menu: Copy/Paste only when connected */
	if (connected) {
		EnableItem(edit_menu, EDIT_MENU_COPY_ID);
		EnableItem(edit_menu, EDIT_MENU_PASTE_ID);
	} else {
		DisableItem(edit_menu, EDIT_MENU_COPY_ID);
		DisableItem(edit_menu, EDIT_MENU_PASTE_ID);
	}
}

static void
main_event_loop(void)
{
	EventRecord event;
	long wait_ticks;
	short prev_state;

	while (running) {
		wait_ticks = 5L;
		WaitNextEvent(everyEvent, &event, wait_ticks, 0L);

		switch (event.what) {
		case nullEvent:
			prev_state = conn.state;
			conn_idle(&conn);

			/* Detect connection lost */
			if (prev_state == CONN_STATE_CONNECTED &&
			    conn.state == CONN_STATE_IDLE) {
				SetWTitle(term_window, "\pFlynn");
				update_menus();
				ParamText("\pConnection closed by remote host",
				    "\p", "\p", "\p");
				Alert(128, 0L);
			}

			if (conn.read_len > 0) {
				unsigned char out_buf[TCP_READ_BUFSIZ];
				unsigned char send_buf[256];
				short out_len = 0, send_len = 0;

				telnet_process(&telnet,
				    (unsigned char *)conn.read_buf,
				    conn.read_len, out_buf, &out_len,
				    send_buf, &send_len);

				if (send_len > 0)
					conn_send(&conn, (char *)send_buf,
					    send_len);

				if (out_len > 0) {
					GrafPtr save;

					terminal.scroll_offset = 0;
					terminal_process(&terminal, out_buf,
					    out_len);

					/* Send any terminal responses (DA, DSR) */
					if (terminal.response_len > 0) {
						conn_send(&conn,
						    terminal.response,
						    terminal.response_len);
						terminal.response_len = 0;
					}

					GetPort(&save);
					SetPort(term_window);
					term_ui_draw(term_window,
					    &terminal);
					SetPort(save);
				}

				conn.read_len = 0;
			}
			if (conn.state == CONN_STATE_CONNECTED)
				term_ui_cursor_blink(term_window,
				    &terminal);
			break;
		case keyDown:
		case autoKey:
			handle_key_down(&event);
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

	ExitToShell();
}

static void
handle_key_down(EventRecord *event)
{
	char key;
	short vkey;
	char esc_seq[8];

	key = event->message & charCodeMask;
	vkey = (event->message >> 8) & 0xFF;

	if (event->modifiers & cmdKey) {
		/* Cmd+Up/Down for scrollback navigation */
		if (vkey == 0x7E || vkey == 0x7D) {
			GrafPtr save;

			if (vkey == 0x7E) {
				if (event->modifiers & shiftKey)
					terminal_scroll_back(&terminal,
					    TERM_ROWS);
				else
					terminal_scroll_back(&terminal, 1);
			} else {
				if (event->modifiers & shiftKey)
					terminal_scroll_forward(&terminal,
					    TERM_ROWS);
				else
					terminal_scroll_forward(&terminal, 1);
			}

			GetPort(&save);
			SetPort(term_window);
			term_ui_draw(term_window, &terminal);

			if (terminal.scroll_offset > 0) {
				Str255 title;
				char tmp[32];
				short i, len;

				len = sprintf(tmp, "Flynn [-%d]",
				    terminal.scroll_offset);
				title[0] = len;
				for (i = 0; i < len; i++)
					title[i + 1] = tmp[i];
				SetWTitle(term_window, title);
			} else {
				SetWTitle(term_window, "\pFlynn");
			}

			SetPort(save);
			return;
		}
		handle_menu(MenuKey(key));
		return;
	}

	if (conn.state != CONN_STATE_CONNECTED)
		return;

	/* Map special keys to VT100 escape sequences */
	switch (vkey) {
	case 0x7E:	/* Up arrow */
		conn_send(&conn, "\033[A", 3);
		return;
	case 0x7D:	/* Down arrow */
		conn_send(&conn, "\033[B", 3);
		return;
	case 0x7C:	/* Right arrow */
		conn_send(&conn, "\033[C", 3);
		return;
	case 0x7B:	/* Left arrow */
		conn_send(&conn, "\033[D", 3);
		return;
	case 0x73:	/* Home */
		conn_send(&conn, "\033[H", 3);
		return;
	case 0x77:	/* End */
		conn_send(&conn, "\033[F", 3);
		return;
	case 0x74:	/* Page Up */
		conn_send(&conn, "\033[5~", 4);
		return;
	case 0x79:	/* Page Down */
		conn_send(&conn, "\033[6~", 4);
		return;
	case 0x75:	/* Forward Delete */
		conn_send(&conn, "\033[3~", 4);
		return;
	case 0x33:	/* Delete/Backspace → DEL */
		key = 0x7F;
		conn_send(&conn, &key, 1);
		return;
	case 0x35:	/* Escape */
		key = 0x1B;
		conn_send(&conn, &key, 1);
		return;
	case 0x24:	/* Return */
	case 0x4C:	/* Keypad Enter */
		key = 0x0D;
		conn_send(&conn, &key, 1);
		return;
	case 0x30:	/* Tab */
		key = 0x09;
		conn_send(&conn, &key, 1);
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
			conn_send(&conn, &key, 1);
			return;
		}
	}

	/* Ctrl+key with physical Control key (extended keyboards) */
	if (event->modifiers & ControlKey) {
		key = key & 0x1F;
		conn_send(&conn, &key, 1);
		return;
	}

	/* Regular printable character */
	conn_send(&conn, &key, 1);
}

static void
handle_mouse_down(EventRecord *event)
{
	WindowPtr win;
	short part;

	part = FindWindow(event->where, &win);

	switch (part) {
	case inMenuBar:
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
			if (conn.state == CONN_STATE_CONNECTED) {
				ParamText("\pDisconnect and quit?",
				    "\p", "\p", "\p");
				if (CautionAlert(128, 0L) != 1)
					break;
			}
			do_disconnect();
			running = false;
		}
		break;
	case inContent:
		if (win != FrontWindow())
			SelectWindow(win);
		/* TODO: handle clicks in terminal content */
		break;
	}
}

static void
handle_update(EventRecord *event)
{
	WindowPtr win;
	GrafPtr old_port;

	win = (WindowPtr)event->message;
	GetPort(&old_port);
	SetPort(win);
	BeginUpdate(win);

	EraseRect(&win->portRect);

	if (conn.state == CONN_STATE_CONNECTED) {
		char title[64];
		Str255 ptitle;
		short i, len;

		len = sprintf(title, "Flynn - %s", conn.host);
		ptitle[0] = len;
		for (i = 0; i < len; i++)
			ptitle[i + 1] = title[i];
		SetWTitle(term_window, ptitle);

		/* Mark all rows dirty for full update repaints */
		for (i = 0; i < TERM_ROWS; i++)
			terminal.dirty[i] = 1;
		term_ui_draw(term_window, &terminal);
	} else {
		SetWTitle(term_window, "\pFlynn");
	}

	EndUpdate(win);
	SetPort(old_port);
}

static void
handle_activate(EventRecord *event)
{
	/* TODO: handle window activation/deactivation */
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
		switch (item) {
		case FILE_MENU_CONNECT_ID:
			do_connect();
			break;
		case FILE_MENU_DISCONNECT_ID:
			do_disconnect();
			break;
		case FILE_MENU_QUIT_ID:
			if (conn.state == CONN_STATE_CONNECTED) {
				ParamText("\pDisconnect and quit?",
				    "\p", "\p", "\p");
				if (CautionAlert(128, 0L) != 1)
					break;
			}
			do_disconnect();
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
			}
		}
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
	/* Pre-fill from saved prefs */
	if (!conn.host[0] && prefs.host[0]) {
		strcpy(conn.host, prefs.host);
		conn.port = prefs.port;
	}

	if (conn_open_dialog(&conn)) {
		telnet_init(&telnet);
		terminal_reset(&terminal);

		/* Save last-used host/port */
		strcpy(prefs.host, conn.host);
		prefs.port = conn.port;
		prefs_save(&prefs);
	}
	update_menus();
}

static void
do_disconnect(void)
{
	conn_close(&conn);
	update_menus();
}

static void
do_copy(void)
{
	char buf[TERM_ROWS * (TERM_COLS + 1)];
	short row, col, len, last_nonspace;
	TermCell *cell;

	len = 0;
	for (row = 0; row < TERM_ROWS; row++) {
		last_nonspace = -1;
		for (col = 0; col < TERM_COLS; col++) {
			cell = terminal_get_display_cell(&terminal, row, col);
			buf[len + col] = cell->ch;
			if (cell->ch != ' ')
				last_nonspace = col;
		}
		len += last_nonspace + 1;
		if (row < TERM_ROWS - 1)
			buf[len++] = '\r';
	}

	ZeroScrap();
	PutScrap(len, 'TEXT', buf);
}

static void
do_paste(void)
{
	Handle h;
	long offset, len;

	if (conn.state != CONN_STATE_CONNECTED)
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
		sent = 0;
		while (sent < len) {
			short chunk;

			chunk = len - sent;
			if (chunk > 256)
				chunk = 256;
			conn_send(&conn, p + sent, chunk);
			sent += chunk;
		}
		HUnlock(h);
	}
	DisposeHandle(h);
}
