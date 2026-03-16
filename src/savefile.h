/*
 * savefile.h - Save session content to text file
 */

#ifndef SAVEFILE_H
#define SAVEFILE_H

#include "terminal.h"

/* Convert a TermCell to a plain text character for clipboard/file export */
char cell_to_char(TermCell *cell);

/* Save active session's terminal content (scrollback + screen) to a text file.
 * Shows SFPutFile dialog for file placement. */
void do_save_session(void);

#endif /* SAVEFILE_H */
