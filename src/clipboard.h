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
#else
#define do_copy()       ((void)0)
#define do_paste()      ((void)0)
#define do_select_all() ((void)0)
#endif

#endif /* CLIPBOARD_H */
