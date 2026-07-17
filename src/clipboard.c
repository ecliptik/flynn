/*
 * clipboard.c - Clipboard operations for Flynn
 * Extracted from main.c
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Memory.h>
#include <Multiverse.h>

#include "main.h"
#include "session.h"
#include "connection.h"
#include "terminal.h"
#include "terminal_ui.h"
#include "glyphs.h"
#include "clipboard.h"
#include "savefile.h"
#include "menus.h"

/* External references to main.c globals */
extern Session *active_session;

void
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
		short scroll_delta;

		term_ui_sel_get_range(&sr, &sc, &er, &ec);

		/* Adjust selection rows if user scrolled since
		 * making the selection */
		scroll_delta = s->terminal.scroll_offset -
		    term_ui_sel_scroll_offset();
		sr += scroll_delta;
		er += scroll_delta;

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

				cell = terminal_get_display_cell(
				    &s->terminal, row, col);
				cc = cell_to_char(cell);
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

void
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

		/* Mac scrap separates lines with bare CR; telnet NVT
		 * wants CR LF, so expand each CR.  Output can be up to
		 * 2x the input, so build into a bounded slice buffer and
		 * flush it, rather than sending from the scrap directly. */
		sent = 0;
		while (sent < len) {
			char out[512];
			short oi;

			oi = 0;
			while (sent < len && oi < (short)(sizeof(out) - 1)) {
				char c = p[sent++];

				if (c == '\r') {
					out[oi++] = '\r';
					out[oi++] = '\n';
				} else {
					out[oi++] = c;
				}
			}
			conn_send(&s->conn, out, oi);
		}

		if (s->terminal.bracketed_paste)
			conn_send(&s->conn, "\033[201~", 6);

		HUnlock(h);
	}
	DisposeHandle(h);
}

void
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

	/* Update menus to reflect selection state */
	update_menus();
}

/* ---- Show Clipboard window (adapted from Geomys) ---- */

#define CLIP_WIN_KIND   100
#define CLIP_WIN_W      300
#define CLIP_WIN_H      160
#define CLIP_MIN_W      150
#define CLIP_MIN_H      80
#define CLIP_SB_W       15

static WindowPtr g_clip_window = 0L;
static TEHandle g_clip_te = 0L;
static ControlHandle g_clip_vscroll = 0L;
static ControlHandle g_clip_hscroll = 0L;

static void
clip_calc_rects(Rect *view_r, Rect *dest_r)
{
	*view_r = g_clip_window->portRect;
	view_r->left += 4;
	view_r->top += 4;
	view_r->right -= (CLIP_SB_W + 4);
	view_r->bottom -= (CLIP_SB_W + 4);
	*dest_r = *view_r;
	dest_r->right = dest_r->left + 2000;
}

static pascal void
clip_vscroll_action(ControlHandle ctl, short part)
{
	short delta = 0, old_val, new_val, max_val;
	short page_lines;

	if (!g_clip_te || part == 0)
		return;

	page_lines = ((*g_clip_te)->viewRect.bottom -
	    (*g_clip_te)->viewRect.top) /
	    (*g_clip_te)->lineHeight;
	if (page_lines < 1) page_lines = 1;

	switch (part) {
	case inUpButton:    delta = -1; break;
	case inDownButton:  delta = 1; break;
	case inPageUp:      delta = -(page_lines - 1); break;
	case inPageDown:    delta = page_lines - 1; break;
	}

	old_val = GetControlValue(ctl);
	max_val = GetControlMaximum(ctl);
	new_val = old_val + delta;
	if (new_val < 0) new_val = 0;
	if (new_val > max_val) new_val = max_val;
	SetControlValue(ctl, new_val);

	TEScroll(0, (old_val - new_val) *
	    (*g_clip_te)->lineHeight, g_clip_te);
}

static pascal void
clip_hscroll_action(ControlHandle ctl, short part)
{
	short delta = 0, old_val, new_val, max_val;
	short page_w;

	if (!g_clip_te || part == 0)
		return;

	page_w = (*g_clip_te)->viewRect.right -
	    (*g_clip_te)->viewRect.left;

	switch (part) {
	case inUpButton:    delta = -8; break;
	case inDownButton:  delta = 8; break;
	case inPageUp:      delta = -(page_w - 16); break;
	case inPageDown:    delta = page_w - 16; break;
	}

	old_val = GetControlValue(ctl);
	max_val = GetControlMaximum(ctl);
	new_val = old_val + delta;
	if (new_val < 0) new_val = 0;
	if (new_val > max_val) new_val = max_val;
	SetControlValue(ctl, new_val);

	TEScroll(old_val - new_val, 0, g_clip_te);
}

static void
clip_update_scroll(void)
{
	short n_lines, vis_lines, max_v;
	short max_w, vis_w, max_h;

	if (!g_clip_te)
		return;

	if (g_clip_vscroll) {
		n_lines = (*g_clip_te)->nLines;
		if ((*g_clip_te)->teLength > 0 &&
		    (*((char **)(*g_clip_te)->hText))
		    [(*g_clip_te)->teLength - 1] != '\r')
			n_lines++;
		vis_lines = ((*g_clip_te)->viewRect.bottom -
		    (*g_clip_te)->viewRect.top) /
		    (*g_clip_te)->lineHeight;
		max_v = n_lines - vis_lines;
		if (max_v < 0) max_v = 0;
		SetControlMaximum(g_clip_vscroll, max_v);
	}

	if (g_clip_hscroll) {
		short i;
		GrafPtr save;

		GetPort(&save);
		SetPort(g_clip_window);
		TextFont(3);
		TextSize(9);

		max_w = 0;
		for (i = 0; i < (*g_clip_te)->nLines; i++) {
			short start, end, w;
			char *text;

			start = (*g_clip_te)->lineStarts[i];
			if (i + 1 < (*g_clip_te)->nLines)
				end = (*g_clip_te)->lineStarts[i + 1];
			else
				end = (*g_clip_te)->teLength;

			HLock((*g_clip_te)->hText);
			text = *(*g_clip_te)->hText;
			w = TextWidth(text, start, end - start);
			HUnlock((*g_clip_te)->hText);

			if (w > max_w) max_w = w;
		}

		vis_w = (*g_clip_te)->viewRect.right -
		    (*g_clip_te)->viewRect.left;
		max_h = max_w - vis_w;
		if (max_h < 0) max_h = 0;
		SetControlMaximum(g_clip_hscroll, max_h);

		SetPort(save);
	}
}

static void
clip_resize(void)
{
	Rect r, view_r, dest_r;
	short dv, dh;

	if (!g_clip_window)
		return;

	r = g_clip_window->portRect;

	if (g_clip_vscroll) {
		MoveControl(g_clip_vscroll, r.right - CLIP_SB_W, -1);
		SizeControl(g_clip_vscroll, CLIP_SB_W + 1,
		    r.bottom - CLIP_SB_W + 2);
	}

	if (g_clip_hscroll) {
		MoveControl(g_clip_hscroll, -1, r.bottom - CLIP_SB_W);
		SizeControl(g_clip_hscroll,
		    r.right - CLIP_SB_W + 2, CLIP_SB_W + 1);
	}

	if (g_clip_te) {
		clip_calc_rects(&view_r, &dest_r);

		dv = (*g_clip_te)->viewRect.top -
		    (*g_clip_te)->destRect.top;
		dh = (*g_clip_te)->viewRect.left -
		    (*g_clip_te)->destRect.left;

		(*g_clip_te)->viewRect = view_r;
		(*g_clip_te)->destRect = dest_r;
		TECalText(g_clip_te);

		clip_update_scroll();
	}
}

void
do_show_clipboard(void)
{
	Handle scrap_h;
	long scrap_len, scrap_offset;
	Rect bounds, view_r, dest_r, scroll_r;
	short mbar_h;
	GrafPtr save;

	if (g_clip_window) {
		SelectWindow(g_clip_window);
		return;
	}

	GetPort(&save);

	mbar_h = GetMBarHeight();
	bounds.left = (qd.screenBits.bounds.right -
	    CLIP_WIN_W) / 2;
	bounds.top = mbar_h + 30;
	bounds.right = bounds.left + CLIP_WIN_W;
	bounds.bottom = bounds.top + CLIP_WIN_H;

	g_clip_window = NewWindow(0L, &bounds,
	    "\pClipboard", true, documentProc,
	    (WindowPtr)-1L, true, 0L);
	if (!g_clip_window) {
		SetPort(save);
		return;
	}

	((WindowPeek)g_clip_window)->windowKind = CLIP_WIN_KIND;

	SetPort(g_clip_window);

	/* Vertical scrollbar */
	scroll_r.left = g_clip_window->portRect.right - CLIP_SB_W;
	scroll_r.top = g_clip_window->portRect.top - 1;
	scroll_r.right = g_clip_window->portRect.right + 1;
	scroll_r.bottom = g_clip_window->portRect.bottom -
	    (CLIP_SB_W - 1);
	g_clip_vscroll = NewControl(g_clip_window,
	    &scroll_r, "\p", true, 0, 0, 0,
	    scrollBarProc, 0L);

	/* Horizontal scrollbar */
	scroll_r.left = g_clip_window->portRect.left - 1;
	scroll_r.top = g_clip_window->portRect.bottom - CLIP_SB_W;
	scroll_r.right = g_clip_window->portRect.right -
	    (CLIP_SB_W - 1);
	scroll_r.bottom = g_clip_window->portRect.bottom + 1;
	g_clip_hscroll = NewControl(g_clip_window,
	    &scroll_r, "\p", true, 0, 0, 0,
	    scrollBarProc, 0L);

	/* TextEdit */
	TextFont(3);    /* Geneva */
	TextSize(9);
	clip_calc_rects(&view_r, &dest_r);

	g_clip_te = TENew(&dest_r, &view_r);
	if (!g_clip_te) {
		DisposeWindow(g_clip_window);
		g_clip_window = 0L;
		g_clip_vscroll = 0L;
		g_clip_hscroll = 0L;
		SetPort(save);
		return;
	}

	/* Get clipboard text */
	scrap_len = GetScrap(0L, 'TEXT', &scrap_offset);
	if (scrap_len > 0) {
		if (scrap_len > 4096)
			scrap_len = 4096;
		scrap_h = NewHandle(scrap_len);
		if (scrap_h) {
			GetScrap(scrap_h, 'TEXT', &scrap_offset);
			HLock(scrap_h);
			TESetText(*scrap_h, scrap_len, g_clip_te);
			HUnlock(scrap_h);
			DisposeHandle(scrap_h);
		}
	} else {
		TESetText("(Clipboard is empty.)", 21,
		    g_clip_te);
	}

	clip_update_scroll();
	SetPort(save);
}

WindowPtr
clipboard_window_ptr(void)
{
	return g_clip_window;
}

void
clipboard_window_update(WindowPtr win)
{
	GrafPtr save;

	if (win != g_clip_window || !g_clip_te)
		return;

	GetPort(&save);
	SetPort(win);
	EraseRect(&win->portRect);
	TEUpdate(&win->portRect, g_clip_te);
	DrawControls(win);
	DrawGrowIcon(win);
	SetPort(save);
}

void
clipboard_window_close(void)
{
	if (g_clip_te) {
		TEDispose(g_clip_te);
		g_clip_te = 0L;
	}
	g_clip_vscroll = 0L;
	g_clip_hscroll = 0L;
	if (g_clip_window) {
		DisposeWindow(g_clip_window);
		g_clip_window = 0L;
	}
}

void
clipboard_window_click(WindowPtr win, Point where)
{
	ControlHandle ctl;
	short ctl_part;

	if (win != g_clip_window)
		return;

	SetPort(win);
	GlobalToLocal(&where);
	ctl_part = FindControl(where, win, &ctl);

	if (ctl && ctl_part != 0) {
		if (ctl_part == inThumb) {
			short old_val = GetControlValue(ctl);

			TrackControl(ctl, where, 0L);
			{
				short new_val = GetControlValue(ctl);
				if (ctl == g_clip_vscroll)
					TEScroll(0,
					    (old_val - new_val) *
					    (*g_clip_te)->lineHeight,
					    g_clip_te);
				else if (ctl == g_clip_hscroll)
					TEScroll(
					    old_val - new_val, 0,
					    g_clip_te);
			}
		} else if (ctl == g_clip_vscroll) {
			TrackControl(ctl, where,
			    (ControlActionUPP)clip_vscroll_action);
		} else if (ctl == g_clip_hscroll) {
			TrackControl(ctl, where,
			    (ControlActionUPP)clip_hscroll_action);
		}
	}
}

void
clipboard_window_grow(WindowPtr win, Point where)
{
	Rect limit;
	long new_size;

	if (win != g_clip_window)
		return;

	SetRect(&limit, CLIP_MIN_W, CLIP_MIN_H,
	    qd.screenBits.bounds.right,
	    qd.screenBits.bounds.bottom);
	new_size = GrowWindow(win, where, &limit);
	if (new_size == 0)
		return;

	SetPort(win);
	EraseRect(&win->portRect);
	SizeWindow(win, LoWord(new_size),
	    HiWord(new_size), true);
	clip_resize();
	InvalRect(&win->portRect);
}
