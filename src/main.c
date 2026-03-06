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
static MenuHandle apple_menu, file_menu, edit_menu, settings_menu;
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
static void update_font_menu(void);
static void rebuild_bookmark_menu(void);
static void main_event_loop(void);
static Boolean handle_menu(long menu_id);
static void handle_mouse_down(EventRecord *event);
static void handle_key_down(EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void do_about(void);
static void do_connect(void);
static void do_connect_bookmark(short index);
static void do_disconnect(void);
static void do_bookmarks(void);
static void do_font_change(short font_id, short font_size);
static void do_copy(void);
static void do_paste(void);
static short pixel_to_row(short v);
static short pixel_to_col(short h);
static void handle_content_click(EventRecord *event);
static void track_selection_drag(void);

int
main(void)
{
	init_toolbox();
	init_menus();

	memset(&conn, 0, sizeof(conn));
	conn_init();
	telnet_init(&telnet);
	terminal_init(&terminal);
	prefs_load(&prefs);

	init_window();

	/* Apply saved font and compute grid */
	term_ui_set_font(term_window, prefs.font_id, prefs.font_size);
	terminal.active_cols = (MAX_WIN_WIDTH - LEFT_MARGIN * 2) /
	    g_cell_width;
	terminal.active_rows = (MAX_WIN_HEIGHT - TOP_MARGIN * 2) /
	    g_cell_height;
	if (terminal.active_cols > TERM_COLS)
		terminal.active_cols = TERM_COLS;
	if (terminal.active_rows > TERM_ROWS)
		terminal.active_rows = TERM_ROWS;
	terminal.scroll_bottom = terminal.active_rows - 1;
	telnet.cols = terminal.active_cols;
	telnet.rows = terminal.active_rows;

	/* Size window to fit the grid */
	SizeWindow(term_window,
	    LEFT_MARGIN * 2 + terminal.active_cols * g_cell_width,
	    TOP_MARGIN * 2 + terminal.active_rows * g_cell_height, true);

	rebuild_bookmark_menu();
	update_menus();
	update_font_menu();

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
	settings_menu = GetMenuHandle(SETTINGS_MENU_ID);

	DrawMenuBar();
}

static void
init_window(void)
{
	Rect bounds;

	SetRect(&bounds, 10, 40,
	    10 + MAX_WIN_WIDTH, 40 + MAX_WIN_HEIGHT);
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
	short i;

	connected = (conn.state == CONN_STATE_CONNECTED);

	/* Session menu: Connect vs Disconnect */
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

	/* Bookmark menu items: enable only when disconnected */
	for (i = 0; i < prefs.bookmark_count; i++) {
		if (connected)
			DisableItem(file_menu,
			    FILE_MENU_BM_BASE + i);
		else
			EnableItem(file_menu,
			    FILE_MENU_BM_BASE + i);
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
	short prev_state;

	while (running) {
		wait_ticks = 1L;
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

					/* Clear selection on new data */
					if (term_ui_sel_active()) {
						term_ui_sel_dirty_all(
						    &terminal);
						term_ui_sel_clear();
					}

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

					/* Update window title from OSC */
					if (terminal.title_changed) {
						if (terminal.window_title[0]) {
							char tmp[80];
							Str255 pt;
							short tl, ti;

							tl = sprintf(tmp,
							    "Flynn - %s",
							    terminal.
							    window_title);
							pt[0] = tl;
							for (ti = 0;
							    ti < tl; ti++)
								pt[ti+1] =
								    tmp[ti];
							SetWTitle(
							    term_window, pt);
						} else {
							SetWTitle(
							    term_window,
							    "\pFlynn");
						}
						terminal.title_changed = 0;
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
					    terminal.active_rows);
				else
					terminal_scroll_back(&terminal, 1);
			} else {
				if (event->modifiers & shiftKey)
					terminal_scroll_forward(&terminal,
					    terminal.active_rows);
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
		/* Cmd+. sends Escape (classic Mac convention) */
		if (key == '.' && conn.state == CONN_STATE_CONNECTED) {
			char esc = 0x1B;
			conn_send(&conn, &esc, 1);
			return;
		}

		/* Cmd+1..0 → F1-F10 for M0110 keyboards */
		if (conn.state == CONN_STATE_CONNECTED &&
		    key >= '0' && key <= '9') {
			switch (key) {
			case '1': conn_send(&conn, "\033OP", 3); return;
			case '2': conn_send(&conn, "\033OQ", 3); return;
			case '3': conn_send(&conn, "\033OR", 3); return;
			case '4': conn_send(&conn, "\033OS", 3); return;
			case '5': conn_send(&conn, "\033[15~", 5); return;
			case '6': conn_send(&conn, "\033[17~", 5); return;
			case '7': conn_send(&conn, "\033[18~", 5); return;
			case '8': conn_send(&conn, "\033[19~", 5); return;
			case '9': conn_send(&conn, "\033[20~", 5); return;
			case '0': conn_send(&conn, "\033[21~", 5); return;
			}
		}

		handle_menu(MenuKey(key));
		return;
	}

	/* Clear selection on any non-Cmd keypress */
	if (term_ui_sel_active()) {
		GrafPtr save;

		term_ui_sel_dirty_all(&terminal);
		term_ui_sel_clear();
		GetPort(&save);
		SetPort(term_window);
		term_ui_draw(term_window, &terminal);
		SetPort(save);
	}

	if (conn.state != CONN_STATE_CONNECTED)
		return;

	/* Application keypad mode (DECKPAM): numpad sends SS3 sequences */
	if (terminal.keypad_mode) {
		switch (vkey) {
		case 0x52: conn_send(&conn, "\033Op", 3); return;  /* KP 0 */
		case 0x53: conn_send(&conn, "\033Oq", 3); return;  /* KP 1 */
		case 0x54: conn_send(&conn, "\033Or", 3); return;  /* KP 2 */
		case 0x55: conn_send(&conn, "\033Os", 3); return;  /* KP 3 */
		case 0x56: conn_send(&conn, "\033Ot", 3); return;  /* KP 4 */
		case 0x57: conn_send(&conn, "\033Ou", 3); return;  /* KP 5 */
		case 0x58: conn_send(&conn, "\033Ov", 3); return;  /* KP 6 */
		case 0x59: conn_send(&conn, "\033Ow", 3); return;  /* KP 7 */
		case 0x5B: conn_send(&conn, "\033Ox", 3); return;  /* KP 8 */
		case 0x5C: conn_send(&conn, "\033Oy", 3); return;  /* KP 9 */
		case 0x41: conn_send(&conn, "\033On", 3); return;  /* KP . */
		case 0x4C: conn_send(&conn, "\033OM", 3); return;  /* KP Enter */
		case 0x45: conn_send(&conn, "\033Ok", 3); return;  /* KP + */
		case 0x4E: conn_send(&conn, "\033Om", 3); return;  /* KP - */
		case 0x43: conn_send(&conn, "\033Oj", 3); return;  /* KP * */
		}
	}

	/* Map special keys to escape sequences */
	switch (vkey) {
	case 0x7E:	/* Up arrow */
		if (terminal.cursor_key_mode)
			conn_send(&conn, "\033OA", 3);
		else
			conn_send(&conn, "\033[A", 3);
		return;
	case 0x7D:	/* Down arrow */
		if (terminal.cursor_key_mode)
			conn_send(&conn, "\033OB", 3);
		else
			conn_send(&conn, "\033[B", 3);
		return;
	case 0x7C:	/* Right arrow */
		if (terminal.cursor_key_mode)
			conn_send(&conn, "\033OC", 3);
		else
			conn_send(&conn, "\033[C", 3);
		return;
	case 0x7B:	/* Left arrow */
		if (terminal.cursor_key_mode)
			conn_send(&conn, "\033OD", 3);
		else
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
	case 0x47:	/* Clear/NumLock → Escape (M0110A keypad) */
		key = 0x1B;
		conn_send(&conn, &key, 1);
		return;
	/* Function keys F1-F12 (ADB extended keyboards) */
	case 0x7A:	/* F1 */
		conn_send(&conn, "\033OP", 3);
		return;
	case 0x78:	/* F2 */
		conn_send(&conn, "\033OQ", 3);
		return;
	case 0x63:	/* F3 */
		conn_send(&conn, "\033OR", 3);
		return;
	case 0x76:	/* F4 */
		conn_send(&conn, "\033OS", 3);
		return;
	case 0x60:	/* F5 */
		conn_send(&conn, "\033[15~", 5);
		return;
	case 0x61:	/* F6 */
		conn_send(&conn, "\033[17~", 5);
		return;
	case 0x62:	/* F7 */
		conn_send(&conn, "\033[18~", 5);
		return;
	case 0x64:	/* F8 */
		conn_send(&conn, "\033[19~", 5);
		return;
	case 0x65:	/* F9 */
		conn_send(&conn, "\033[20~", 5);
		return;
	case 0x6D:	/* F10 */
		conn_send(&conn, "\033[21~", 5);
		return;
	case 0x67:	/* F11 */
		conn_send(&conn, "\033[23~", 5);
		return;
	case 0x6F:	/* F12 */
		conn_send(&conn, "\033[24~", 5);
		return;
	}

	/* ESC character from any source (keyboard adapters, Clear key) */
	if (key == 0x1B) {
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
		else
			handle_content_click(event);
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
		char title[80];
		Str255 ptitle;
		short i, len;

		if (terminal.window_title[0])
			len = sprintf(title, "Flynn - %s",
			    terminal.window_title);
		else
			len = sprintf(title, "Flynn - %s", conn.host);
		ptitle[0] = len;
		for (i = 0; i < len; i++)
			ptitle[i + 1] = title[i];
		SetWTitle(term_window, ptitle);

		/* Mark all rows dirty for full update repaints */
		for (i = 0; i < terminal.active_rows; i++)
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
	case SETTINGS_MENU_ID:
		switch (item) {
		case SETTINGS_FONT_9_ID:
			do_font_change(4, 9);
			break;
		case SETTINGS_FONT_12_ID:
			do_font_change(4, 12);
			break;
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
do_connect_bookmark(short index)
{
	Bookmark *bm;

	if (index < 0 || index >= prefs.bookmark_count)
		return;
	if (conn.state != CONN_STATE_IDLE)
		return;

	bm = &prefs.bookmarks[index];
	if (conn_connect(&conn, bm->host, bm->port)) {
		telnet_init(&telnet);
		terminal_reset(&terminal);

		strcpy(prefs.host, conn.host);
		prefs.port = conn.port;
		prefs_save(&prefs);
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
	char line[64];
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
	conn_close(&conn);
	terminal_reset(&terminal);
	telnet_init(&telnet);
	SetWTitle(term_window, "\pFlynn");

	{
		GrafPtr save;
		short i;

		GetPort(&save);
		SetPort(term_window);
		EraseRect(&term_window->portRect);
		for (i = 0; i < terminal.active_rows; i++)
			terminal.dirty[i] = 1;
		term_ui_draw(term_window, &terminal);
		SetPort(save);
	}

	update_menus();
}

static void
do_font_change(short font_id, short font_size)
{
	short new_cols, new_rows;
	GrafPtr save;
	short i;

	if (font_id == prefs.font_id && font_size == prefs.font_size)
		return;

	term_ui_set_font(term_window, font_id, font_size);

	/* Compute new grid */
	new_cols = (MAX_WIN_WIDTH - LEFT_MARGIN * 2) / g_cell_width;
	new_rows = (MAX_WIN_HEIGHT - TOP_MARGIN * 2) / g_cell_height;
	if (new_cols > TERM_COLS)
		new_cols = TERM_COLS;
	if (new_rows > TERM_ROWS)
		new_rows = TERM_ROWS;

	terminal.active_cols = new_cols;
	terminal.active_rows = new_rows;
	terminal.scroll_bottom = new_rows - 1;
	terminal.scroll_top = 0;
	if (terminal.cur_col >= new_cols)
		terminal.cur_col = new_cols - 1;
	if (terminal.cur_row >= new_rows)
		terminal.cur_row = new_rows - 1;

	/* Resize window */
	SizeWindow(term_window,
	    LEFT_MARGIN * 2 + new_cols * g_cell_width,
	    TOP_MARGIN * 2 + new_rows * g_cell_height, true);

	/* Clear and redraw */
	GetPort(&save);
	SetPort(term_window);
	EraseRect(&term_window->portRect);
	for (i = 0; i < new_rows; i++)
		terminal.dirty[i] = 1;
	term_ui_draw(term_window, &terminal);
	SetPort(save);

	/* Send NAWS if connected */
	if (conn.state == CONN_STATE_CONNECTED) {
		unsigned char naws_buf[32];
		short naws_len = 0;

		telnet.cols = new_cols;
		telnet.rows = new_rows;
		telnet_send_naws(&telnet, naws_buf, &naws_len,
		    new_cols, new_rows);
		if (naws_len > 0)
			conn_send(&conn, (char *)naws_buf, naws_len);
	}

	/* Save preference */
	prefs.font_id = font_id;
	prefs.font_size = font_size;
	prefs_save(&prefs);

	update_font_menu();
}

static void
update_font_menu(void)
{
	if (!settings_menu)
		return;
	CheckItem(settings_menu, SETTINGS_FONT_9_ID,
	    prefs.font_size == 9);
	CheckItem(settings_menu, SETTINGS_FONT_12_ID,
	    prefs.font_size == 12);
}

static void
do_copy(void)
{
	char buf[TERM_ROWS * (TERM_COLS + 1)];
	short row, col, len, last_nonspace;
	TermCell *cell;

	if (term_ui_sel_active()) {
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
				c_end = terminal.active_cols - 1;
			} else if (row == er) {
				c_start = 0;
				c_end = ec;
			} else {
				c_start = 0;
				c_end = terminal.active_cols - 1;
			}

			last_nonspace = -1;
			for (col = c_start; col <= c_end; col++) {
				cell = terminal_get_display_cell(
				    &terminal, row, col);
				buf[len + (col - c_start)] = cell->ch;
				if (cell->ch != ' ')
					last_nonspace = col - c_start;
			}
			len += last_nonspace + 1;
			if (row < er)
				buf[len++] = '\r';
		}
	} else {
		len = 0;
		for (row = 0; row < terminal.active_rows; row++) {
			last_nonspace = -1;
			for (col = 0; col < terminal.active_cols; col++) {
				cell = terminal_get_display_cell(
				    &terminal, row, col);
				buf[len + col] = cell->ch;
				if (cell->ch != ' ')
					last_nonspace = col;
			}
			len += last_nonspace + 1;
			if (row < terminal.active_rows - 1)
				buf[len++] = '\r';
		}
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

		if (terminal.bracketed_paste)
			conn_send(&conn, "\033[200~", 6);

		sent = 0;
		while (sent < len) {
			short chunk;

			chunk = len - sent;
			if (chunk > 256)
				chunk = 256;
			conn_send(&conn, p + sent, chunk);
			sent += chunk;
		}

		if (terminal.bracketed_paste)
			conn_send(&conn, "\033[201~", 6);

		HUnlock(h);
	}
	DisposeHandle(h);
}

static short
pixel_to_row(short v)
{
	short r;

	r = (v - TOP_MARGIN) / g_cell_height;
	if (r < 0) r = 0;
	if (r >= terminal.active_rows) r = terminal.active_rows - 1;
	return r;
}

static short
pixel_to_col(short h)
{
	short c;

	c = (h - LEFT_MARGIN) / g_cell_width;
	if (c < 0) c = 0;
	if (c >= terminal.active_cols) c = terminal.active_cols - 1;
	return c;
}

static void
handle_content_click(EventRecord *event)
{
	Point local_pt;
	short row, col;
	GrafPtr save;
	short i;

	GetPort(&save);
	SetPort(term_window);

	local_pt = event->where;
	GlobalToLocal(&local_pt);
	row = pixel_to_row(local_pt.v);
	col = pixel_to_col(local_pt.h);

	/* Clear any previous selection first */
	if (term_ui_sel_active()) {
		term_ui_sel_dirty_all(&terminal);
		term_ui_sel_clear();
	}

	if (term_ui_sel_check_double_click(event->when, row, col)) {
		/* Double-click: word selection */
		term_ui_sel_start_word(row, col,
		    terminal.scroll_offset, &terminal);
	} else if (event->modifiers & shiftKey) {
		/* Shift-click: extend from cursor to click */
		term_ui_sel_start(terminal.cur_row,
		    terminal.cur_col, terminal.scroll_offset);
		term_ui_sel_extend(row, col, &terminal);
	} else {
		/* New selection */
		term_ui_sel_start(row, col,
		    terminal.scroll_offset);
	}

	track_selection_drag();

	/* Redraw all rows to show final selection state */
	for (i = 0; i < terminal.active_rows; i++)
		terminal.dirty[i] = 1;
	term_ui_draw(term_window, &terminal);

	SetPort(save);
}

static void
track_selection_drag(void)
{
	Point pt;
	short row, col, prev_row, prev_col;
	short old_extent_row;

	prev_row = -1;
	prev_col = -1;

	while (StillDown()) {
		GetMouse(&pt);
		row = pixel_to_row(pt.v);
		col = pixel_to_col(pt.h);

		if (row == prev_row && col == prev_col)
			continue;

		old_extent_row = row;  /* will be used after extend */
		term_ui_sel_dirty_rows(&terminal, prev_row < 0 ?
		    row : prev_row, row);
		term_ui_sel_extend(row, col, &terminal);
		term_ui_draw(term_window, &terminal);

		prev_row = row;
		prev_col = col;
	}

	term_ui_sel_finalize();
}
