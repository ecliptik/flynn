/*
 * savefile.h - Save session content to text file
 */

#ifndef SAVEFILE_H
#define SAVEFILE_H

/* Save active session's terminal content (scrollback + screen) to a text file.
 * Shows SFPutFile dialog for file placement. */
void do_save_session(void);

#endif /* SAVEFILE_H */
