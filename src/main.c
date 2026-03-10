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

/* Number of static items in File menu (before dynamic recent bookmarks) */
#define FILE_MENU_STATIC_ITEMS  5

/* Globals */
static MenuHandle apple_menu, file_menu, edit_menu, prefs_menu, ctrl_menu;
static MenuHandle window_menu;
static Boolean running = true;
static FlynnPrefs prefs;
static RgnHandle grow_clip_rgn = 0L;
static Session *active_session = 0L;

/* Save current font metrics into session */
static void
session_save_font(Session *s)
{
	s->cell_width = g_cell_width;
	s->cell_height = g_cell_height;
	s->cell_baseline = g_cell_baseline;
}

/* Restore session's font metrics into globals */
static void
session_load_font(Session *s)
{
	g_cell_width = s->cell_width;
	g_cell_height = s->cell_height;
	g_cell_baseline = s->cell_baseline;
	g_font_id = s->font_id;
	g_font_size = s->font_size;
}

/* Draw a 3-pixel rounded rect outline around the default button (item 1) */
static pascal void
draw_default_button(WindowPtr dlg, short item)
{
	short item_type;
	Handle item_h;
	Rect item_rect, outline_r;

	(void)item;  /* unused — we always outline item 1 */
	GetDialogItem((DialogPtr)dlg, 1, &item_type, &item_h, &item_rect);
	outline_r = item_rect;
	InsetRect(&outline_r, -4, -4);
	PenSize(3, 3);
	FrameRoundRect(&outline_r, 16, 16);
	PenNormal();
}

/* Register the default button outline UserItem in a dialog */
static void
setup_default_button_outline(DialogPtr dlg, short outline_item)
{
	short item_type;
	Handle item_h;
	Rect item_rect;

	GetDialogItem(dlg, outline_item, &item_type, &item_h, &item_rect);
	SetDialogItem(dlg, outline_item, userItem,
	    (Handle)draw_default_button, &item_rect);
}

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
static void rebuild_file_menu(void);
static void add_recent_bookmark(short index);
static void main_event_loop(void);
static Boolean handle_menu(long menu_id);
static void handle_mouse_down(EventRecord *event);
static void handle_key_down(Session *s, EventRecord *event);
static void handle_update(EventRecord *event);
static void handle_activate(EventRecord *event);
static void do_about(void);
static void do_connect(void);
static void do_connect_bookmark(short index);
static void do_bookmarks(void);
static void do_save_as_bookmark(void);
static void do_font_change(short font_id, short font_size);
static void do_window_resize(Session *s, short width, short height);
static void do_copy(void);
static void do_paste(void);
static void do_select_all(void);
static void do_dns_server_dialog(void);
static pascal Boolean std_dlg_filter(DialogPtr dlg, EventRecord *evt,
    short *item);
static short pixel_to_row(Session *s, short v);
static short pixel_to_col(Session *s, short h);
static void handle_content_click(Session *s, EventRecord *event);
static void track_selection_drag(Session *s);

/* Font preset table for bookmark font cycling */
typedef struct {
	short	font_id;
	short	font_size;
	char	name[16];
} FontPreset;

static FontPreset font_presets[] = {
	{ 4, 9, "Monaco 9" },
	{ 4, 12, "Monaco 12" },
	{ 22, 10, "Courier 10" },
	{ 0, 12, "Chicago 12" },
	{ 3, 9, "Geneva 9" },
	{ 3, 10, "Geneva 10" }
};
#define NUM_FONT_PRESETS 6

static void
ttype_to_str(short ttype, char *buf, short buflen)
{
	const char *str;
	switch (ttype) {
	case 0:  str = "xterm"; break;
	case 1:  str = "VT220"; break;
	case 2:  str = "VT100"; break;
	case 3:  str = "xterm-256color"; break;
	default: str = "Default"; break;
	}
	strncpy(buf, str, buflen - 1);
	buf[buflen - 1] = '\0';
}

static void
font_to_str(short font_id, short font_size, char *buf, short buflen)
{
	short i;

	if (font_id == 0 && font_size == 0) {
		strncpy(buf, "Default", buflen - 1);
		buf[buflen - 1] = '\0';
		return;
	}
	for (i = 0; i < NUM_FONT_PRESETS; i++) {
		if (font_presets[i].font_id == font_id &&
		    font_presets[i].font_size == font_size) {
			strncpy(buf, font_presets[i].name, buflen - 1);
			buf[buflen - 1] = '\0';
			return;
		}
	}
	snprintf(buf, buflen, "Font %d/%d", font_id, font_size);
}

int
main(void)
{
	init_toolbox();
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

	connected = (active_session &&
	    active_session->conn.state == CONN_STATE_CONNECTED);

	/* File menu: New Session always enabled, Close when session exists */
	EnableItem(file_menu, FILE_MENU_CONNECT_ID);
	if (active_session)
		EnableItem(file_menu, FILE_MENU_DISCONNECT_ID);
	else
		DisableItem(file_menu, FILE_MENU_DISCONNECT_ID);

	/* Save as Bookmark: only when connected and not already from a bookmark */
	if (active_session &&
	    active_session->conn.state == CONN_STATE_CONNECTED &&
	    active_session->bookmark_index < 0 &&
	    prefs.bookmark_count < MAX_BOOKMARKS)
		EnableItem(file_menu, FILE_MENU_SAVE_BM_ID);
	else
		DisableItem(file_menu, FILE_MENU_SAVE_BM_ID);

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

		for (ci = CTRL_MENU_CTRLC; ci <= CTRL_MENU_ESC; ci++) {
			if (connected)
				EnableItem(ctrl_menu, ci);
			else
				DisableItem(ctrl_menu, ci);
		}
	}

	update_window_menu();
	update_prefs_menu();
}

static void
update_window_menu(void)
{
	short count, i, item;
	Str255 title;
	Session *s;
	char count_str[32];
	short sess_count;

	if (!window_menu)
		return;

	/* Remove all items */
	count = CountMItems(window_menu);
	while (count > 0) {
		DeleteMenuItem(window_menu, count);
		count--;
	}

	/* Add count header (disabled) */
	sess_count = session_count();
	sprintf(count_str, "%d of %d Sessions", sess_count, MAX_SESSIONS);
	{
		Str255 ps;
		short len = strlen(count_str);

		ps[0] = len;
		memcpy(ps + 1, count_str, len);
		AppendMenu(window_menu, "\p ");
		SetMenuItemText(window_menu, 1, ps);
	}
	DisableItem(window_menu, 1);

	/* Separator */
	AppendMenu(window_menu, "\p(-");

	/* Append one item per session (starting at WIN_MENU_FIRST_WIN) */
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

				/* Load this session's UI + font state */
				term_ui_load_state(&sess->ui);
				session_load_font(sess);

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

					/* Set title to show disconnected state */
					if (sess->conn.host[0]) {
						char dtmp[80];
						Str255 dtitle;
						short dl, di;

						dl = sprintf(dtmp,
						    "Flynn - %s "
						    "(disconnected)",
						    sess->conn.host);
						if (dl > 254)
							dl = 254;
						dtitle[0] = dl;
						for (di = 0;
						    di < dl; di++)
							dtitle[di + 1] =
							    dtmp[di];
						SetWTitle(
						    sess->window,
						    dtitle);
					} else {
						SetWTitle(
						    sess->window,
						    "\pFlynn");
					}

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
						NoteAlert(128, 0L);
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

	if (prefs.dark_mode)
		PaintRect(&win->portRect);
	else
		EraseRect(&win->portRect);

	if (sess) {
		term_ui_load_state(&sess->ui);
		session_load_font(sess);

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
			/* Load incoming session's UI + font state */
			term_ui_load_state(&sess->ui);
			session_load_font(sess);
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
		switch (item) {
		case FILE_MENU_CONNECT_ID:
			do_connect();
			break;
		case FILE_MENU_DISCONNECT_ID:
			if (active_session) {
				if (active_session->conn.state ==
				    CONN_STATE_CONNECTED) {
					ParamText(
					    "\pDisconnect and close "
					    "session?",
					    "\p", "\p", "\p");
					if (CautionAlert(128, 0L) != 1)
						break;
				}
				term_ui_load_state(
				    &active_session->ui);
				session_destroy(active_session);
				active_session =
				    session_from_window(
				    FrontWindow());
				update_menus();
			}
			break;
		case FILE_MENU_SAVE_BM_ID:
			do_save_as_bookmark();
			break;
		case FILE_MENU_BOOKMARKS_ID:
			do_bookmarks();
			break;
		default:
			/* Quit is always last item */
			if (item == CountMItems(file_menu)) {
				if (session_any_connected()) {
					ParamText(
					    "\pDisconnect all "
					    "sessions and quit?",
					    "\p", "\p", "\p");
					if (CautionAlert(128, 0L)
					    != 1)
						break;
				}
				{
					short si;
					Session *sess;

					for (si = MAX_SESSIONS - 1;
					    si >= 0; si--) {
						sess =
						    session_get(si);
						if (sess)
						    session_destroy(
						    sess);
					}
				}
				active_session = 0L;
				running = false;
			} else if (item > FILE_MENU_STATIC_ITEMS
			    && prefs.recent_count > 0) {
				/* Recent bookmark click */
				short ri = item -
				    FILE_MENU_STATIC_ITEMS - 1;

				if (ri >= 0 &&
				    ri < prefs.recent_count) {
					short bm_idx =
					    prefs.recent[ri];

					if (bm_idx >= 0 &&
					    bm_idx <
					    prefs.bookmark_count)
						do_connect_bookmark(
						    bm_idx);
				}
			}
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
			case CTRL_MENU_CTRLL:
				ctrl_byte = 0x0C;
				conn_send(&active_session->conn,
				    &ctrl_byte, 1);
				break;
			case CTRL_MENU_CTRLZ:
				ctrl_byte = 0x1A;
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
			case CTRL_MENU_ESC:
				ctrl_byte = 0x1B;
				conn_send(&active_session->conn,
				    &ctrl_byte, 1);
				break;
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
		case PREFS_XTERM256_ID:
			if (active_session)
				active_session->telnet.preferred_ttype =
				    item - PREFS_XTERM_ID;
			/* Auto-save to originating bookmark */
			if (active_session &&
			    active_session->bookmark_index >= 0 &&
			    active_session->bookmark_index <
			    prefs.bookmark_count) {
				prefs.bookmarks[
				    active_session->bookmark_index
				    ].terminal_type =
				    item - PREFS_XTERM_ID;
			}
			/* Also update global default */
			prefs.terminal_type = item - PREFS_XTERM_ID;
			prefs_save(&prefs);
			update_prefs_menu();
			if (active_session &&
			    active_session->conn.state ==
			    CONN_STATE_CONNECTED) {
				ParamText(
				    "\pTerminal type change takes "
				    "effect on next connection.",
				    "\p", "\p", "\p");
				NoteAlert(128, 0L);
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
					session_load_font(ws);
					break;
				}
				count++;
			}
			break;
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

	/* Register default button outline */
	setup_default_button_outline(dlg, 10);

	ModalDialog((ModalFilterUPP)std_dlg_filter, &item);
	DisposeDialog(dlg);
}

/* Forward declaration for button title helper */
static void bme_set_btn_title(DialogPtr dlg, short item,
    const char *text);

/* Bookmark popup menu, shared with dialog filter */
static MenuHandle g_bm_popup;
static short g_bm_selected = -1;  /* bookmark index selected from popup */
static short g_connect_ttype;     /* terminal type selected in connect dialog */

static pascal Boolean
connect_dlg_filter(DialogPtr dlg, EventRecord *evt, short *item)
{
	/* Return/Enter key maps to Connect button */
	if (evt->what == keyDown) {
		char key = evt->message & charCodeMask;
		if (key == '\r' || key == '\n' || key == 0x03) {
			*item = 1;  /* Connect button */
			return true;
		}
		/* Cmd+. maps to Cancel button */
		if ((evt->modifiers & cmdKey) && key == '.') {
			*item = 2;  /* Cancel button */
			return true;
		}
		/* Tab cycles through edit fields: Host(4)->Port(6)->User(9) */
		if (key == '\t') {
			DialogPeek dp = (DialogPeek)dlg;
			short cur = dp->editField + 1;  /* 1-based */
			short next;

			if (cur == DLOG_HOST_FIELD)
				next = DLOG_PORT_FIELD;
			else if (cur == DLOG_PORT_FIELD)
				next = DLOG_USER_FIELD;
			else
				next = DLOG_HOST_FIELD;
			SelectDialogItemText(dlg, next, 0, 32767);
			*item = next;
			return true;
		}
	}

	if (evt->what == mouseDown) {
		Point pt;
		short item_type;
		Handle item_h;
		Rect item_rect;

		pt = evt->where;
		SetPort(dlg);
		GlobalToLocal(&pt);

		/* Terminal type popup menu */
		GetDialogItem(dlg, DLOG_TTYPE_BTN,
		    &item_type, &item_h, &item_rect);
		if (PtInRect(pt, &item_rect)) {
			MenuHandle popup;
			Point popup_pt;
			long result;
			short choice;

			popup = NewMenu(201, "\p");
			AppendMenu(popup, "\pxterm");
			AppendMenu(popup, "\pVT220");
			AppendMenu(popup, "\pVT100");
			AppendMenu(popup, "\pxterm-256color");
			InsertMenu(popup, -1);

			CheckItem(popup, g_connect_ttype + 1, true);

			popup_pt.h = item_rect.left;
			popup_pt.v = item_rect.top;
			LocalToGlobal(&popup_pt);

			result = PopUpMenuSelect(popup,
			    popup_pt.v, popup_pt.h,
			    g_connect_ttype + 1);
			choice = LoWord(result);

			if (choice > 0) {
				char btn_text[32];

				g_connect_ttype = choice - 1;
				ttype_to_str(g_connect_ttype,
				    btn_text, sizeof(btn_text));
				bme_set_btn_title(dlg,
				    DLOG_TTYPE_BTN, btn_text);
			}

			DeleteMenu(201);
			DisposeMenu(popup);

			*item = DLOG_TTYPE_BTN;
			return true;
		}

		/* Bookmark popup menu */
		if (g_bm_popup) {
			GetDialogItem(dlg, DLOG_BOOKMARKS,
			    &item_type, &item_h, &item_rect);

			if (PtInRect(pt, &item_rect)) {
				long choice;
				Point popup_pt;

				if (g_bm_selected >= 0)
					CheckItem(g_bm_popup,
					    g_bm_selected + 1,
					    true);

				popup_pt.h = item_rect.left;
				popup_pt.v = item_rect.top;
				LocalToGlobal(&popup_pt);

				choice = PopUpMenuSelect(g_bm_popup,
				    popup_pt.v, popup_pt.h,
				    g_bm_selected >= 0 ?
				    g_bm_selected + 1 : 0);

				if (g_bm_selected >= 0)
					CheckItem(g_bm_popup,
					    g_bm_selected + 1,
					    false);

				if (HiWord(choice) != 0) {
					short sel;
					Bookmark *bm;
					Str255 pstr;
					short i;

					sel = LoWord(choice) - 1;
					g_bm_selected = sel;
					bm = &prefs.bookmarks[sel];

					/* Update button to
					   show bookmark name */
					bme_set_btn_title(dlg,
					    DLOG_BOOKMARKS,
					    bm->name);

					/* Fill host */
					GetDialogItem(dlg,
					    DLOG_HOST_FIELD,
					    &item_type, &item_h,
					    &item_rect);
					pstr[0] = strlen(bm->host);
					for (i = 0;
					    i < pstr[0]; i++)
						pstr[i + 1] =
						    bm->host[i];
					SetDialogItemText(
					    item_h, pstr);

					/* Fill port */
					GetDialogItem(dlg,
					    DLOG_PORT_FIELD,
					    &item_type, &item_h,
					    &item_rect);
					sprintf(
					    (char *)&pstr[1],
					    "%d", bm->port);
					pstr[0] = strlen(
					    (char *)&pstr[1]);
					SetDialogItemText(
					    item_h, pstr);

					/* Fill username */
					GetDialogItem(dlg,
					    DLOG_USER_FIELD,
					    &item_type, &item_h,
					    &item_rect);
					if (bm->username[0]) {
						pstr[0] = strlen(
						    bm->username);
						for (i = 0;
						    i < pstr[0];
						    i++)
							pstr[i+1] =
							    bm->
							    username
							    [i];
					} else {
						pstr[0] = 0;
					}
					SetDialogItemText(
					    item_h, pstr);

					/* Fill terminal type */
					if (bm->terminal_type
					    >= 0)
						g_connect_ttype =
						    bm->
						    terminal_type;
					else
						g_connect_ttype =
						    prefs.
						    terminal_type;
					{
						char btn_text[32];
						ttype_to_str(
						    g_connect_ttype,
						    btn_text,
						    sizeof(btn_text));
						bme_set_btn_title(
						    dlg,
						    DLOG_TTYPE_BTN,
						    btn_text);
					}

					SelectDialogItemText(
					    dlg,
					    DLOG_HOST_FIELD,
					    0, 32767);
				}

				*item = 0;
				return true;
			}
		}
	}
	return false;  /* let ModalDialog handle it */
}

static void
do_connect(void)
{
	Session *s = active_session;
	Boolean need_new_session = false;
	char prefill_host[128];
	short prefill_port;
	char prefill_user[32];

	/* Determine if we need a new session, but defer creation
	 * until after the user clicks OK in the dialog */
	if (!s) {
		need_new_session = true;
	} else if (s->conn.state == CONN_STATE_CONNECTED) {
		need_new_session = true;
		s = 0L;  /* will create after dialog */
	}

	/* Set up pre-fill values from prefs (no session yet) or
	 * from existing disconnected session */
	if (need_new_session) {
		strncpy(prefill_host, prefs.host,
		    sizeof(prefill_host) - 1);
		prefill_host[sizeof(prefill_host) - 1] = '\0';
		prefill_port = prefs.port;
		strncpy(prefill_user, prefs.username,
		    sizeof(prefill_user) - 1);
		prefill_user[sizeof(prefill_user) - 1] = '\0';
	} else {
		/* Existing disconnected session — pre-fill from it
		 * or fall back to prefs */
		if (s->conn.host[0]) {
			strncpy(prefill_host, s->conn.host,
			    sizeof(prefill_host) - 1);
			prefill_host[sizeof(prefill_host) - 1] = '\0';
			prefill_port = s->conn.port;
		} else {
			strncpy(prefill_host, prefs.host,
			    sizeof(prefill_host) - 1);
			prefill_host[sizeof(prefill_host) - 1] = '\0';
			prefill_port = prefs.port;
		}
		if (s->conn.username[0]) {
			strncpy(prefill_user, s->conn.username,
			    sizeof(prefill_user) - 1);
			prefill_user[sizeof(prefill_user) - 1] = '\0';
		} else {
			strncpy(prefill_user, prefs.username,
			    sizeof(prefill_user) - 1);
			prefill_user[sizeof(prefill_user) - 1] = '\0';
		}
	}

	g_connect_ttype = prefs.terminal_type;

	/* Show connect dialog with bookmark support */
	{
		DialogPtr dlg;
		short item_hit;
		Handle item_h;
		short item_type;
		Rect item_rect;
		Str255 pstr;
		long port_num;
		short i;
		Boolean connected = false;
		char dlg_host[128];
		short dlg_port;
		char dlg_user[32];

		dlg_host[0] = '\0';
		dlg_port = DEFAULT_PORT;
		dlg_user[0] = '\0';

		dlg = GetNewDialog(DLOG_CONNECT_ID, 0L,
		    (WindowPtr)-1L);
		if (!dlg) {
			SysBeep(10);
			update_menus();
			return;
		}

		/* Pre-fill host */
		if (prefill_host[0]) {
			GetDialogItem(dlg, DLOG_HOST_FIELD,
			    &item_type, &item_h, &item_rect);
			pstr[0] = strlen(prefill_host);
			for (i = 0; i < pstr[0]; i++)
				pstr[i + 1] = prefill_host[i];
			SetDialogItemText(item_h, pstr);
		}
		/* Pre-fill port */
		if (prefill_port > 0) {
			GetDialogItem(dlg, DLOG_PORT_FIELD,
			    &item_type, &item_h, &item_rect);
			sprintf((char *)&pstr[1], "%d",
			    prefill_port);
			pstr[0] = strlen((char *)&pstr[1]);
			SetDialogItemText(item_h, pstr);
		}
		/* Pre-fill username */
		if (prefill_user[0]) {
			GetDialogItem(dlg, DLOG_USER_FIELD,
			    &item_type, &item_h, &item_rect);
			pstr[0] = strlen(prefill_user);
			for (i = 0; i < pstr[0]; i++)
				pstr[i + 1] = prefill_user[i];
			SetDialogItemText(item_h, pstr);
		}

		/* Hide Bookmarks button if no bookmarks saved */
		if (prefs.bookmark_count <= 0) {
			GetDialogItem(dlg, DLOG_BOOKMARKS,
			    &item_type, &item_h, &item_rect);
			HideDialogItem(dlg, DLOG_BOOKMARKS);
		}

		/* Build bookmark popup menu */
		g_bm_popup = 0L;
		g_bm_selected = -1;
		if (prefs.bookmark_count > 0) {
			short bmi;

			g_bm_popup = NewMenu(200, "\p");
			for (bmi = 0;
			    bmi < prefs.bookmark_count;
			    bmi++) {
				Str255 bm_item;
				short ni, nlen;

				nlen = strlen(
				    prefs.bookmarks[bmi].name);
				if (nlen > 254) nlen = 254;
				bm_item[0] = nlen;
				for (ni = 0; ni < nlen; ni++)
					bm_item[ni + 1] =
					    prefs.bookmarks
					    [bmi].name[ni];
				AppendMenu(g_bm_popup, "\p ");
				SetMenuItemText(g_bm_popup,
				    bmi + 1, bm_item);
			}
			InsertMenu(g_bm_popup, -1);
		}

		/* Set terminal type button text */
		{
			char btn_text[32];
			ttype_to_str(g_connect_ttype, btn_text, sizeof(btn_text));
			bme_set_btn_title(dlg, DLOG_TTYPE_BTN,
			    btn_text);
		}

		/* Register default button outline */
		setup_default_button_outline(dlg,
		    DLOG_DEFAULT_BTN);

		ShowWindow(dlg);

		for (;;) {
			ModalDialog(
			    (ModalFilterUPP)connect_dlg_filter,
			    &item_hit);

			if (item_hit == DLOG_CANCEL ||
			    item_hit == DLOG_OK)
				break;

			/* Terminal type handled by filter proc popup */
		}

		if (g_bm_popup) {
			DeleteMenu(200);
			DisposeMenu(g_bm_popup);
			g_bm_popup = 0L;
		}

		if (item_hit == DLOG_OK) {
			/* Extract host into local buffer */
			GetDialogItem(dlg, DLOG_HOST_FIELD,
			    &item_type, &item_h, &item_rect);
			GetDialogItemText(item_h, pstr);
			if (pstr[0] > 0) {
				for (i = 0; i < pstr[0] &&
				    i < (short)(sizeof(
				    dlg_host) - 1); i++)
					dlg_host[i] =
					    pstr[i + 1];
				dlg_host[i] = '\0';
			}

			/* Extract port */
			GetDialogItem(dlg, DLOG_PORT_FIELD,
			    &item_type, &item_h, &item_rect);
			GetDialogItemText(item_h, pstr);
			if (pstr[0] > 0) {
				pstr[pstr[0] + 1] = '\0';
				StringToNum(pstr, &port_num);
				dlg_port = (short)port_num;
			} else {
				dlg_port = DEFAULT_PORT;
			}

			/* Extract username */
			GetDialogItem(dlg, DLOG_USER_FIELD,
			    &item_type, &item_h, &item_rect);
			GetDialogItemText(item_h, pstr);
			if (pstr[0] > 0 && pstr[0] <
			    (short)(sizeof(dlg_user) - 1)) {
				for (i = 0; i < pstr[0]; i++)
					dlg_user[i] =
					    pstr[i + 1];
				dlg_user[i] = '\0';
			} else {
				dlg_user[0] = '\0';
			}

			DisposeDialog(dlg);

			/* Now create session if needed (dialog is
			 * dismissed, so user sees it fast) */
			if (need_new_session) {
				/* Ensure font metrics are set before
				 * session_new() — it uses g_cell_width
				 * and g_cell_height for window sizing */
				if (g_cell_width == 0) {
					GrafPtr save_port;
					GrafPort temp_port;

					GetPort(&save_port);
					OpenPort(&temp_port);
					term_ui_set_font(
					    (WindowPtr)&temp_port,
					    prefs.font_id,
					    prefs.font_size);
					ClosePort(&temp_port);
					SetPort(save_port);
				}

				s = session_new();
				if (!s) {
					ParamText(
					    "\pOut of memory",
					    "\p", "\p", "\p");
					StopAlert(128, 0L);
					update_menus();
					return;
				}
				SetPort(s->window);
				s->font_id = prefs.font_id;
				s->font_size = prefs.font_size;
				term_ui_set_font(s->window,
				    s->font_id, s->font_size);
				session_save_font(s);
				term_ui_init(s->window,
				    &s->terminal);
				term_ui_save_state(&s->ui);
				s->conn.dns_server =
				    ip2long(prefs.dns_server);
				if (prefs.dark_mode)
					PaintRect(
					    &s->window->portRect);
				if (active_session &&
				    active_session->conn.state ==
				    CONN_STATE_CONNECTED)
					SelectWindow(s->window);
				active_session = s;
			}

			/* Copy dialog values into session */
			strncpy(s->conn.host, dlg_host,
			    sizeof(s->conn.host) - 1);
			s->conn.host[sizeof(s->conn.host) - 1] =
			    '\0';
			s->conn.port = dlg_port;
			strncpy(s->conn.username, dlg_user,
			    sizeof(s->conn.username) - 1);
			s->conn.username[
			    sizeof(s->conn.username) - 1] = '\0';

			if (s->conn.host[0]) {
				WindowPtr sw;
				char smsg[80];

				if (ip2long(s->conn.host) != 0)
					sprintf(smsg,
					    "Connecting to %.40s\311",
					    s->conn.host);
				else
					sprintf(smsg,
					    "Resolving %.40s\311",
					    s->conn.host);
				sw = conn_status_show(smsg);
				connected = conn_connect(
				    &s->conn,
				    s->conn.host,
				    s->conn.port, sw);
				conn_status_close(sw);
			}
		} else {
			DisposeDialog(dlg);

			/* Cancel: destroy session only if we just
			 * created it (need_new_session was false
			 * means we reused an existing one) */
		}

		if (connected) {
			Bookmark *sel_bm = 0L;

			if (g_bm_selected >= 0 &&
			    g_bm_selected < prefs.bookmark_count)
				sel_bm = &prefs.bookmarks[
				    g_bm_selected];

			/* Apply bookmark-specific font */
			if (sel_bm && (sel_bm->font_id != 0 ||
			    sel_bm->font_size != 0)) {
				short win_w, win_h;

				s->font_id = sel_bm->font_id;
				s->font_size = sel_bm->font_size;
				term_ui_set_font(s->window,
				    s->font_id, s->font_size);
				session_save_font(s);
				win_w = LEFT_MARGIN * 2 +
				    TERM_DEFAULT_COLS *
				    g_cell_width;
				win_h = TOP_MARGIN * 2 +
				    TERM_DEFAULT_ROWS *
				    g_cell_height;
				if (win_w > MAX_WIN_WIDTH)
					win_w = MAX_WIN_WIDTH;
				if (win_h > MAX_WIN_HEIGHT)
					win_h = MAX_WIN_HEIGHT;
				do_window_resize(s, win_w, win_h);
			}

			telnet_init(&s->telnet);

			/* Apply terminal type from dialog */
			s->telnet.preferred_ttype =
			    g_connect_ttype;

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
			prefs.username[sizeof(prefs.username) - 1] =
			    '\0';
			prefs_save(&prefs);

			/* Track recently used bookmark */
			if (g_bm_selected >= 0)
				add_recent_bookmark(
				    g_bm_selected);
			if (g_bm_selected >= 0)
				s->bookmark_index = g_bm_selected;

			/* Auto-send username after connect */
			if (s->conn.username[0]) {
				conn_send(&s->conn,
				    s->conn.username,
				    strlen(s->conn.username));
				conn_send(&s->conn, "\r", 1);
			}

			/* Update window title */
			{
				char tmp[270];
				Str255 title;
				short tlen, ti;

				tlen = sprintf(tmp, "Flynn - %s",
				    s->conn.host);
				if (tlen > 254) tlen = 254;
				title[0] = tlen;
				for (ti = 0; ti < tlen; ti++)
					title[ti + 1] = tmp[ti];
				SetWTitle(s->window, title);
			}
		} else if (need_new_session && s &&
		    s->conn.state == CONN_STATE_IDLE) {
			/* Connect failed or no host — destroy the
			 * freshly created session */
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
			StopAlert(128, 0L);
			return;
		}
		SetPort(s->window);
		s->font_id = prefs.font_id;
		s->font_size = prefs.font_size;
		term_ui_set_font(s->window, s->font_id, s->font_size);
		session_save_font(s);
		term_ui_init(s->window, &s->terminal);
		term_ui_save_state(&s->ui);
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
			StopAlert(128, 0L);
			update_menus();
			return;
		}
		SetPort(s->window);
		s->font_id = prefs.font_id;
		s->font_size = prefs.font_size;
		term_ui_set_font(s->window, s->font_id, s->font_size);
		session_save_font(s);
		term_ui_init(s->window, &s->terminal);
		term_ui_save_state(&s->ui);
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

	/* Apply bookmark-specific font before connect (affects grid size) */
	if (bm->font_id != 0 || bm->font_size != 0) {
		short win_w, win_h;

		s->font_id = bm->font_id;
		s->font_size = bm->font_size;
		term_ui_set_font(s->window, s->font_id,
		    s->font_size);
		session_save_font(s);
		win_w = LEFT_MARGIN * 2 +
		    TERM_DEFAULT_COLS * g_cell_width;
		win_h = TOP_MARGIN * 2 +
		    TERM_DEFAULT_ROWS * g_cell_height;
		if (win_w > MAX_WIN_WIDTH) win_w = MAX_WIN_WIDTH;
		if (win_h > MAX_WIN_HEIGHT) win_h = MAX_WIN_HEIGHT;
		do_window_resize(s, win_w, win_h);
	}

	{
		WindowPtr sw;
		char smsg[80];
		Boolean ok;

		sprintf(smsg, "Resolving %.40s\311", bm->host);
		sw = conn_status_show(smsg);
		ok = conn_connect(&s->conn, bm->host, bm->port, sw);
		conn_status_close(sw);

		if (!ok) {
			update_menus();
			return;
		}
	}
	{
		telnet_init(&s->telnet);

		/* Apply bookmark terminal type, fall back to global */
		if (bm->terminal_type >= 0)
			s->telnet.preferred_ttype =
			    bm->terminal_type;
		else
			s->telnet.preferred_ttype =
			    prefs.terminal_type;

		s->telnet.cols = s->terminal.active_cols;
		s->telnet.rows = s->terminal.active_rows;
		terminal_reset(&s->terminal);

		strncpy(prefs.host, s->conn.host,
		    sizeof(prefs.host) - 1);
		prefs.host[sizeof(prefs.host) - 1] = '\0';
		prefs.port = s->conn.port;
		add_recent_bookmark(index);
		s->bookmark_index = index;

		/* Auto-send username from bookmark */
		if (bm->username[0]) {
			conn_send(&s->conn, bm->username,
			    strlen(bm->username));
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

static void
bme_set_btn_title(DialogPtr dlg, short item, const char *text)
{
	short item_type;
	Handle item_h;
	Rect item_rect;
	Str255 pstr;
	short len, i;

	GetDialogItem(dlg, item, &item_type, &item_h, &item_rect);
	len = strlen(text);
	if (len > 254) len = 254;
	pstr[0] = len;
	for (i = 0; i < len; i++)
		pstr[i + 1] = text[i];
	SetControlTitle((ControlHandle)item_h, pstr);
}

/* Simple dialog filter for Return=OK, Cmd+.=Cancel */
static pascal Boolean
std_dlg_filter(DialogPtr dlg, EventRecord *evt, short *item)
{
	(void)dlg;
	if (evt->what == keyDown) {
		char key = evt->message & charCodeMask;
		if (key == '\r' || key == '\n' || key == 0x03) {
			*item = 1;  /* OK button */
			return true;
		}
		if ((evt->modifiers & cmdKey) && key == '.') {
			*item = 2;  /* Cancel button */
			return true;
		}
	}
	return false;
}

/* Bookmark edit dialog state shared with filter proc */
static short g_bme_ttype;
static short g_bme_font_id;
static short g_bme_font_size;

static pascal Boolean
bme_dlg_filter(DialogPtr dlg, EventRecord *evt, short *item)
{
	if (evt->what == keyDown) {
		char key = evt->message & charCodeMask;
		/* Return/Enter = OK */
		if (key == '\r' || key == '\n' || key == 0x03) {
			*item = 1;  /* OK button */
			return true;
		}
		/* Cmd+. = Cancel */
		if ((evt->modifiers & cmdKey) && key == '.') {
			*item = 2;  /* Cancel button */
			return true;
		}
		/* Tab cycles: Name(4)->Host(6)->Port(8)->User(10) */
		if (key == '\t') {
			DialogPeek dp = (DialogPeek)dlg;
			short cur = dp->editField + 1;
			short next;

			if (cur == BME_NAME_FIELD)
				next = BME_HOST_FIELD;
			else if (cur == BME_HOST_FIELD)
				next = BME_PORT_FIELD;
			else if (cur == BME_PORT_FIELD)
				next = BME_USER_FIELD;
			else
				next = BME_NAME_FIELD;
			SelectDialogItemText(dlg, next, 0, 32767);
			*item = next;
			return true;
		}
	}

	if (evt->what == mouseDown) {
		Point pt;
		short item_type;
		Handle item_h;
		Rect item_rect;

		pt = evt->where;
		SetPort(dlg);
		GlobalToLocal(&pt);

		/* Terminal type popup menu */
		GetDialogItem(dlg, BME_TTYPE_BTN,
		    &item_type, &item_h, &item_rect);
		if (PtInRect(pt, &item_rect)) {
			MenuHandle popup;
			Point popup_pt;
			long result;
			short choice;

			popup = NewMenu(202, "\p");
			AppendMenu(popup, "\pDefault");
			AppendMenu(popup, "\pxterm");
			AppendMenu(popup, "\pVT220");
			AppendMenu(popup, "\pVT100");
			AppendMenu(popup, "\pxterm-256color");
			InsertMenu(popup, -1);

			/* Checkmark: -1=Default(1), 0=xterm(2),
			 * 1=VT220(3), 2=VT100(4), 3=xterm-256(5) */
			CheckItem(popup, g_bme_ttype + 2, true);

			popup_pt.h = item_rect.left;
			popup_pt.v = item_rect.top;
			LocalToGlobal(&popup_pt);

			result = PopUpMenuSelect(popup,
			    popup_pt.v, popup_pt.h,
			    g_bme_ttype + 2);
			choice = LoWord(result);

			if (choice > 0) {
				char btn_text[32];

				g_bme_ttype = choice - 2;
				ttype_to_str(g_bme_ttype,
				    btn_text, sizeof(btn_text));
				bme_set_btn_title(dlg,
				    BME_TTYPE_BTN, btn_text);
			}

			DeleteMenu(202);
			DisposeMenu(popup);

			*item = BME_TTYPE_BTN;
			return true;
		}

		/* Font popup menu */
		GetDialogItem(dlg, BME_FONT_BTN,
		    &item_type, &item_h, &item_rect);
		if (PtInRect(pt, &item_rect)) {
			MenuHandle popup;
			Point popup_pt;
			long result;
			short choice, fi;
			short cur_item = 1;  /* Default */

			popup = NewMenu(203, "\p");
			AppendMenu(popup, "\pDefault");
			for (fi = 0; fi < NUM_FONT_PRESETS; fi++) {
				Str255 ps;
				short len;

				len = strlen(font_presets[fi].name);
				ps[0] = len;
				memcpy(ps + 1,
				    font_presets[fi].name, len);
				AppendMenu(popup, "\p ");
				SetMenuItemText(popup,
				    fi + 2, ps);
			}
			InsertMenu(popup, -1);

			/* Find current selection for checkmark */
			if (g_bme_font_id == 0 &&
			    g_bme_font_size == 0) {
				cur_item = 1;  /* Default */
			} else {
				for (fi = 0;
				    fi < NUM_FONT_PRESETS;
				    fi++) {
					if (font_presets[fi].font_id
					    == g_bme_font_id &&
					    font_presets[fi].font_size
					    == g_bme_font_size) {
						cur_item = fi + 2;
						break;
					}
				}
			}
			CheckItem(popup, cur_item, true);

			popup_pt.h = item_rect.left;
			popup_pt.v = item_rect.top;
			LocalToGlobal(&popup_pt);

			result = PopUpMenuSelect(popup,
			    popup_pt.v, popup_pt.h, cur_item);
			choice = LoWord(result);

			if (choice > 0) {
				char btn_text[32];

				if (choice == 1) {
					g_bme_font_id = 0;
					g_bme_font_size = 0;
				} else {
					g_bme_font_id =
					    font_presets
					    [choice - 2].font_id;
					g_bme_font_size =
					    font_presets
					    [choice - 2].font_size;
				}
				font_to_str(g_bme_font_id,
				    g_bme_font_size, btn_text,
				    sizeof(btn_text));
				bme_set_btn_title(dlg,
				    BME_FONT_BTN, btn_text);
			}

			DeleteMenu(203);
			DisposeMenu(popup);

			*item = BME_FONT_BTN;
			return true;
		}
	}
	return false;
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
	char btn_text[32];

	dlg = GetNewDialog(DLOG_BM_EDIT_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return false;

	/* Initialize shared state for filter proc from bookmark */
	g_bme_ttype = bm->terminal_type;
	g_bme_font_id = bm->font_id;
	g_bme_font_size = bm->font_size;

	/* Pre-fill fields from bookmark struct */
	if (bm->name[0]) {
		GetDialogItem(dlg, BME_NAME_FIELD, &item_type,
		    &item_h, &item_rect);
		str[0] = strlen(bm->name);
		memcpy(&str[1], bm->name, str[0]);
		SetDialogItemText(item_h, str);
	}

	if (bm->host[0]) {
		GetDialogItem(dlg, BME_HOST_FIELD, &item_type,
		    &item_h, &item_rect);
		str[0] = strlen(bm->host);
		memcpy(&str[1], bm->host, str[0]);
		SetDialogItemText(item_h, str);
	}

	if (bm->port > 0) {
		GetDialogItem(dlg, BME_PORT_FIELD, &item_type,
		    &item_h, &item_rect);
		sprintf((char *)&str[1], "%u", bm->port);
		str[0] = strlen((char *)&str[1]);
		SetDialogItemText(item_h, str);
	}

	if (bm->username[0]) {
		GetDialogItem(dlg, BME_USER_FIELD, &item_type,
		    &item_h, &item_rect);
		str[0] = strlen(bm->username);
		memcpy(&str[1], bm->username, str[0]);
		SetDialogItemText(item_h, str);
	}

	/* Set terminal type button text */
	ttype_to_str(g_bme_ttype, btn_text, sizeof(btn_text));
	bme_set_btn_title(dlg, BME_TTYPE_BTN, btn_text);

	/* Set font button text */
	font_to_str(g_bme_font_id, g_bme_font_size, btn_text,
	    sizeof(btn_text));
	bme_set_btn_title(dlg, BME_FONT_BTN, btn_text);

	/* Register default button outline */
	setup_default_button_outline(dlg, BME_DEFAULT_BTN);

	ShowWindow(dlg);

	for (;;) {
		ModalDialog(
		    (ModalFilterUPP)bme_dlg_filter,
		    &item_hit);
		if (item_hit == BME_CANCEL) {
			DisposeDialog(dlg);
			return false;
		}
		if (item_hit == BME_OK)
			break;

		/* Terminal type and font handled by filter
		 * proc popup menus */
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

	/* Extract username */
	GetDialogItem(dlg, BME_USER_FIELD, &item_type,
	    &item_h, &item_rect);
	GetDialogItemText(item_h, str);
	if (str[0] > 0 && str[0] < (short)(sizeof(bm->username) - 1)) {
		for (i = 0; i < str[0]; i++)
			bm->username[i] = str[i + 1];
		bm->username[i] = '\0';
	} else {
		bm->username[0] = '\0';
	}

	/* Store terminal type and font from filter proc state */
	bm->terminal_type = g_bme_ttype;
	bm->font_id = g_bme_font_id;
	bm->font_size = g_bme_font_size;

	DisposeDialog(dlg);
	return true;
}

static pascal Boolean
bm_filter(DialogPtr dlg, EventRecord *event, short *item)
{
	Point pt;
	short i;

	/* Cmd+. maps to Done button */
	if (event->what == keyDown) {
		char key = event->message & charCodeMask;
		if ((event->modifiers & cmdKey) && key == '.') {
			*item = BM_DONE;
			return true;
		}
	}

	if (event->what == mouseDown) {
		SetPort(dlg);
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
			/* Redraw list in dialog port */
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

	/* Register default button outline */
	setup_default_button_outline(dlg, BM_DEFAULT_BTN);

	ShowWindow(dlg);

	for (;;) {
		ModalDialog((ModalFilterProcPtr)bm_filter,
		    &item_hit);

		if (item_hit == BM_DONE)
			break;

		/* List click handled by filter — just redraw */
		if (item_hit == BM_LIST)
			continue;

		if (item_hit == BM_ADD) {
			if (prefs.bookmark_count >= MAX_BOOKMARKS) {
				SysBeep(10);
				continue;
			}
			memset(&prefs.bookmarks[prefs.bookmark_count],
			    0, sizeof(Bookmark));
			prefs.bookmarks[prefs.bookmark_count].port = 23;
			prefs.bookmarks[prefs.bookmark_count].terminal_type = -1;
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
			short j, ri, wi, del_idx;

			if (bm_selection < 0 ||
			    bm_selection >= prefs.bookmark_count) {
				SysBeep(10);
				continue;
			}
			del_idx = bm_selection;
			for (j = del_idx;
			    j < prefs.bookmark_count - 1; j++)
				prefs.bookmarks[j] =
				    prefs.bookmarks[j + 1];
			prefs.bookmark_count--;
			if (bm_selection >= prefs.bookmark_count)
				bm_selection = prefs.bookmark_count - 1;

			/* Fix recent indices after delete */
			wi = 0;
			for (ri = 0; ri < prefs.recent_count;
			    ri++) {
				if (prefs.recent[ri] == del_idx)
					continue; /* removed */
				if (prefs.recent[ri] > del_idx)
					prefs.recent[ri]--;
				prefs.recent[wi++] =
				    prefs.recent[ri];
			}
			prefs.recent_count = wi;

			/* Fix bookmark_index in live sessions */
			{
				short si;
				Session *sess;
				for (si = 0; si < MAX_SESSIONS; si++) {
					sess = session_get(si);
					if (!sess)
						continue;
					if (sess->bookmark_index == del_idx)
						sess->bookmark_index = -1;
					else if (sess->bookmark_index >
					    del_idx)
						sess->bookmark_index--;
				}
			}

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
			if (changed)
				prefs_save(&prefs);
			do_connect_bookmark(bm_selection);
			return;
		}
	}

	DisposeDialog(dlg);
	if (changed) {
		prefs_save(&prefs);
		rebuild_file_menu();
	}
}

static void
do_save_as_bookmark(void)
{
	Session *s = active_session;
	Bookmark bm;

	if (!s || prefs.bookmark_count >= MAX_BOOKMARKS)
		return;

	memset(&bm, 0, sizeof(Bookmark));
	strncpy(bm.host, s->conn.host, sizeof(bm.host) - 1);
	bm.port = s->conn.port;
	if (s->conn.username[0])
		strncpy(bm.username, s->conn.username,
		    sizeof(bm.username) - 1);
	bm.font_id = s->font_id;
	bm.font_size = s->font_size;
	if (s->telnet.preferred_ttype >= 0)
		bm.terminal_type = s->telnet.preferred_ttype;
	else
		bm.terminal_type = -1;

	/* Auto-generate name from window title (user@hostname:path).
	 * Extract just user@hostname, stripping :path and beyond.
	 * If title is empty or has no '@', build from connection. */
	bm.name[0] = '\0';
	if (s->terminal.window_title[0]) {
		char *src = s->terminal.window_title;
		short ni = 0;
		Boolean have_at = false;

		while (src[ni] && src[ni] != ':' && src[ni] != ' ' &&
		    ni < (short)(sizeof(bm.name) - 1)) {
			if (src[ni] == '@')
				have_at = true;
			bm.name[ni] = src[ni];
			ni++;
		}
		bm.name[ni] = '\0';

		if (!have_at)
			bm.name[0] = '\0';  /* not user@host, discard */
	}
	if (!bm.name[0]) {
		if (s->conn.username[0])
			sprintf(bm.name, "%.15s@%.15s",
			    s->conn.username, s->conn.host);
		else
			strncpy(bm.name, s->conn.host,
			    sizeof(bm.name) - 1);
	}

	if (bm_edit_dialog(&bm, true)) {
		prefs.bookmarks[prefs.bookmark_count] = bm;
		prefs.bookmark_count++;
		s->bookmark_index = prefs.bookmark_count - 1;
		add_recent_bookmark(s->bookmark_index);
		prefs_save(&prefs);
		rebuild_file_menu();
	}
}

static void
do_font_change(short font_id, short font_size)
{
	short win_w, win_h;

	if (!active_session)
		return;

	if (font_id == active_session->font_id &&
	    font_size == active_session->font_size)
		return;

	/* Update active session only */
	active_session->font_id = font_id;
	active_session->font_size = font_size;
	term_ui_set_font(active_session->window, font_id,
	    font_size);
	session_save_font(active_session);

	/* Compute window size for default grid */
	win_w = LEFT_MARGIN * 2 +
	    TERM_DEFAULT_COLS * g_cell_width;
	win_h = TOP_MARGIN * 2 +
	    TERM_DEFAULT_ROWS * g_cell_height;
	if (win_w > MAX_WIN_WIDTH)
		win_w = MAX_WIN_WIDTH;
	if (win_h > MAX_WIN_HEIGHT)
		win_h = MAX_WIN_HEIGHT;

	do_window_resize(active_session, win_w, win_h);

	/* Auto-save to originating bookmark */
	if (active_session->bookmark_index >= 0 &&
	    active_session->bookmark_index < prefs.bookmark_count) {
		prefs.bookmarks[active_session->bookmark_index].font_id =
		    font_id;
		prefs.bookmarks[active_session->bookmark_index].font_size =
		    font_size;
	}

	/* Also update global default for new sessions */
	prefs.font_id = font_id;
	prefs.font_size = font_size;
	prefs_save(&prefs);

	update_prefs_menu();
}

static void
do_window_resize(Session *s, short width, short height)
{
	short new_cols, new_rows;
	GrafPtr save;
	short i;

	/* Ensure we use this session's font metrics */
	session_load_font(s);

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
	short fid, fsz, ttype;

	if (!prefs_menu)
		return;

	/* Read from active session if available, else global prefs */
	if (active_session) {
		fid = active_session->font_id;
		fsz = active_session->font_size;
		ttype = active_session->telnet.preferred_ttype;
	} else {
		fid = prefs.font_id;
		fsz = prefs.font_size;
		ttype = prefs.terminal_type;
	}

	CheckItem(prefs_menu, PREFS_FONT9_ID,
	    fid == 4 && fsz == 9);
	CheckItem(prefs_menu, PREFS_FONT12_ID,
	    fid == 4 && fsz == 12);
	CheckItem(prefs_menu, PREFS_FONT_C10,
	    fid == 22 && fsz == 10);
	CheckItem(prefs_menu, PREFS_FONT_CH12,
	    fid == 0 && fsz == 12);
	CheckItem(prefs_menu, PREFS_FONT_G9,
	    fid == 3 && fsz == 9);
	CheckItem(prefs_menu, PREFS_FONT_G10,
	    fid == 3 && fsz == 10);
	CheckItem(prefs_menu, PREFS_XTERM_ID,
	    ttype == 0);
	CheckItem(prefs_menu, PREFS_VT220_ID,
	    ttype == 1);
	CheckItem(prefs_menu, PREFS_VT100_ID,
	    ttype == 2);
	CheckItem(prefs_menu, PREFS_XTERM256_ID,
	    ttype == 3);
	CheckItem(prefs_menu, PREFS_DARK_ID,
	    prefs.dark_mode != 0);
}

static void
rebuild_file_menu(void)
{
	short count, i;
	Str255 item_str;
	short nlen, ni;
	const char *name;

	if (!file_menu)
		return;

	/* Remove all dynamic items (after Bookmarks...) */
	count = CountMItems(file_menu);
	while (count > FILE_MENU_STATIC_ITEMS) {
		DeleteMenuItem(file_menu, count);
		count--;
	}

	/* Add indented recent bookmarks under Bookmarks */
	if (prefs.recent_count > 0) {
		for (i = 0; i < prefs.recent_count; i++) {
			short bm_idx = prefs.recent[i];

			if (bm_idx < 0 ||
			    bm_idx >= prefs.bookmark_count)
				continue;

			name = prefs.bookmarks[bm_idx].name;
			nlen = strlen(name);
			if (nlen > 252) nlen = 252;
			/* Indent with 2 spaces */
			item_str[0] = nlen + 2;
			item_str[1] = ' ';
			item_str[2] = ' ';
			for (ni = 0; ni < nlen; ni++)
				item_str[ni + 3] = name[ni];
			AppendMenu(file_menu, "\p ");
			SetMenuItemText(file_menu,
			    CountMItems(file_menu), item_str);
		}
	}

	/* Separator + Quit */
	AppendMenu(file_menu, "\p(-");
	AppendMenu(file_menu, "\pQuit/Q");
}

static void
add_recent_bookmark(short index)
{
	short i, pos;

	if (index < 0 || index >= prefs.bookmark_count)
		return;

	/* Check if already in recent list */
	pos = -1;
	for (i = 0; i < prefs.recent_count; i++) {
		if (prefs.recent[i] == index) {
			pos = i;
			break;
		}
	}

	if (pos == 0)
		return;	/* Already at front */

	/* Remove from current position if found */
	if (pos > 0) {
		for (i = pos; i > 0; i--)
			prefs.recent[i] = prefs.recent[i - 1];
	} else {
		/* Not found — shift everything right */
		short limit = prefs.recent_count;

		if (limit >= MAX_RECENT)
			limit = MAX_RECENT - 1;
		for (i = limit; i > 0; i--)
			prefs.recent[i] = prefs.recent[i - 1];
		if (prefs.recent_count < MAX_RECENT)
			prefs.recent_count++;
	}

	prefs.recent[0] = index;
	prefs_save(&prefs);
	rebuild_file_menu();
}

static pascal Boolean
dns_dlg_filter(DialogPtr dlg, EventRecord *evt, short *item)
{
	if (evt->what == keyDown) {
		char key = evt->message & charCodeMask;
		/* Return/Enter = OK */
		if (key == '\r' || key == '\n' || key == 0x03) {
			*item = 1;  /* OK button */
			return true;
		}
		/* Cmd+. = Cancel */
		if ((evt->modifiers & cmdKey) && key == '.') {
			*item = 2;  /* Cancel button */
			return true;
		}
		/* Only one edit field (item 4) — keep Tab on it */
		if (key == '\t') {
			SelectDialogItemText(dlg, 4, 0, 32767);
			*item = 4;
			return true;
		}
	}
	return false;
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

	/* Register default button outline */
	setup_default_button_outline(dlg, 6);

	ShowWindow(dlg);

	for (;;) {
		ModalDialog(
		    (ModalFilterUPP)dns_dlg_filter,
		    &item_hit);

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
		StopAlert(128, 0L);
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
