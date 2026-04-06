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
#include "favorites.h"
#include "finger.h"
#include "color.h"
#include "logging.h"
#include "printing.h"
#include "theme.h"

/* Menu handles (private to this module) */
static MenuHandle apple_menu, file_menu, edit_menu, prefs_menu, ctrl_menu;
static MenuHandle window_menu;
static MenuHandle font_submenu, size_submenu, ttype_submenu;
#ifdef FLYNN_THEMES
static MenuHandle theme_submenu;
#endif

/* External references to main.c globals */
extern FlynnPrefs prefs;
extern Session *active_session;
extern Boolean running;

/* Map menu item (1-5) to internal ttype index (0-4) */
static const short ttype_from_menu[] = { 0, 3, 2, 1, 4 };
/* Map internal ttype index (0-4) to menu item (1-5) */
static const short ttype_to_menu[] = { 1, 4, 3, 2, 5 };

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
	if (font_submenu) {
		InsertMenu(font_submenu, -1);
		/* Append System 7 fonts at runtime */
		if (g_has_color_qd) {
			AppendMenu(font_submenu, "\p ");
			SetMenuItemText(font_submenu,
			    FONT_HELVETICA_ID, "\pHelvetica");
			AppendMenu(font_submenu, "\p ");
			SetMenuItemText(font_submenu,
			    FONT_TIMES_ID, "\pTimes");
			AppendMenu(font_submenu, "\p ");
			SetMenuItemText(font_submenu,
			    FONT_PALATINO_ID, "\pPalatino");
		}
	}
	size_submenu = GetMenu(SIZE_MENU_ID);
	if (size_submenu)
		InsertMenu(size_submenu, -1);
	ttype_submenu = GetMenu(TTYPE_MENU_ID);
	if (ttype_submenu)
		InsertMenu(ttype_submenu, -1);
	/* Load and insert Theme hierarchical submenu (color only) */
#ifdef FLYNN_THEMES
	if (g_has_color_qd) {
		theme_submenu = GetMenu(THEME_MENU_ID);
		if (theme_submenu)
			InsertMenu(theme_submenu, -1);
	}
#endif

	/* Set hierarchical menu markers (0x1B = hMenuCmd) */
	if (prefs_menu) {
		SetItemCmd(prefs_menu, PREFS_FONT_HIER, 0x1B);
		SetItemMark(prefs_menu, PREFS_FONT_HIER,
		    FONT_MENU_ID);
		SetItemCmd(prefs_menu, PREFS_SIZE_HIER, 0x1B);
		SetItemMark(prefs_menu, PREFS_SIZE_HIER,
		    SIZE_MENU_ID);
		SetItemCmd(prefs_menu, PREFS_TTYPE_HIER, 0x1B);
		SetItemMark(prefs_menu, PREFS_TTYPE_HIER,
		    TTYPE_MENU_ID);
#ifdef FLYNN_THEMES
		/* On color systems: Theme hierarchical submenu.
		 * On mono: plain "Dark Mode On/Off" toggle. */
		if (g_has_color_qd && theme_submenu) {
			SetItemCmd(prefs_menu, PREFS_THEME_HIER,
			    0x1B);
			SetItemMark(prefs_menu, PREFS_THEME_HIER,
			    THEME_MENU_ID);
		} else {
			SetMenuItemText(prefs_menu, PREFS_THEME_HIER,
			    prefs.dark_mode ?
			    "\pTurn Dark Mode Off" : "\pTurn Dark Mode On");
		}
#endif
	}

#ifdef FLYNN_THEMES
	/* Set initial theme checkmark (color systems only) */
	if (g_has_color_qd && theme_submenu) {
		short t, check_item;

		for (t = THEME_ITEM_FIRST; t <= THEME_ITEM_LAST; t++)
			CheckItem(theme_submenu, t, false);

		/* Map theme_id to menu item (separator at item 3
		 * offsets color themes by +2) */
		if (prefs.theme_id < THEME_COUNT_MONO)
			check_item = prefs.theme_id + 1;
		else
			check_item = prefs.theme_id + 2;
		if (check_item >= THEME_ITEM_FIRST &&
		    check_item <= THEME_ITEM_LAST)
			CheckItem(theme_submenu, check_item, true);
	}
#endif
#ifndef FLYNN_FAVORITES
	/* Hide Favorites menu when feature is disabled */
	DeleteMenu(FAVORITES_MENU_ID);
#endif

	DrawMenuBar();
}

void
update_menus(void)
{
	Boolean connected;

	connected = (active_session &&
	    active_session->conn.state == CONN_STATE_CONNECTED);

	/* File menu: New Session always enabled,
	 * Finger and Close when session exists */
	EnableItem(file_menu, FILE_MENU_CONNECT_ID);
#ifdef FLYNN_FINGER
	EnableItem(file_menu, FILE_MENU_FINGER_ID);
#else
	DisableItem(file_menu, FILE_MENU_FINGER_ID);
#endif
	if (active_session)
		EnableItem(file_menu, FILE_MENU_DISCONNECT_ID);
	else
		DisableItem(file_menu, FILE_MENU_DISCONNECT_ID);

#ifdef FLYNN_SAVEFILE
	/* Save Session: enable when session exists */
	if (active_session)
		EnableItem(file_menu, FILE_MENU_SAVE_ID);
	else
		DisableItem(file_menu, FILE_MENU_SAVE_ID);
#else
	DisableItem(file_menu, FILE_MENU_SAVE_ID);
#endif

#ifdef FLYNN_LOGGING
	/* Logging: enable when session exists, toggle text */
	if (active_session) {
		EnableItem(file_menu, FILE_MENU_LOG_ID);
		SetMenuItemText(file_menu, FILE_MENU_LOG_ID,
		    active_session->log_refnum ?
		    "\pStop Logging" :
		    "\pStart Logging\311");
	} else {
		DisableItem(file_menu, FILE_MENU_LOG_ID);
		SetMenuItemText(file_menu, FILE_MENU_LOG_ID,
		    "\pStart Logging\311");
	}
#else
	DisableItem(file_menu, FILE_MENU_LOG_ID);
#endif

#ifdef FLYNN_PRINTING
	/* Page Setup: always enabled. Print: when session exists. */
	EnableItem(file_menu, FILE_MENU_PAGESETUP_ID);
	if (active_session)
		EnableItem(file_menu, FILE_MENU_PRINT_ID);
	else
		DisableItem(file_menu, FILE_MENU_PRINT_ID);
#else
	DisableItem(file_menu, FILE_MENU_PAGESETUP_ID);
	DisableItem(file_menu, FILE_MENU_PRINT_ID);
#endif

#ifdef FLYNN_FAVORITES
	/* Favorites menu: Add Favorite enable/disable.
	 * Allow saving when host is set (not just connected) --
	 * finger sessions disconnect after response but should
	 * still be saveable as favorites. */
	{
		MenuHandle fav_menu;
		fav_menu = GetMenuHandle(FAVORITES_MENU_ID);
		if (fav_menu) {
			if (active_session &&
			    active_session->conn.host[0] &&
			    active_session->bookmark_index < 0 &&
			    prefs.bookmark_count < MAX_BOOKMARKS)
				EnableItem(fav_menu, FAV_ADD_ID);
			else
				DisableItem(fav_menu, FAV_ADD_ID);
		}
	}
#endif

#ifdef FLYNN_CLIPBOARD
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
#else
	DisableItem(edit_menu, EDIT_MENU_COPY_ID);
	DisableItem(edit_menu, EDIT_MENU_PASTE_ID);
	DisableItem(edit_menu, EDIT_MENU_SELALL_ID);
#endif

	/* File menu: Reconnect when disconnected but host is set */
	if (active_session &&
	    active_session->conn.state != CONN_STATE_CONNECTED &&
	    active_session->conn.host[0])
		EnableItem(file_menu, FILE_MENU_RECONNECT_ID);
	else
		DisableItem(file_menu, FILE_MENU_RECONNECT_ID);

	/* Edit menu: Find when session exists, Find Again when last
	 * search text exists, Clear Scrollback when scrollback > 0 */
	if (active_session)
		EnableItem(edit_menu, EDIT_MENU_FIND_ID);
	else
		DisableItem(edit_menu, EDIT_MENU_FIND_ID);
	if (active_session && find_has_last_search())
		EnableItem(edit_menu, EDIT_MENU_FINDAGAIN_ID);
	else
		DisableItem(edit_menu, EDIT_MENU_FINDAGAIN_ID);
#if FLYNN_SCROLLBACK_LINES > 0
	if (active_session &&
	    active_session->terminal.sb_count > 0)
		EnableItem(edit_menu, EDIT_MENU_CLRSCROLL_ID);
	else
		DisableItem(edit_menu, EDIT_MENU_CLRSCROLL_ID);
#else
	DisableItem(edit_menu, EDIT_MENU_CLRSCROLL_ID);
#endif

	/* Control menu: enable only when connected to telnet
	 * (not applicable to finger sessions) */
	if (ctrl_menu) {
		short ci;
		Boolean ctrl_enabled;

#ifdef FLYNN_FINGER
		ctrl_enabled = connected &&
		    active_session->conn.protocol != PROTO_FINGER;
#else
		ctrl_enabled = connected;
#endif
		for (ci = CTRL_MENU_CTRLC; ci <= CTRL_MENU_ESC;
		    ci++) {
			if (ctrl_enabled)
				EnableItem(ctrl_menu, ci);
			else
				DisableItem(ctrl_menu, ci);
		}
		/* Reset Terminal: enabled when session exists */
		if (active_session)
			EnableItem(ctrl_menu, CTRL_MENU_RESET);
		else
			DisableItem(ctrl_menu, CTRL_MENU_RESET);
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
		short fi, fc;

		fc = CountMItems(font_submenu);
		for (fi = 1; fi <= fc; fi++)
			CheckItem(font_submenu, fi, false);

		if (fid == 4)
			CheckItem(font_submenu, FONT_MONACO_ID, true);
		else if (fid == 3)
			CheckItem(font_submenu, FONT_GENEVA_ID, true);
		else if (fid == 0)
			CheckItem(font_submenu, FONT_CHICAGO_ID, true);
		else if (fid == 22)
			CheckItem(font_submenu, FONT_COURIER_ID, true);
		else if (fid == 2)
			CheckItem(font_submenu, FONT_NEWYORK_ID, true);
		else if (fid == 21 && g_has_color_qd)
			CheckItem(font_submenu, FONT_HELVETICA_ID, true);
		else if (fid == 20 && g_has_color_qd)
			CheckItem(font_submenu, FONT_TIMES_ID, true);
		else if (fid == 16 && g_has_color_qd)
			CheckItem(font_submenu, FONT_PALATINO_ID, true);
	}

	/* Size submenu checkmarks */
	if (size_submenu) {
		CheckItem(size_submenu, SIZE_9_ID, fsz == 9);
		CheckItem(size_submenu, SIZE_10_ID, fsz == 10);
		CheckItem(size_submenu, SIZE_12_ID, fsz == 12);
		CheckItem(size_submenu, SIZE_14_ID, fsz == 14);
	}

	/* Terminal Type submenu checkmarks */
	if (ttype_submenu) {
		short i;

		for (i = 0; i < 5; i++)
			CheckItem(ttype_submenu, i + 1,
			    ttype == ttype_from_menu[i]);
	}

	/* Theme: submenu checkmarks on color, toggle text on mono */
#ifdef FLYNN_THEMES
	if (g_has_color_qd && theme_submenu) {
		short t, check_item, cur;

		cur = active_session ?
		    active_session->theme_id : theme_get();
		for (t = THEME_ITEM_FIRST; t <= THEME_ITEM_LAST; t++)
			CheckItem(theme_submenu, t, false);
		if (cur < THEME_COUNT_MONO)
			check_item = cur + 1;
		else
			check_item = cur + 2;
		if (check_item >= THEME_ITEM_FIRST &&
		    check_item <= THEME_ITEM_LAST)
			CheckItem(theme_submenu, check_item, true);
	} else {
		SetMenuItemText(prefs_menu, PREFS_THEME_HIER,
		    theme_is_dark() ?
		    "\pTurn Dark Mode Off" : "\pTurn Dark Mode On");
	}
#elif defined(FLYNN_DARK_MODE)
	SetMenuItemText(prefs_menu, PREFS_DARK_ID,
	    prefs.dark_mode ? "\pTurn Dark Mode Off" : "\pTurn Dark Mode On");
#else
	DisableItem(prefs_menu, PREFS_DARK_ID);
#endif
	{
		unsigned char bs_val, echo_val;

		bs_val = active_session ?
		    active_session->backspace_bs :
		    prefs.backspace_bs;
		echo_val = active_session ?
		    active_session->local_echo :
		    prefs.local_echo;
		SetMenuItemText(prefs_menu, PREFS_BKSP_DEL_ID,
		    bs_val ?
		    "\pSwitch Backspace to DEL (^?)" :
		    "\pSwitch Backspace to BS (^H)");
		SetMenuItemText(prefs_menu, PREFS_LOCAL_ECHO_ID,
		    echo_val ?
		    "\pTurn Local Echo Off" :
		    "\pTurn Local Echo On");
	}
#ifdef FLYNN_STATUS_BAR
	SetMenuItemText(prefs_menu, PREFS_STATUS_BAR_ID,
	    prefs.show_status_bar ?
	    "\pHide Status Bar" : "\pShow Status Bar");
#else
	DisableItem(prefs_menu, PREFS_STATUS_BAR_ID);
#endif
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
	case FILE_MENU_FINGER_ID:
		do_finger();
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
			session_destroy_and_fixup(active_session);
			update_menus();
		}
		break;
	case FILE_MENU_RECONNECT_ID:
		do_reconnect();
		break;
	case FILE_MENU_SAVE_ID:
		do_save_session();
		break;
	case FILE_MENU_LOG_ID:
#ifdef FLYNN_LOGGING
		if (active_session && active_session->log_refnum)
			do_stop_logging();
		else
			do_start_logging();
#endif
		break;
	case FILE_MENU_PAGESETUP_ID:
		do_page_setup();
		break;
	case FILE_MENU_PRINT_ID:
		do_print();
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
		session_destroy_all();
		active_session = 0L;
		running = false;
		break;
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
		case EDIT_MENU_FIND_ID:
			do_find();
			break;
		case EDIT_MENU_FINDAGAIN_ID:
			do_find_again();
			break;
		case EDIT_MENU_CLRSCROLL_ID:
			do_clear_scrollback();
			break;
		case EDIT_MENU_SHOW_CLIP_ID:
			if (clipboard_window_ptr())
				clipboard_window_close();
			else
				do_show_clipboard();
			break;
		}
	}
}

static void
handle_ctrl_menu(short item)
{
	/* Lookup table: menu item -> control byte */
	static const struct { short item; char byte; } ctrl_table[] = {
		{ CTRL_MENU_CTRLC, 0x03 },
		{ CTRL_MENU_CTRLD, 0x04 },
		{ CTRL_MENU_CTRLH, 0x08 },
		{ CTRL_MENU_CTRLL, 0x0C },
		{ CTRL_MENU_CTRLX, 0x18 },
		{ CTRL_MENU_CTRLZ, 0x1A },
		{ CTRL_MENU_ESC,   0x1B }
	};
	short i;

	if (!active_session)
		return;

	/* Reset Terminal: works even when disconnected */
	if (item == CTRL_MENU_RESET) {
		GrafPtr save;
		Session *s = active_session;

		terminal_reset(&s->terminal);
		term_dirty_all(&s->terminal);
		GetPort(&save);
		SetPort(s->window);
		term_ui_invalidate_offscreen();
		term_ui_draw(s->window, &s->terminal);
		if (prefs.show_status_bar)
			draw_status_bar(s->window, s);
		SetPort(save);
		return;
	}

	if (active_session->conn.state != CONN_STATE_CONNECTED)
		return;

	/* Special case: Break sends IAC BRK */
	if (item == CTRL_MENU_BREAK) {
		char brk_seq[2];

		brk_seq[0] = (char)0xFF;  /* IAC */
		brk_seq[1] = (char)0xF3;  /* BRK */
		conn_send(&active_session->conn, brk_seq, 2);
		return;
	}

	/* Table lookup for single-byte control codes */
	for (i = 0; i < (short)(sizeof(ctrl_table) /
	    sizeof(ctrl_table[0])); i++) {
		if (ctrl_table[i].item == item) {
			char ctrl_byte = ctrl_table[i].byte;

			conn_send(&active_session->conn,
			    &ctrl_byte, 1);
			return;
		}
	}
}

static void
handle_font_submenu(short item)
{
	short new_id = -1;

	switch (item) {
	case FONT_MONACO_ID:    new_id = 4; break;   /* Monaco */
	case FONT_GENEVA_ID:    new_id = 3; break;   /* Geneva */
	case FONT_CHICAGO_ID:   new_id = 0; break;   /* Chicago */
	case FONT_COURIER_ID:   new_id = 22; break;  /* Courier */
	case FONT_NEWYORK_ID:   new_id = 2; break;   /* New York */
	case FONT_HELVETICA_ID: new_id = 21; break;  /* Helvetica */
	case FONT_TIMES_ID:     new_id = 20; break;  /* Times */
	case FONT_PALATINO_ID:  new_id = 16; break;  /* Palatino */
	}
	if (new_id >= 0) {
		short sz = active_session ?
		    active_session->font_size : prefs.font_size;
		do_font_change(new_id, sz);
	}
}

static void
handle_size_submenu(short item)
{
	short new_sz = -1;

	switch (item) {
	case SIZE_9_ID:  new_sz = 9; break;
	case SIZE_10_ID: new_sz = 10; break;
	case SIZE_12_ID: new_sz = 12; break;
	case SIZE_14_ID: new_sz = 14; break;
	}
	if (new_sz > 0) {
		short fid = active_session ?
		    active_session->font_id : prefs.font_id;
		do_font_change(fid, new_sz);
	}
}

static void
handle_ttype_submenu(short item)
{
	short ttype = ttype_from_menu[item - 1];

	if (active_session)
		active_session->telnet.preferred_ttype = ttype;
#ifdef FLYNN_FAVORITES
	/* Auto-save to originating bookmark */
	if (active_session &&
	    active_session->bookmark_index >= 0 &&
	    active_session->bookmark_index <
	    prefs.bookmark_count) {
		prefs.bookmarks[
		    active_session->bookmark_index
		    ].terminal_type = ttype;
	}
#endif
	/* Also update global default */
	prefs.terminal_type = ttype;
	/* Sync backspace and local echo to match terminal type */
	prefs.backspace_bs = (ttype == 4) ? 1 : 0;
	prefs.local_echo = (ttype == 4) ? 1 : 0;
	if (active_session) {
		active_session->backspace_bs = prefs.backspace_bs;
		active_session->local_echo = prefs.local_echo;
	}
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

#ifdef FLYNN_THEMES
static void
handle_theme_menu(short item)
{
	short new_theme;
	GrafPtr save;

	/* Map menu item to theme index.
	 * Items 1-2 → index 0-1 (Light, Dark).
	 * Item 3 is separator.
	 * Items 4-10 → index 2-8 (color themes). */
	if (item <= THEME_ITEM_DARK)
		new_theme = item - 1;
	else if (item >= THEME_ITEM_SOLARIZED_LIGHT)
		new_theme = item - 2;
	else
		return;	/* separator */

	if (!active_session)
		return;

	if (new_theme == active_session->theme_id)
		return;	/* no change */

	theme_set(new_theme);
	term_ui_set_dark_mode(theme_is_dark());

	/* Update active session */
	active_session->theme_id = (unsigned char)new_theme;
	active_session->terminal.dark_mode = theme_is_dark();
	active_session->terminal.theme_id = (unsigned char)new_theme;

	/* Update prefs as defaults for new sessions */
	prefs.theme_id = (unsigned char)new_theme;
	prefs.dark_mode = theme_is_dark() ? 1 : 0;
	prefs_save(&prefs);

	/* Update checkmarks */
	if (theme_submenu) {
		short t;
		for (t = THEME_ITEM_FIRST; t <= THEME_ITEM_LAST; t++)
			CheckItem(theme_submenu, t, t == item);
	}

	/* Redraw active window only */
	GetPort(&save);
	SetPort(active_session->window);
#ifdef FLYNN_COLOR
	/* Set port backColor so Window Manager uses theme
	 * bg on subsequent BeginUpdate erase */
	if (g_has_color_qd) {
		if (theme_is_dark()) {
			RGBColor black = {0, 0, 0};
			RGBBackColor(&black);
		} else {
			RGBColor white = {0xFFFF, 0xFFFF, 0xFFFF};
			RGBBackColor(&white);
		}
	}
#endif
	clear_window_bg(active_session->window, theme_is_dark());
	term_ui_invalidate_offscreen();
	session_load_font(active_session);
	term_dirty_all(&active_session->terminal);
	term_ui_draw(active_session->window, &active_session->terminal);
	if (prefs.show_status_bar)
		draw_status_bar(active_session->window, active_session);

	/* Redraw scrollbar column and grow box —
	 * clear_window_bg overwrites them */
	{
		Rect sb_r;
		RgnHandle sc;
#ifdef FLYNN_COLOR
		if (g_has_color_qd) {
			RGBColor blk = {0, 0, 0};
			RGBColor wht =
			    {0xFFFF, 0xFFFF, 0xFFFF};
			RGBForeColor(&blk);
			RGBBackColor(&wht);
		}
#endif
		SetRect(&sb_r,
		    active_session->window->portRect.right -
		    SCROLLBAR_WIDTH, 0,
		    active_session->window->portRect.right,
		    active_session->window->portRect.bottom);
		EraseRect(&sb_r);
		sc = NewRgn();
		GetClip(sc);
		SetRect(&sb_r,
		    active_session->window->portRect.right -
		    SCROLLBAR_WIDTH,
		    active_session->window->portRect.bottom -
		    SCROLLBAR_WIDTH,
		    active_session->window->portRect.right,
		    active_session->window->portRect.bottom);
		ClipRect(&sb_r);
		DrawGrowIcon(active_session->window);
		SetClip(sc);
		DisposeRgn(sc);
#if FLYNN_SCROLLBACK_LINES > 0
		if (active_session->scrollbar)
			Draw1Control(active_session->scrollbar);
#endif
	}
	SetPort(save);

#ifdef FLYNN_FAVORITES
	/* Auto-save to originating bookmark */
	if (active_session->bookmark_index >= 0 &&
	    active_session->bookmark_index <
	    prefs.bookmark_count) {
		prefs.bookmarks[
		    active_session->bookmark_index
		    ].bm_theme_id = new_theme;
		prefs_save(&prefs);
	}
#endif

	update_prefs_menu();
}
#endif

static void
handle_prefs_menu(short item)
{
	switch (item) {
#ifdef FLYNN_THEMES
	case PREFS_THEME_HIER:
		/* Mono: plain toggle between Light/Dark themes */
		if (!g_has_color_qd) {
			short new_id = theme_is_dark() ?
			    THEME_LIGHT : THEME_DARK;
			handle_theme_menu(new_id + 1); /* +1: menu item */
		}
		break;
#elif defined(FLYNN_DARK_MODE)
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
				sess->terminal.dark_mode =
				    prefs.dark_mode;
				term_ui_invalidate_offscreen();
				session_load_font(sess);
				term_dirty_all(&sess->terminal);
				term_ui_draw(sess->window,
				    &sess->terminal);
				if (prefs.show_status_bar)
					draw_status_bar(
					    sess->window, sess);
			}
			SetPort(save);
		}
		break;
#endif
	case PREFS_BKSP_DEL_ID:
		if (active_session)
			active_session->backspace_bs =
			    !active_session->backspace_bs;
		prefs.backspace_bs = active_session ?
		    active_session->backspace_bs :
		    !prefs.backspace_bs;
#ifdef FLYNN_FAVORITES
		if (active_session &&
		    active_session->bookmark_index >= 0 &&
		    active_session->bookmark_index <
		    prefs.bookmark_count) {
			prefs.bookmarks[
			    active_session->bookmark_index
			    ].bm_backspace_bs =
			    active_session->backspace_bs;
		}
#endif
		prefs_save(&prefs);
		update_prefs_menu();
		if (active_session)
			draw_status_bar(active_session->window,
			    active_session);
		break;
	case PREFS_LOCAL_ECHO_ID:
		if (active_session)
			active_session->local_echo =
			    !active_session->local_echo;
		prefs.local_echo = active_session ?
		    active_session->local_echo :
		    !prefs.local_echo;
#ifdef FLYNN_FAVORITES
		if (active_session &&
		    active_session->bookmark_index >= 0 &&
		    active_session->bookmark_index <
		    prefs.bookmark_count) {
			prefs.bookmarks[
			    active_session->bookmark_index
			    ].bm_local_echo =
			    active_session->local_echo;
		}
#endif
		prefs_save(&prefs);
		update_prefs_menu();
		if (active_session)
			draw_status_bar(active_session->window,
			    active_session);
		break;
#ifdef FLYNN_STATUS_BAR
	case PREFS_STATUS_BAR_ID: {
		short si;

		prefs.show_status_bar = !prefs.show_status_bar;
		prefs_save(&prefs);
		update_prefs_menu();
		/* Resize ALL session windows to add/remove
		 * status bar area — not just the active one */
		for (si = 0; si < MAX_SESSIONS; si++) {
			Session *sess = session_get(si);
			short win_w, win_h;
			if (!sess)
				continue;
			session_load_font(sess);
			win_w = LEFT_MARGIN * 2 +
			    sess->terminal.active_cols *
			    g_cell_width + SCROLLBAR_WIDTH;
			win_h = status_bar_height() +
			    sess->terminal.active_rows *
			    g_cell_height;
			do_window_resize(sess, win_w, win_h);
		}
		break;
	}
#endif
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
			if (active_session) {
				term_ui_save_state(
				    &active_session->ui);
				session_save_settings(
				    active_session);
			}
			active_session = ws;
			term_ui_load_state(&ws->ui);
			session_load_font(ws);
			session_load_settings(ws);
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
	case SIZE_MENU_ID:
		handle_size_submenu(item);
		break;
	case TTYPE_MENU_ID:
		handle_ttype_submenu(item);
		break;
#ifdef FLYNN_THEMES
	case THEME_MENU_ID:
		handle_theme_menu(item);
		break;
#endif
#ifdef FLYNN_FAVORITES
	case FAVORITES_MENU_ID:
		favorites_menu_click(item);
		break;
#endif
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
