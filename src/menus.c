/*
 * menus.c - Menu management for Flynn
 * Extracted from main.c
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <Dialogs.h>
#include <Resources.h>
#include <ToolUtils.h>
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
#include "clipboard.h"
#include "savefile.h"
#include "macutil.h"
#include "menus.h"

/* Number of static items in Favorites submenu (Manage + Add) */
#define FAV_STATIC_ITEMS  2

/* Menu handles (private to this module) */
static MenuHandle apple_menu, file_menu, edit_menu, prefs_menu, ctrl_menu;
static MenuHandle window_menu;
static MenuHandle font_submenu, ttype_submenu, favorites_submenu;

/* External references to main.c globals */
extern FlynnPrefs prefs;
extern Session *active_session;
extern Boolean running;

/* External references to session helpers (session.c) */
extern void session_save_font(Session *s);
extern void session_load_font(Session *s);
extern void do_font_change(short font_id, short font_size);
extern void do_window_resize(Session *s, short width, short height);

void
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

	/* Load and insert hierarchical submenus */
	font_submenu = GetMenu(FONT_MENU_ID);
	if (font_submenu)
		InsertMenu(font_submenu, -1);
	ttype_submenu = GetMenu(TTYPE_MENU_ID);
	if (ttype_submenu)
		InsertMenu(ttype_submenu, -1);
	favorites_submenu = GetMenu(FAVORITES_MENU_ID);
	if (favorites_submenu)
		InsertMenu(favorites_submenu, -1);

	/* Set hierarchical menu markers (0x1B = hMenuCmd) */
	if (prefs_menu) {
		SetItemCmd(prefs_menu, PREFS_FONT_HIER, 0x1B);
		SetItemMark(prefs_menu, PREFS_FONT_HIER,
		    FONT_MENU_ID);
		SetItemCmd(prefs_menu, PREFS_TTYPE_HIER, 0x1B);
		SetItemMark(prefs_menu, PREFS_TTYPE_HIER,
		    TTYPE_MENU_ID);
	}
	if (file_menu) {
		SetItemCmd(file_menu, FILE_MENU_FAVORITES_ID,
		    0x1B);
		SetItemMark(file_menu, FILE_MENU_FAVORITES_ID,
		    FAVORITES_MENU_ID);
	}

	DrawMenuBar();
}

void
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

	/* Save Session: enable when session exists */
	if (active_session)
		EnableItem(file_menu, FILE_MENU_SAVE_ID);
	else
		DisableItem(file_menu, FILE_MENU_SAVE_ID);

	/* Favorites submenu: Add Favorite enable/disable */
	if (favorites_submenu) {
		if (active_session &&
		    active_session->conn.state ==
		    CONN_STATE_CONNECTED &&
		    active_session->bookmark_index < 0 &&
		    prefs.bookmark_count < MAX_BOOKMARKS)
			EnableItem(favorites_submenu, FAV_ADD_ID);
		else
			DisableItem(favorites_submenu, FAV_ADD_ID);
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

void
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
	snprintf(count_str, sizeof(count_str), "%d of %d Sessions",
	    sess_count, MAX_SESSIONS);
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

void
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

	/* Font submenu checkmarks */
	if (font_submenu) {
		CheckItem(font_submenu, FONT_MONACO9_ID,
		    fid == 4 && fsz == 9);
		CheckItem(font_submenu, FONT_MONACO12_ID,
		    fid == 4 && fsz == 12);
		CheckItem(font_submenu, FONT_COURIER10_ID,
		    fid == 22 && fsz == 10);
		CheckItem(font_submenu, FONT_CHICAGO12_ID,
		    fid == 0 && fsz == 12);
		CheckItem(font_submenu, FONT_GENEVA9_ID,
		    fid == 3 && fsz == 9);
		CheckItem(font_submenu, FONT_GENEVA10_ID,
		    fid == 3 && fsz == 10);
	}

	/* Terminal Type submenu checkmarks */
	if (ttype_submenu) {
		CheckItem(ttype_submenu, TTYPE_XTERM_ID,
		    ttype == 0);
		CheckItem(ttype_submenu, TTYPE_VT220_ID,
		    ttype == 1);
		CheckItem(ttype_submenu, TTYPE_VT100_ID,
		    ttype == 2);
		CheckItem(ttype_submenu, TTYPE_XTERM256_ID,
		    ttype == 3);
		CheckItem(ttype_submenu, TTYPE_ANSI_ID,
		    ttype == 4);
	}

	/* Options menu checkmarks */
	CheckItem(prefs_menu, PREFS_DARK_ID,
	    prefs.dark_mode != 0);
}

void
rebuild_file_menu(void)
{
	short count, i;
	Str255 item_str;
	short nlen, ni;
	const char *name;

	if (!favorites_submenu)
		return;

	/* Remove all dynamic items (after Manage + Add) */
	count = CountMItems(favorites_submenu);
	while (count > FAV_STATIC_ITEMS) {
		DeleteMenuItem(favorites_submenu, count);
		count--;
	}

	/* Add separator + all bookmarks */
	if (prefs.bookmark_count > 0) {
		AppendMenu(favorites_submenu, "\p(-");

		for (i = 0; i < prefs.bookmark_count; i++) {
			name = prefs.bookmarks[i].name;
			nlen = strlen(name);
			if (nlen > 254) nlen = 254;
			item_str[0] = nlen;
			for (ni = 0; ni < nlen; ni++)
				item_str[ni + 1] = name[ni];
			AppendMenu(favorites_submenu, "\p ");
			SetMenuItemText(favorites_submenu,
			    CountMItems(favorites_submenu),
			    item_str);
		}
	}
}

void
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

static void
handle_apple_menu(short item)
{
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
}

static void
handle_file_menu(short item)
{
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
			    session_from_window(FrontWindow());
			update_menus();
		}
		break;
	case FILE_MENU_SAVE_ID:
		do_save_session();
		break;
	case FILE_MENU_QUIT_ID:
		if (session_any_connected()) {
			ParamText(
			    "\pDisconnect all "
			    "sessions and quit?",
			    "\p", "\p", "\p");
			if (CautionAlert(128, 0L) != 1)
				break;
		}
		{
			short si;
			Session *sess;

			for (si = MAX_SESSIONS - 1;
			    si >= 0; si--) {
				sess = session_get(si);
				if (sess)
					session_destroy(sess);
			}
		}
		active_session = 0L;
		running = false;
		break;
	}
}

static void
handle_favorites_submenu(short item)
{
	switch (item) {
	case FAV_MANAGE_ID:
		do_bookmarks();
		break;
	case FAV_ADD_ID:
		do_save_as_bookmark();
		break;
	default: {
		/* Bookmark entry: item 4+ maps to bookmark index */
		short bm_idx = item - FAV_FIRST_BM;

		if (bm_idx >= 0 &&
		    bm_idx < prefs.bookmark_count) {
			add_recent_bookmark(bm_idx);
			do_connect_bookmark(bm_idx);
		}
		break;
	}
	}
}

static void
handle_edit_menu(short item)
{
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
}

static void
handle_ctrl_menu(short item)
{
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
}

static void
handle_font_submenu(short item)
{
	switch (item) {
	case FONT_MONACO9_ID:
		do_font_change(4, 9);
		break;
	case FONT_MONACO12_ID:
		do_font_change(4, 12);
		break;
	case FONT_COURIER10_ID:
		do_font_change(22, 10);
		break;
	case FONT_CHICAGO12_ID:
		do_font_change(0, 12);
		break;
	case FONT_GENEVA9_ID:
		do_font_change(3, 9);
		break;
	case FONT_GENEVA10_ID:
		do_font_change(3, 10);
		break;
	}
}

static void
handle_ttype_submenu(short item)
{
	short ttype = item - TTYPE_XTERM_ID;

	if (active_session)
		active_session->telnet.preferred_ttype = ttype;
	/* Auto-save to originating bookmark */
	if (active_session &&
	    active_session->bookmark_index >= 0 &&
	    active_session->bookmark_index <
	    prefs.bookmark_count) {
		prefs.bookmarks[
		    active_session->bookmark_index
		    ].terminal_type = ttype;
	}
	/* Also update global default */
	prefs.terminal_type = ttype;
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
}

static void
handle_prefs_menu(short item)
{
	switch (item) {
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
				clear_window_bg(sess->window,
				    prefs.dark_mode);
				term_dirty_all(&sess->terminal);
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
}

static void
handle_window_menu(short item)
{
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
	update_menus();
}

Boolean
handle_menu(long menu_id)
{
	short menu, item;
	Boolean handled = true;

	menu = HiWord(menu_id);
	item = LoWord(menu_id);

	switch (menu) {
	case APPLE_MENU_ID:
		handle_apple_menu(item);
		break;
	case FILE_MENU_ID:
		handle_file_menu(item);
		break;
	case EDIT_MENU_ID:
		handle_edit_menu(item);
		break;
	case CTRL_MENU_ID:
		handle_ctrl_menu(item);
		break;
	case PREFS_MENU_ID:
		handle_prefs_menu(item);
		break;
	case FONT_MENU_ID:
		handle_font_submenu(item);
		break;
	case TTYPE_MENU_ID:
		handle_ttype_submenu(item);
		break;
	case FAVORITES_MENU_ID:
		handle_favorites_submenu(item);
		break;
	case WINDOW_MENU_ID:
		handle_window_menu(item);
		break;
	default:
		handled = false;
		break;
	}

	HiliteMenu(0);
	return handled;
}
