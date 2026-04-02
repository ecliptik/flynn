/*
 * favorites.c - Favorites menu and dialog management for Flynn
 * Extracted from menus.c and dialogs.c
 */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <Menus.h>
#include <Dialogs.h>
#include <Memory.h>
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
#include "macutil.h"
#include "menus.h"
#include "favorites.h"
#include "finger.h"

#ifdef FLYNN_FAVORITES

/* Number of static items in Favorites menu (Manage + Add) */
#define FAV_STATIC_ITEMS  2

/* External references to main.c globals */
extern FlynnPrefs prefs;
extern Session *active_session;

/* Font preset table (defined in session.c) */
extern FontPreset font_presets[];

/* Bookmark edit dialog state shared with filter proc */
static short g_bme_ttype;
static short g_bme_font_id;
static short g_bme_font_size;
static short g_bme_protocol;
static short g_bme_verbose;

/* List Manager handle for manage dialog */
static ListHandle g_fav_list;

/* ---- Menu management ---- */

void
favorites_rebuild_menu(void)
{
	MenuHandle fav_menu;
	short count, i;
	Str255 item_str;
	short nlen, ni;
	const char *name;

	fav_menu = GetMenuHandle(FAVORITES_MENU_ID);
	if (!fav_menu)
		return;

	/* Remove all dynamic items (after Manage + Add) */
	count = CountMItems(fav_menu);
	while (count > FAV_STATIC_ITEMS) {
		DeleteMenuItem(fav_menu, count);
		count--;
	}

	/* Add separator + all bookmarks */
	if (prefs.bookmark_count > 0) {
		AppendMenu(fav_menu, "\p(-");

		for (i = 0; i < prefs.bookmark_count; i++) {
			name = prefs.bookmarks[i].name;
			nlen = strlen(name);
			if (nlen > 254) nlen = 254;
			item_str[0] = nlen;
			for (ni = 0; ni < nlen; ni++)
				item_str[ni + 1] = name[ni];
			AppendMenu(fav_menu, "\p ");
			SetMenuItemText(fav_menu,
			    CountMItems(fav_menu),
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
		/* Not found -- shift everything right */
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
	favorites_rebuild_menu();
}

void
favorites_menu_click(short item)
{
	switch (item) {
	case FAV_MANAGE_ID:
		favorites_manage();
		break;
	case FAV_ADD_ID:
		favorites_add();
		break;
	default: {
		/* Bookmark entry: item 4+ maps to bookmark index */
		short bm_idx = item - FAV_FIRST_BM;

		if (bm_idx >= 0 &&
		    bm_idx < prefs.bookmark_count) {
			add_recent_bookmark(bm_idx);
#ifdef FLYNN_FINGER
			if (prefs.bookmark_protocol[bm_idx]
			    == PROTO_FINGER)
				do_finger_bookmark(bm_idx);
			else
#endif
				do_connect_bookmark(bm_idx);
		}
		break;
	}
	}
}

/* ---- Bookmark edit dialog ---- */

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
			g_bme_ttype = show_ttype_popup(dlg,
			    BME_TTYPE_BTN, g_bme_ttype,
			    true);
			*item = BME_TTYPE_BTN;
			return true;
		}

		/* Font popup menu */
		GetDialogItem(dlg, BME_FONT_BTN,
		    &item_type, &item_h, &item_rect);
		if (PtInRect(pt, &item_rect)) {
			MenuHandle fpopup;
			Point fpopup_pt;
			long fresult;
			short fchoice, fi;
			short cur_item = 1;  /* Default */

			fpopup = NewMenu(203, "\p");
			AppendMenu(fpopup, "\pDefault");
			for (fi = 0; fi < NUM_FONT_PRESETS; fi++) {
				Str255 ps;
				short len;

				len = strlen(font_presets[fi].name);
				ps[0] = len;
				memcpy(ps + 1,
				    font_presets[fi].name, len);
				AppendMenu(fpopup, "\p ");
				SetMenuItemText(fpopup,
				    fi + 2, ps);
			}
			InsertMenu(fpopup, -1);

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
			CheckItem(fpopup, cur_item, true);

			fpopup_pt.h = item_rect.left;
			fpopup_pt.v = item_rect.top;
			LocalToGlobal(&fpopup_pt);

			fresult = PopUpMenuSelect(fpopup,
			    fpopup_pt.v, fpopup_pt.h, cur_item);
			fchoice = LoWord(fresult);

			if (fchoice > 0) {
				char btn_text[32];

				if (fchoice == 1) {
					g_bme_font_id = 0;
					g_bme_font_size = 0;
				} else {
					g_bme_font_id =
					    font_presets
					    [fchoice - 2].font_id;
					g_bme_font_size =
					    font_presets
					    [fchoice - 2].font_size;
				}
				font_to_str(g_bme_font_id,
				    g_bme_font_size, btn_text,
				    sizeof(btn_text));
				bme_set_btn_title(dlg,
				    BME_FONT_BTN, btn_text);
			}

			DeleteMenu(203);
			DisposeMenu(fpopup);

			*item = BME_FONT_BTN;
			return true;
		}

		/* Protocol popup menu */
		GetDialogItem(dlg, BME_PROTO_BTN,
		    &item_type, &item_h, &item_rect);
		if (PtInRect(pt, &item_rect)) {
			MenuHandle ppopup;
			Point ppopup_pt;
			long presult;
			short pchoice;
			short pcur_item;

			ppopup = NewMenu(204, "\p");
			AppendMenu(ppopup, "\pTelnet");
			AppendMenu(ppopup, "\pFinger");
			InsertMenu(ppopup, -1);

			pcur_item = (g_bme_protocol ==
			    PROTO_FINGER) ? 2 : 1;
			CheckItem(ppopup, pcur_item, true);

			ppopup_pt.h = item_rect.left;
			ppopup_pt.v = item_rect.top;
			LocalToGlobal(&ppopup_pt);

			presult = PopUpMenuSelect(ppopup,
			    ppopup_pt.v, ppopup_pt.h,
			    pcur_item);
			pchoice = LoWord(presult);

			if (pchoice > 0) {
				short it;
				Handle ih;
				Rect ir;

				g_bme_protocol =
				    (pchoice == 2) ?
				    PROTO_FINGER :
				    PROTO_TELNET;
				bme_set_btn_title(dlg,
				    BME_PROTO_BTN,
				    (g_bme_protocol ==
				    PROTO_FINGER) ?
				    "Finger" : "Telnet");
				/* Show/hide verbose checkbox */
				GetDialogItem(dlg,
				    BME_VERBOSE_CHK,
				    &it, &ih, &ir);
				HiliteControl(
				    (ControlHandle)ih,
				    (g_bme_protocol ==
				    PROTO_FINGER) ?
				    0 : 255);
			}

			DeleteMenu(204);
			DisposeMenu(ppopup);

			*item = BME_PROTO_BTN;
			return true;
		}
	}
	return false;
}

static Boolean
bm_edit_dialog(Bookmark *bm, Boolean is_new, short bm_idx)
{
	DialogPtr dlg;
	short item_hit;
	long num;
	char btn_text[32];

	(void)is_new;

	dlg = GetNewDialog(DLOG_FAV_EDIT_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return false;

	/* Initialize shared state for filter proc from bookmark */
	g_bme_ttype = bm->terminal_type;
	g_bme_font_id = bm->font_id;
	g_bme_font_size = bm->font_size;
	g_bme_protocol = (bm_idx >= 0 &&
	    bm_idx < MAX_BOOKMARKS) ?
	    prefs.bookmark_protocol[bm_idx] : 0;

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

	/* Set protocol button text */
	bme_set_btn_title(dlg, BME_PROTO_BTN,
	    (g_bme_protocol == PROTO_FINGER) ?
	    "Finger" : "Telnet");

	/* Verbose checkbox: init from prefs, disable if not finger */
	g_bme_verbose = (bm_idx >= 0 &&
	    bm_idx < MAX_BOOKMARKS) ?
	    prefs.bookmark_verbose[bm_idx] : 0;
	{
		short it;
		Handle ih;
		Rect ir;

		GetDialogItem(dlg, BME_VERBOSE_CHK,
		    &it, &ih, &ir);
		SetControlValue((ControlHandle)ih,
		    g_bme_verbose);
		HiliteControl((ControlHandle)ih,
		    (g_bme_protocol == PROTO_FINGER) ?
		    0 : 255);
	}

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

		if (item_hit == BME_VERBOSE_CHK) {
			short it;
			Handle ih;
			Rect ir;
			short val;

			GetDialogItem(dlg, BME_VERBOSE_CHK,
			    &it, &ih, &ir);
			val = GetControlValue(
			    (ControlHandle)ih);
			SetControlValue(
			    (ControlHandle)ih, !val);
			g_bme_verbose = !val;
		}

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
		dlg_get_text(dlg, BME_PORT_FIELD, port_buf,
		    sizeof(port_buf));
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

	/* Store terminal type, font from filter proc state */
	bm->terminal_type = g_bme_ttype;
	bm->font_id = g_bme_font_id;
	bm->font_size = g_bme_font_size;
	/* Store protocol and verbose in prefs arrays */
	if (bm_idx >= 0 && bm_idx < MAX_BOOKMARKS) {
		prefs.bookmark_protocol[bm_idx] = g_bme_protocol;
		prefs.bookmark_verbose[bm_idx] =
		    g_bme_verbose ? 1 : 0;
	}

	DisposeDialog(dlg);
	return true;
}

/* ---- List Manager helpers ---- */

/*
 * fav_list_draw_proc - UserItem draw proc for the List Manager
 * list. Called by the Dialog Manager on update events.
 */
static pascal void
fav_list_draw_proc(DialogPtr dlg, short item)
{
	Rect frame_r;
	short item_type;
	Handle item_h;

	(void)item;
	GetDialogItem(dlg, BM_LIST, &item_type, &item_h,
	    &frame_r);

	if (g_fav_list) {
		LUpdate(((WindowPtr)dlg)->visRgn,
		    g_fav_list);
	}

	/* Frame around the entire list area */
	FrameRect(&frame_r);
}

/*
 * fav_list_populate - Fill list cells from prefs data.
 * Clears existing rows and adds fresh ones.
 */
static void
fav_list_populate(void)
{
	short i, existing;
	Cell cell;

	if (!g_fav_list)
		return;

	/* Remove all existing rows */
	existing = (**g_fav_list).dataBounds.bottom;
	if (existing > 0)
		LDelRow(existing, 0, g_fav_list);

	/* Add rows for each bookmark */
	if (prefs.bookmark_count > 0) {
		LAddRow(prefs.bookmark_count, 0,
		    g_fav_list);
		for (i = 0; i < prefs.bookmark_count; i++) {
			cell.h = 0;
			cell.v = i;
			LSetCell(prefs.bookmarks[i].name,
			    strlen(prefs.bookmarks[i].name),
			    cell, g_fav_list);
		}
	}
}

/*
 * fav_list_get_selection - Get the selected row index.
 * Returns -1 if nothing is selected.
 */
static short
fav_list_get_selection(void)
{
	Cell cell;

	if (!g_fav_list)
		return -1;

	cell.h = 0;
	cell.v = 0;
	if (LGetSelect(true, &cell, g_fav_list))
		return cell.v;
	return -1;
}

/*
 * fav_list_set_selection - Select a specific row and
 * auto-scroll to make it visible.
 */
static void
fav_list_set_selection(short row)
{
	Cell cell;
	short total;

	if (!g_fav_list)
		return;

	/* Deselect all */
	total = (**g_fav_list).dataBounds.bottom;
	for (cell.v = 0; cell.v < total; cell.v++) {
		cell.h = 0;
		LSetSelect(false, cell, g_fav_list);
	}

	/* Select the requested row */
	if (row >= 0 && row < total) {
		cell.h = 0;
		cell.v = row;
		LSetSelect(true, cell, g_fav_list);
		LAutoScroll(g_fav_list);
	}
}

/* ---- Manage favorites filter proc ---- */

static pascal Boolean
fav_dlg_filter(DialogPtr dlg, EventRecord *event, short *item)
{
	/* Cmd+. maps to Done button */
	if (event->what == keyDown) {
		char key = event->message & charCodeMask;
		if ((event->modifiers & cmdKey) && key == '.') {
			*item = BM_DONE;
			return true;
		}
	}

	if (event->what == mouseDown) {
		Point pt;
		short item_type;
		Handle item_h;
		Rect list_rect;

		SetPort(dlg);
		pt = event->where;
		GlobalToLocal(&pt);

		/* Check if click is in the list area */
		GetDialogItem(dlg, BM_LIST, &item_type,
		    &item_h, &list_rect);
		if (PtInRect(pt, &list_rect) && g_fav_list) {
			Boolean dbl;
			GrafPtr save;

			GetPort(&save);
			SetPort((WindowPtr)dlg);
			dbl = LClick(pt, event->modifiers,
			    g_fav_list);
			SetPort(save);

			if (dbl) {
				*item = BM_CONNECT;
			} else {
				*item = BM_LIST;
			}
			return true;
		}
	}
	return false;
}

/* ---- Fix recent/session indices after reorder ---- */

static void
fix_indices_after_swap(short a, short b)
{
	short ri, si;
	Session *sess;

	/* Fix recent[] indices */
	for (ri = 0; ri < prefs.recent_count; ri++) {
		if (prefs.recent[ri] == a)
			prefs.recent[ri] = b;
		else if (prefs.recent[ri] == b)
			prefs.recent[ri] = a;
	}

	/* Fix live session bookmark_index */
	for (si = 0; si < MAX_SESSIONS; si++) {
		sess = session_get(si);
		if (!sess)
			continue;
		if (sess->bookmark_index == a)
			sess->bookmark_index = b;
		else if (sess->bookmark_index == b)
			sess->bookmark_index = a;
	}
}

/* ---- Manage Favorites dialog ---- */

void
favorites_manage(void)
{
	DialogPtr dlg;
	short item_hit;
	short item_type;
	Handle item_h;
	Rect list_rect, rview;
	Rect data_bounds;
	Point cell_size;
	GrafPtr save;
	Boolean changed = false;
	short selection;

	dlg = GetNewDialog(DLOG_FAVORITES_ID, 0L, (WindowPtr)-1L);
	if (!dlg)
		return;

	/* Register default button outline */
	setup_default_button_outline(dlg, BM_DEFAULT_BTN);

	/* Get list area rect from dialog item */
	GetDialogItem(dlg, BM_LIST, &item_type, &item_h,
	    &list_rect);

	/* Register user item draw proc for list updates */
	SetDialogItem(dlg, BM_LIST, item_type,
	    (Handle)fav_list_draw_proc, &list_rect);

	/* Create List Manager list.
	 * rView excludes scrollbar (15px) and 1px frame. */
	rview = list_rect;
	InsetRect(&rview, 1, 1);
	rview.right -= 15;

	SetRect(&data_bounds, 0, 0, 1,
	    prefs.bookmark_count);
	cell_size.h = rview.right - rview.left;
	cell_size.v = 16;

	GetPort(&save);
	SetPort((WindowPtr)dlg);
	g_fav_list = LNew(&rview, &data_bounds,
	    cell_size, 0, (WindowPtr)dlg,
	    true, false, false, true);
	SetPort(save);

	if (!g_fav_list) {
		DisposeDialog(dlg);
		return;
	}

	/* Only allow single selection */
	(**g_fav_list).selFlags = lOnlyOne;

	/* Populate cells from prefs */
	fav_list_populate();

	ShowWindow(dlg);

	for (;;) {
		ModalDialog((ModalFilterProcPtr)fav_dlg_filter,
		    &item_hit);

		if (item_hit == BM_DONE)
			break;

		/* List click handled by filter */
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
			prefs.bookmark_protocol[prefs.bookmark_count] = 0;
			if (bm_edit_dialog(
			    &prefs.bookmarks[prefs.bookmark_count],
			    true, prefs.bookmark_count)) {
				prefs.bookmark_count++;
				changed = true;
				fav_list_populate();
				fav_list_set_selection(
				    prefs.bookmark_count - 1);
			}
		}

		if (item_hit == BM_EDIT) {
			selection = fav_list_get_selection();
			if (selection < 0 ||
			    selection >= prefs.bookmark_count) {
				SysBeep(10);
				continue;
			}
			if (bm_edit_dialog(
			    &prefs.bookmarks[selection], false,
			    selection)) {
				changed = true;
				fav_list_populate();
				fav_list_set_selection(selection);
			}
		}

		if (item_hit == BM_REMOVE) {
			short j, ri, wi, del_idx;

			selection = fav_list_get_selection();
			if (selection < 0 ||
			    selection >= prefs.bookmark_count) {
				SysBeep(10);
				continue;
			}

			/* Confirmation alert */
			ParamText(
			    "\pRemove this favorite?",
			    "\p", "\p", "\p");
			if (CautionAlert(128, 0L) != 1)
				continue;

			del_idx = selection;
			for (j = del_idx;
			    j < prefs.bookmark_count - 1; j++) {
				prefs.bookmarks[j] =
				    prefs.bookmarks[j + 1];
				prefs.bookmark_protocol[j] =
				    prefs.bookmark_protocol[j + 1];
				prefs.bookmark_verbose[j] =
				    prefs.bookmark_verbose[j + 1];
			}
			prefs.bookmark_count--;

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
				for (si = 0; si < MAX_SESSIONS;
				    si++) {
					sess = session_get(si);
					if (!sess)
						continue;
					if (sess->bookmark_index ==
					    del_idx)
						sess->bookmark_index =
						    -1;
					else if (
					    sess->bookmark_index >
					    del_idx)
						sess->bookmark_index--;
				}
			}

			changed = true;
			fav_list_populate();
			if (selection >= prefs.bookmark_count)
				selection =
				    prefs.bookmark_count - 1;
			if (selection >= 0)
				fav_list_set_selection(selection);
		}

		if (item_hit == BM_MOVE_UP) {
			Bookmark tmp;
			short tmp_proto, tmp_verbose;

			selection = fav_list_get_selection();
			if (selection <= 0 ||
			    selection >= prefs.bookmark_count)
				continue;

			/* Swap bookmarks[sel] with bookmarks[sel-1] */
			tmp = prefs.bookmarks[selection - 1];
			prefs.bookmarks[selection - 1] =
			    prefs.bookmarks[selection];
			prefs.bookmarks[selection] = tmp;

			tmp_proto =
			    prefs.bookmark_protocol[selection - 1];
			prefs.bookmark_protocol[selection - 1] =
			    prefs.bookmark_protocol[selection];
			prefs.bookmark_protocol[selection] =
			    tmp_proto;

			tmp_verbose =
			    prefs.bookmark_verbose[selection - 1];
			prefs.bookmark_verbose[selection - 1] =
			    prefs.bookmark_verbose[selection];
			prefs.bookmark_verbose[selection] =
			    tmp_verbose;

			fix_indices_after_swap(selection,
			    selection - 1);

			selection--;
			changed = true;
			fav_list_populate();
			fav_list_set_selection(selection);
		}

		if (item_hit == BM_MOVE_DOWN) {
			Bookmark tmp;
			short tmp_proto, tmp_verbose;

			selection = fav_list_get_selection();
			if (selection < 0 ||
			    selection >= prefs.bookmark_count - 1)
				continue;

			/* Swap bookmarks[sel] with bookmarks[sel+1] */
			tmp = prefs.bookmarks[selection + 1];
			prefs.bookmarks[selection + 1] =
			    prefs.bookmarks[selection];
			prefs.bookmarks[selection] = tmp;

			tmp_proto =
			    prefs.bookmark_protocol[selection + 1];
			prefs.bookmark_protocol[selection + 1] =
			    prefs.bookmark_protocol[selection];
			prefs.bookmark_protocol[selection] =
			    tmp_proto;

			tmp_verbose =
			    prefs.bookmark_verbose[selection + 1];
			prefs.bookmark_verbose[selection + 1] =
			    prefs.bookmark_verbose[selection];
			prefs.bookmark_verbose[selection] =
			    tmp_verbose;

			fix_indices_after_swap(selection,
			    selection + 1);

			selection++;
			changed = true;
			fav_list_populate();
			fav_list_set_selection(selection);
		}

		if (item_hit == BM_CONNECT) {
			selection = fav_list_get_selection();
			if (selection < 0 ||
			    selection >= prefs.bookmark_count) {
				SysBeep(10);
				continue;
			}
			LDispose(g_fav_list);
			g_fav_list = 0L;
			DisposeDialog(dlg);
			if (changed)
				prefs_save(&prefs);
			favorites_rebuild_menu();
#ifdef FLYNN_FINGER
			if (prefs.bookmark_protocol[selection]
			    == PROTO_FINGER)
				do_finger_bookmark(selection);
			else
#endif
				do_connect_bookmark(selection);
			return;
		}
	}

	LDispose(g_fav_list);
	g_fav_list = 0L;
	DisposeDialog(dlg);
	if (changed) {
		prefs_save(&prefs);
		favorites_rebuild_menu();
	}
}

/* ---- Save as favorite ---- */

void
favorites_add(void)
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

	/* Pre-set protocol and verbose so edit dialog picks them up */
	prefs.bookmark_protocol[prefs.bookmark_count] =
	    s->conn.protocol;
	prefs.bookmark_verbose[prefs.bookmark_count] = 0;

	if (bm_edit_dialog(&bm, true, prefs.bookmark_count)) {
		prefs.bookmarks[prefs.bookmark_count] = bm;
		prefs.bookmark_count++;
		s->bookmark_index = prefs.bookmark_count - 1;
		add_recent_bookmark(s->bookmark_index);
		prefs_save(&prefs);
		favorites_rebuild_menu();
	}
}

#endif /* FLYNN_FAVORITES */
