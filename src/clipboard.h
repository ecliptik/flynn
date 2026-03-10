/*
 * clipboard.h - Clipboard operations for Flynn
 * Extracted from main.c
 */

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

/* Copy selected text to system clipboard */
void do_copy(void);

/* Paste system clipboard into active session */
void do_paste(void);

/* Select all text in active session */
void do_select_all(void);

#endif /* CLIPBOARD_H */
