/*
 * dialogs.h - Dialog management for Flynn
 * Extracted from main.c and connection.c
 */

#ifndef DIALOGS_H
#define DIALOGS_H

/* Draw a 3-pixel rounded rect outline around the default button */
pascal void draw_default_button(WindowPtr dlg, short item);

/* Register the default button outline UserItem in a dialog */
void setup_default_button_outline(DialogPtr dlg, short outline_item);

/* Simple dialog filter for Return=OK, Cmd+.=Cancel */
pascal Boolean std_dlg_filter(DialogPtr dlg, EventRecord *evt,
    short *item);

/* Show the Connect dialog */
void do_connect(void);

/* Connect directly via bookmark index */
void do_connect_bookmark(short index);

/* Show the Bookmarks manager dialog */
void do_bookmarks(void);

/* Save current session as a new bookmark */
void do_save_as_bookmark(void);

/* Show the About dialog */
void do_about(void);

/* Show the DNS server configuration dialog */
void do_dns_server_dialog(void);

/* Status window UI (moved from connection.c) */
WindowPtr conn_status_show(const char *msg);
void conn_status_update(WindowPtr w, const char *msg);
void conn_status_close(WindowPtr w);

#endif /* DIALOGS_H */
