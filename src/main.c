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

#include "main.h"
#include "connection.h"
#include "telnet.h"
#include "terminal.h"
#include "terminal_ui.h"

/* Globals */
static MenuHandle apple_menu, file_menu, edit_menu;
static WindowPtr term_window;
static Boolean running = true;
static Connection conn;
static TelnetState telnet;
static Terminal terminal;

/* Forward declarations */
static void init_toolbox(void);
static void init_menus(void);
static void init_window(void);
static void main_event_loop(void);
static Boolean handle_menu(long menu_id);
static void handle_mouse_down(EventRecord *event);
static void handle_key_down(EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void do_about(void);
static void do_connect(void);
static void do_disconnect(void);

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
main_event_loop(void)
{
	EventRecord event;
	long wait_ticks;

	while (running) {
		wait_ticks = 5L;
		WaitNextEvent(everyEvent, &event, wait_ticks, 0L);

		switch (event.what) {
		case nullEvent:
			conn_idle(&conn);
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

					terminal_process(&terminal, out_buf,
					    out_len);
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

	/* Ctrl+key: compute control character */
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
		char status[64];
		Str255 pstatus;
		short i;

		conn_status_str(&conn, status, sizeof(status));
		pstatus[0] = strlen(status);
		for (i = 0; i < pstatus[0]; i++)
			pstatus[i + 1] = status[i];
		SetWTitle(term_window, pstatus);

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
			do_disconnect();
			running = false;
			break;
		}
		break;
	case EDIT_MENU_ID:
		/* TODO: handle edit menu for DA support */
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
	ParamText("\pFlynn\r\rA Telnet client for classic Macintosh",
	    "\p", "\p", "\p");
	Alert(128, 0L);
}

static void
do_connect(void)
{
	if (conn_open_dialog(&conn)) {
		telnet_init(&telnet);
		terminal_reset(&terminal);
	}
}

static void
do_disconnect(void)
{
	conn_close(&conn);
}
