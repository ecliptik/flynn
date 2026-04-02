/*
 * clipboard.h - Clipboard operations for Flynn
 * Extracted from main.c
 */

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#ifdef FLYNN_CLIPBOARD
/* Copy selected text to system clipboard */
void do_copy(void);

/* Paste system clipboard into active session */
void do_paste(void);

/* Select all text in active session */
void do_select_all(void);

/* Show Clipboard viewer window */
void do_show_clipboard(void);

/* Clipboard window event handlers (called from main event loop) */
WindowPtr clipboard_window_ptr(void);
void clipboard_window_update(WindowPtr win);
void clipboard_window_close(void);
void clipboard_window_click(WindowPtr win, Point where);
void clipboard_window_grow(WindowPtr win, Point where);
#else
#define do_copy()       ((void)0)
#define do_paste()      ((void)0)
#define do_select_all() ((void)0)
#define do_show_clipboard()              ((void)0)
#define clipboard_window_ptr()           ((WindowPtr)0L)
#define clipboard_window_update(w)       ((void)0)
#define clipboard_window_close()         ((void)0)
#define clipboard_window_click(w, p)     ((void)0)
#define clipboard_window_grow(w, p)      ((void)0)
#endif

#endif /* CLIPBOARD_H */
