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
#include "favorites.h"
#include "color.h"
#include "macutil.h"
#include "sysutil.h"
#include "finger.h"
#include "menus.h"

/* External references to main.c globals */
extern FlynnPrefs prefs;
extern Session *active_session;

/* Status window dimensions (centered on 512x342 screen) */
#define STATUS_WIN_W   320
#define STATUS_WIN_H    40

static short g_connect_ttype;     /* terminal type selected in connect dialog */

#ifdef FLYNN_FAVORITES
/* Bookmark popup menu, shared with dialog filter */
static MenuHandle g_bm_popup;
static short g_bm_selected = -1;  /* bookmark index selected from popup */
#endif /* FLYNN_FAVORITES */

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
		Rect clip_r;

		GetPort(&save);
		SetPort(w);
		TextFont(0);   /* Chicago */
		TextSize(12);
		SetRect(&clip_r, 0, 0, STATUS_WIN_W, STATUS_WIN_H);
		ClipRect(&clip_r);
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
	ClipRect(&r);
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

void
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
	setup_default_button_outline(dlg, 8);

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
#ifdef FLYNN_CP437
	s->terminal.cp437_mode = (ttype == 4) ? 1 : 0;
#endif

	/* Save last-used host/port/terminal type to prefs */
	strncpy(prefs.host, s->conn.host,
	    sizeof(prefs.host) - 1);
	prefs.host[sizeof(prefs.host) - 1] = '\0';
	prefs.port = s->conn.port;
	prefs.terminal_type = ttype;
	prefs.backspace_bs = (ttype == 4) ? 1 : 0;
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

#ifdef FLYNN_FAVORITES
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
	    TERM_DEFAULT_COLS * g_cell_width + SCROLLBAR_WIDTH;
	win_h = status_bar_height() +
	    TERM_DEFAULT_ROWS * g_cell_height;
	if (win_w > MAX_WIN_WIDTH) win_w = MAX_WIN_WIDTH;
	if (win_h > MAX_WIN_HEIGHT) win_h = MAX_WIN_HEIGHT;
	do_window_resize(s, win_w, win_h);
}
#endif /* FLYNN_FAVORITES */

/* ---- Terminal type popup helper ---- */

/*
 * show_ttype_popup - Show terminal type popup menu at a dialog item.
 * If include_default is true, adds "Default" as first item (-1).
 * Returns new ttype selection, or current_ttype if no change.
 * Updates button title on selection.
 */
short
show_ttype_popup(DialogPtr dlg, short item_num,
    short current_ttype, Boolean include_default)
{
	MenuHandle popup;
	Point popup_pt;
	long result;
	short choice;
	short item_type;
	Handle item_h;
	Rect item_rect;
	short menu_id = include_default ? 202 : 201;
	static const short t2m[] = { 1, 4, 3, 2, 5 };
	static const short m2t[] = { 0, 3, 2, 1, 4 };
	short offset = include_default ? 1 : 0;
	short cur_item;

	popup = NewMenu(menu_id, "\p");
	if (include_default)
		AppendMenu(popup, "\pDefault");
	AppendMenu(popup, "\pxterm");
	AppendMenu(popup, "\pxterm-256color");
	AppendMenu(popup, "\pVT100");
	AppendMenu(popup, "\pVT220");
	AppendMenu(popup, "\pANSI-BBS");
	InsertMenu(popup, -1);

	if (include_default && current_ttype == -1)
		cur_item = 1;
	else if (current_ttype >= 0 &&
	    current_ttype <= 4)
		cur_item = t2m[current_ttype] + offset;
	else
		cur_item = 1;
	CheckItem(popup, cur_item, true);

	GetDialogItem(dlg, item_num,
	    &item_type, &item_h, &item_rect);
	popup_pt.h = item_rect.left;
	popup_pt.v = item_rect.top;
	LocalToGlobal(&popup_pt);

	result = PopUpMenuSelect(popup,
	    popup_pt.v, popup_pt.h, cur_item);
	choice = LoWord(result);

	DeleteMenu(menu_id);
	DisposeMenu(popup);

	if (choice > 0) {
		char btn_text[32];
		short new_ttype;

		if (include_default && choice == 1)
			new_ttype = -1;
		else
			new_ttype = m2t[choice - 1 - offset];
		ttype_to_str(new_ttype, btn_text,
		    sizeof(btn_text));
		bme_set_btn_title(dlg, item_num,
		    btn_text);
		return new_ttype;
	}
	return current_ttype;
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
			g_connect_ttype = show_ttype_popup(dlg,
			    DLOG_TTYPE_BTN, g_connect_ttype,
			    false);
			*item = DLOG_TTYPE_BTN;
			return true;
		}

#ifdef FLYNN_FAVORITES
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
#endif /* FLYNN_FAVORITES */
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

#ifdef FLYNN_FAVORITES
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
#else
		HideDialogItem(dlg, DLOG_FAVORITES);
#endif

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

#ifdef FLYNN_FAVORITES
		if (g_bm_popup) {
			DeleteMenu(200);
			DisposeMenu(g_bm_popup);
			g_bm_popup = 0L;
		}
#endif

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
				term_ui_ensure_metrics(prefs.font_id,
				    prefs.font_size);

				s = session_new();
				if (!s) {
					show_error_alert("Out of memory");
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
#ifdef FLYNN_FAVORITES
			Bookmark *sel_bm = 0L;

			if (g_bm_selected >= 0 &&
			    g_bm_selected < prefs.bookmark_count)
				sel_bm = &prefs.bookmarks[
				    g_bm_selected];

			apply_bookmark_font(s, sel_bm);
#endif

			/* Save username to prefs (bookmark
			 * path doesn't do this) */
			strncpy(prefs.username,
			    s->conn.username,
			    sizeof(prefs.username) - 1);
			prefs.username[
			    sizeof(prefs.username) - 1] = '\0';

			session_post_connect(s,
			    g_connect_ttype,
#ifdef FLYNN_FAVORITES
			    g_bm_selected,
#else
			    -1,
#endif
			    s->conn.username);
		} else if (need_new_session && s &&
		    s->conn.state == CONN_STATE_IDLE) {
			/* Connect failed or no host — destroy the
			 * freshly created session */
			session_destroy_and_fixup(s);
		}
	}
	update_menus();
}

#ifdef FLYNN_FAVORITES
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
			show_error_alert("Out of memory");
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
			show_error_alert("Maximum sessions reached");
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
			    s->conn.state == CONN_STATE_IDLE)
				session_destroy_and_fixup(s);
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
#endif /* FLYNN_FAVORITES */

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
		show_error_alert("Invalid DNS server IP address");
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
