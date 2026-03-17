/*
 * savefile.h - Save session content to text file
 */

#ifndef SAVEFILE_H
#define SAVEFILE_H

#include "terminal.h"

/* cell_to_char is now in terminal.h / terminal.c (shared with clipboard) */

#ifdef FLYNN_SAVEFILE
/* Save active session's terminal content (scrollback + screen) to a text file.
 * Shows SFPutFile dialog for file placement. */
void do_save_session(void);
#else
#define do_save_session() ((void)0)
#endif

#endif /* SAVEFILE_H */
