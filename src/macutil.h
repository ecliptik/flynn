/*
 * macutil.h - Shared Mac Toolbox utility helpers
 *
 * Pascal string conversions, dialog item helpers, window title helpers,
 * and dark-mode background clear.
 */

#ifndef MACUTIL_H
#define MACUTIL_H

#include <Types.h>
#include <Windows.h>
#include <Dialogs.h>

/* C string to Pascal string (caller-provided Str255 buffer) */
void c2pstr(Str255 dst, const char *src);

/* C string to Pascal string with max length */
void c2pstrn(Str255 dst, const char *src, short maxlen);

/* sprintf into Pascal string, returns length */
short sprintfp(Str255 dst, const char *fmt, ...);

/* SetWTitle from C string */
void set_wtitle_c(WindowPtr win, const char *title);

/* Formatted window title (sprintf + SetWTitle) */
void set_wtitlef(WindowPtr win, const char *fmt, ...);

/* Set dialog item text from C string */
void dlg_set_text(DialogPtr dlg, short item, const char *text);

/* Get dialog item text as C string */
void dlg_get_text(DialogPtr dlg, short item, char *buf, short buflen);

/* Clear window background: PaintRect if dark, EraseRect if light */
void clear_window_bg(WindowPtr win, Boolean dark_mode);

#endif /* MACUTIL_H */
