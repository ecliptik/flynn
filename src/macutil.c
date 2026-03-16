/*
 * macutil.c - Shared Mac Toolbox utility helpers
 *
 * Pascal string conversions, dialog item helpers, window title helpers,
 * and dark-mode background clear.
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Dialogs.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "macutil.h"

void
c2pstr(Str255 dst, const char *src)
{
	short len;

	len = strlen(src);
	if (len > 255)
		len = 255;
	dst[0] = len;
	memcpy(dst + 1, src, len);
}

void
set_wtitlef(WindowPtr win, const char *fmt, ...)
{
	va_list ap;
	char tmp[256];
	short len;
	Str255 ptitle;

	va_start(ap, fmt);
	len = vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	if (len < 0)
		len = 0;
	if (len > 255)
		len = 255;
	ptitle[0] = len;
	memcpy(ptitle + 1, tmp, len);
	SetWTitle(win, ptitle);
}

void
dlg_set_text(DialogPtr dlg, short item, const char *text)
{
	short item_type;
	Handle item_h;
	Rect item_rect;
	Str255 pstr;

	GetDialogItem(dlg, item, &item_type, &item_h, &item_rect);
	c2pstr(pstr, text);
	SetDialogItemText(item_h, pstr);
}

void
dlg_get_text(DialogPtr dlg, short item, char *buf, short buflen)
{
	short item_type;
	Handle item_h;
	Rect item_rect;
	Str255 pstr;
	short len, i;

	if (buflen <= 0)
		return;

	GetDialogItem(dlg, item, &item_type, &item_h, &item_rect);
	GetDialogItemText(item_h, pstr);

	len = pstr[0];
	if (len >= buflen)
		len = buflen - 1;
	for (i = 0; i < len; i++)
		buf[i] = pstr[i + 1];
	buf[len] = '\0';
}

void
clear_window_bg(WindowPtr win, Boolean dark_mode)
{
	if (dark_mode)
		PaintRect(&win->portRect);
	else
		EraseRect(&win->portRect);
}

void
show_error_alert(const char *msg)
{
	Str255 pmsg;

	c2pstr(pmsg, msg);
	ParamText(pmsg, "\p", "\p", "\p");
	StopAlert(128, 0L);
}
