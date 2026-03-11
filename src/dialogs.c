/*
 * dialogs.c - Dialog management for Flynn
 * Extracted from main.c and connection.c
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <Dialogs.h>
#include <Memory.h>
#include <ToolUtils.h>
#include <Resources.h>
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
#include "dialogs.h"
#include "color.h"
#include "macutil.h"
#include "sysutil.h"

/* External references to main.c globals */
extern FlynnPrefs prefs;
extern Session *active_session;

/* Functions now in menus.c */
extern void update_menus(void);
extern void rebuild_file_menu(void);
extern void add_recent_bookmark(short index);

/* Status window dimensions (centered on 512x342 screen) */
#define STATUS_WIN_W   320
#define STATUS_WIN_H    40

/* Bookmark popup menu, shared with dialog filter */
static MenuHandle g_bm_popup;
static short g_bm_selected = -1;  /* bookmark index selected from popup */
static short g_connect_ttype;     /* terminal type selected in connect dialog */

/* Bookmark edit dialog state shared with filter proc */
static short g_bme_ttype;
static short g_bme_font_id;
static short g_bme_font_size;

/* Bookmark manager state */
static short bm_selection = -1;
static FlynnPrefs *bm_prefs_ptr;
static Rect bm_list_rect;

/* ---- Status window UI (moved from connection.c) ---- */

WindowPtr
conn_status_show(const char *msg)
{
	WindowPtr w;
	Rect r;
	Str255 title;
	GrafPtr save;
	Str255 ps;
	short len;

	SetRect(&r,
	    (512 - STATUS_WIN_W) / 2,
	    (342 - STATUS_WIN_H) / 2 + 20,  /* +20 for menu bar */
	    (512 + STATUS_WIN_W) / 2,
	    (342 + STATUS_WIN_H) / 2 + 20);
	title[0] = 0;
	w = NewWindow(0L, &r, title, true, dBoxProc,
	    (WindowPtr)-1L, false, 0L);
	if (w) {
		GetPort(&save);
		SetPort(w);
		TextFont(0);   /* Chicago */
		TextSize(12);
		len = strlen(msg);
		if (len > 255) len = 255;
		ps[0] = len;
		memcpy(ps + 1, msg, len);
		MoveTo(10, 26);
		DrawString(ps);
		SetPort(save);
	}
	return w;
}

void
conn_status_update(WindowPtr w, const char *msg)
{
	GrafPtr save;
	Rect r;
	Str255 ps;
	short len;

	if (!w) return;
	GetPort(&save);
	SetPort(w);
	SetRect(&r, 0, 0, STATUS_WIN_W, STATUS_WIN_H);
	EraseRect(&r);
	len = strlen(msg);
	if (len > 255) len = 255;
	ps[0] = len;
	memcpy(ps + 1, msg, len);
	MoveTo(10, 26);
	DrawString(ps);
	SetPort(save);
}

void
conn_status_close(WindowPtr w)
{
	if (w)
		DisposeWindow(w);
}

/* ---- Default button outline ---- */

/* Draw a 3-pixel rounded rect outline around the default button (item 1) */
pascal void
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
void
setup_default_button_outline(DialogPtr dlg, short outline_item)
{
	short item_type;
	Handle item_h;
	Rect item_rect;

	GetDialogItem(dlg, outline_item, &item_type, &item_h, &item_rect);
	SetDialogItem(dlg, outline_item, userItem,
	    (Handle)draw_default_button, &item_rect);
}

/* ---- Standard dialog filter ---- */

/* Simple dialog filter for Return=OK, Cmd+.=Cancel */
pascal Boolean
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

/* ---- Button title helper ---- */

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

/* ---- About dialog ---- */

void
do_about(void)
{
	DialogPtr dlg;
	short item;
	char machine[32];
	char running_on[64];
	short item_type;
	Handle item_h;
	Rect item_rect;
	Str255 pstr;

	dlg = GetNewDialog(DLOG_ABOUT_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return;

	/* Set machine type in item 4 */
	get_machine_name(machine, sizeof(machine));
	snprintf(running_on, sizeof(running_on), "Running on %s%s",
	    machine, g_has_color_qd ? " (Color)" : "");
	c2pstr(pstr, running_on);
	GetDialogItem(dlg, 4, &item_type, &item_h, &item_rect);
	SetDialogItemText(item_h, pstr);

	/* Register default button outline */
	setup_default_button_outline(dlg, 7);

	ModalDialog((ModalFilterUPP)std_dlg_filter, &item);
	DisposeDialog(dlg);
}

/* ---- Post-connect shared logic ---- */

/*
 * session_post_connect - shared setup after successful TCP connect.
 * Initializes telnet, sets terminal type, resets terminal, saves
 * host/port to prefs, tracks bookmark, auto-sends username, sets title.
 *
 * Caller must save prefs.username separately if needed (do_connect does,
 * do_connect_bookmark does not).
 */
static void
session_post_connect(Session *s, short ttype, short bm_index,
    const char *username)
{
	telnet_init(&s->telnet);
	s->telnet.preferred_ttype = ttype;
	s->telnet.cols = s->terminal.active_cols;
	s->telnet.rows = s->terminal.active_rows;
	terminal_reset(&s->terminal);
	s->terminal.cp437_mode = (ttype == 4) ? 1 : 0;

	/* Save last-used host/port/terminal type to prefs */
	strncpy(prefs.host, s->conn.host,
	    sizeof(prefs.host) - 1);
	prefs.host[sizeof(prefs.host) - 1] = '\0';
	prefs.port = s->conn.port;
	prefs.terminal_type = ttype;
	prefs_save(&prefs);

	/* Track bookmark */
	if (bm_index >= 0) {
		add_recent_bookmark(bm_index);
		s->bookmark_index = bm_index;
	}

	/* Auto-send username */
	if (username && username[0]) {
		conn_send(&s->conn, (char *)username,
		    strlen(username));
		conn_send(&s->conn, "\r", 1);
	}

	/* Update window title */
	set_wtitlef(s->window, "Flynn - %s", s->conn.host);
}

/*
 * apply_bookmark_font - apply bookmark-specific font and resize window.
 * Shared between do_connect (after connect) and do_connect_bookmark
 * (before connect).
 */
static void
apply_bookmark_font(Session *s, Bookmark *bm)
{
	short win_w, win_h;

	if (!bm || (bm->font_id == 0 && bm->font_size == 0))
		return;

	s->font_id = bm->font_id;
	s->font_size = bm->font_size;
	term_ui_set_font(s->window, s->font_id, s->font_size);
	session_save_font(s);
	win_w = LEFT_MARGIN * 2 +
	    TERM_DEFAULT_COLS * g_cell_width;
	win_h = TOP_MARGIN * 2 +
	    TERM_DEFAULT_ROWS * g_cell_height;
	if (win_w > MAX_WIN_WIDTH) win_w = MAX_WIN_WIDTH;
	if (win_h > MAX_WIN_HEIGHT) win_h = MAX_WIN_HEIGHT;
	do_window_resize(s, win_w, win_h);
}

/* ---- Connect dialog ---- */

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
			AppendMenu(popup, "\pANSI-BBS");
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
			GetDialogItem(dlg, DLOG_FAVORITES,
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
					    DLOG_FAVORITES,
					    bm->name);

					/* Fill host */
					dlg_set_text(dlg,
					    DLOG_HOST_FIELD,
					    bm->host);

					/* Fill port */
					{
						char portbuf[8];
						snprintf(portbuf,
						    sizeof(portbuf),
						    "%d", bm->port);
						dlg_set_text(dlg,
						    DLOG_PORT_FIELD,
						    portbuf);
					}

					/* Fill username */
					dlg_set_text(dlg,
					    DLOG_USER_FIELD,
					    bm->username[0] ?
					    bm->username : "");

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

void
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
		if (prefill_host[0])
			dlg_set_text(dlg, DLOG_HOST_FIELD,
			    prefill_host);
		/* Pre-fill port */
		if (prefill_port > 0) {
			char portbuf[8];
			snprintf(portbuf, sizeof(portbuf), "%d",
		    prefill_port);
			dlg_set_text(dlg, DLOG_PORT_FIELD,
			    portbuf);
		}
		/* Pre-fill username */
		if (prefill_user[0])
			dlg_set_text(dlg, DLOG_USER_FIELD,
			    prefill_user);

		/* Hide Favorites button if no favorites saved */
		if (prefs.bookmark_count <= 0) {
			GetDialogItem(dlg, DLOG_FAVORITES,
			    &item_type, &item_h, &item_rect);
			HideDialogItem(dlg, DLOG_FAVORITES);
		}

		/* Build favorites popup menu */
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
			dlg_get_text(dlg, DLOG_HOST_FIELD,
			    dlg_host, sizeof(dlg_host));

			/* Extract port */
			{
				char portbuf[8];
				dlg_get_text(dlg, DLOG_PORT_FIELD,
				    portbuf, sizeof(portbuf));
				if (portbuf[0]) {
					c2pstr(pstr, portbuf);
					StringToNum(pstr, &port_num);
					dlg_port = (short)port_num;
				} else {
					dlg_port = DEFAULT_PORT;
				}
			}

			/* Extract username */
			dlg_get_text(dlg, DLOG_USER_FIELD,
			    dlg_user, sizeof(dlg_user));

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
				session_init_from_prefs(s);
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
					snprintf(smsg, sizeof(smsg),
					    "Connecting to %.50s\311",
					    s->conn.host);
				else
					snprintf(smsg, sizeof(smsg),
					    "Resolving %.50s\311",
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

			apply_bookmark_font(s, sel_bm);

			/* Save username to prefs (bookmark
			 * path doesn't do this) */
			strncpy(prefs.username,
			    s->conn.username,
			    sizeof(prefs.username) - 1);
			prefs.username[
			    sizeof(prefs.username) - 1] = '\0';

			session_post_connect(s,
			    g_connect_ttype,
			    g_bm_selected,
			    s->conn.username);
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

void
do_connect_bookmark(short index)
{
	Bookmark *bm;
	Session *s = active_session;
	Boolean created_session = false;

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
		session_init_from_prefs(s);
		active_session = s;
		created_session = true;
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
		session_init_from_prefs(s);
		SelectWindow(s->window);
		active_session = s;
		created_session = true;
	}

	if (s->conn.state != CONN_STATE_IDLE) {
		update_menus();
		return;
	}

	bm = &prefs.bookmarks[index];

	/* Apply bookmark-specific font before connect (affects grid size) */
	apply_bookmark_font(s, bm);

	{
		WindowPtr sw;
		char smsg[80];
		Boolean ok;
		short ttype;

		snprintf(smsg, sizeof(smsg), "Resolving %.50s\311",
		    bm->host);
		sw = conn_status_show(smsg);
		ok = conn_connect(&s->conn, bm->host, bm->port, sw);
		conn_status_close(sw);

		if (!ok) {
			if (created_session &&
			    s->conn.state == CONN_STATE_IDLE) {
				if (s == active_session)
					active_session = 0L;
				session_destroy(s);
				if (!active_session) {
					WindowPtr front = FrontWindow();
					active_session =
					    session_from_window(front);
				}
			}
			update_menus();
			return;
		}

		/* Determine terminal type: bookmark or global */
		ttype = (bm->terminal_type >= 0) ?
		    bm->terminal_type : prefs.terminal_type;

		session_post_connect(s, ttype, index,
		    bm->username);
	}
	update_menus();
}

/* ---- Bookmark manager ---- */

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
			len = snprintf(line, sizeof(line),
			    "%s - %s:%u",
			    p->bookmarks[i].name,
			    p->bookmarks[i].host,
			    p->bookmarks[i].port);
		else
			len = snprintf(line, sizeof(line),
			    "%s - %s",
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
			AppendMenu(popup, "\pANSI-BBS");
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
	long num;
	char btn_text[32];

	dlg = GetNewDialog(DLOG_FAV_EDIT_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return false;

	/* Initialize shared state for filter proc from bookmark */
	g_bme_ttype = bm->terminal_type;
	g_bme_font_id = bm->font_id;
	g_bme_font_size = bm->font_size;

	/* Pre-fill fields from bookmark struct */
	if (bm->name[0])
		dlg_set_text(dlg, BME_NAME_FIELD, bm->name);
	if (bm->host[0])
		dlg_set_text(dlg, BME_HOST_FIELD, bm->host);
	if (bm->port > 0) {
		char port_buf[8];
		snprintf(port_buf, sizeof(port_buf), "%u", bm->port);
		dlg_set_text(dlg, BME_PORT_FIELD, port_buf);
	}
	if (bm->username[0])
		dlg_set_text(dlg, BME_USER_FIELD, bm->username);

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
	dlg_get_text(dlg, BME_NAME_FIELD, bm->name, 32);
	if (bm->name[0] == '\0') {
		DisposeDialog(dlg);
		return false;
	}

	/* Extract host */
	dlg_get_text(dlg, BME_HOST_FIELD, bm->host, 128);
	if (bm->host[0] == '\0') {
		DisposeDialog(dlg);
		return false;
	}

	/* Extract port */
	{
		char port_buf[8];
		dlg_get_text(dlg, BME_PORT_FIELD, port_buf, sizeof(port_buf));
		if (port_buf[0]) {
			Str255 pstr;
			c2pstr(pstr, port_buf);
			StringToNum(pstr, &num);
			bm->port = (unsigned short)num;
		} else {
			bm->port = 23;
		}
	}

	/* Extract username */
	dlg_get_text(dlg, BME_USER_FIELD, bm->username,
	    sizeof(bm->username));

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

void
do_bookmarks(void)
{
	DialogPtr dlg;
	short item_hit;
	Handle item_h;
	short item_type;
	Rect item_rect;
	Boolean changed = false;

	dlg = GetNewDialog(DLOG_FAVORITES_ID, 0L, (WindowPtr)-1L);
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

/* ---- Save as bookmark ---- */

void
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
			snprintf(bm.name, sizeof(bm.name),
			    "%.15s@%.15s",
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

/* ---- DNS server dialog ---- */

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

void
do_dns_server_dialog(void)
{
	DialogPtr dlg;
	short item_hit;
	char ip_cstr[16];
	unsigned long ip;

	dlg = GetNewDialog(DLOG_DNS_ID, 0L, (WindowPtr)-1L);
	if (!dlg) {
		SysBeep(10);
		return;
	}

	/* Pre-fill with current DNS server */
	dlg_set_text(dlg, 4, prefs.dns_server);

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
	dlg_get_text(dlg, 4, ip_cstr, sizeof(ip_cstr));
	DisposeDialog(dlg);

	if (ip_cstr[0] == '\0')
		return;

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
