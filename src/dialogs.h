/*
 * dialogs.h - Dialog management for Flynn
 * Extracted from main.c and connection.c
 */

#ifndef DIALOGS_H
#define DIALOGS_H

/* Connect dialog resource ID and items (must match DITL ordering) */
#define DLOG_CONNECT_ID  129
#define DLOG_OK          1
#define DLOG_CANCEL      2
#define DLOG_HOST_LABEL  3
#define DLOG_HOST_FIELD  4
#define DLOG_PORT_LABEL  5
#define DLOG_PORT_FIELD  6
#define DLOG_USER_LABEL  8
#define DLOG_USER_FIELD  9
#define DLOG_FAVORITES   10
#define DLOG_TTYPE_LABEL 11
#define DLOG_TTYPE_BTN   12
#define DLOG_DEFAULT_BTN 13	/* UserItem for default button outline */

/* Dialog resource IDs */
#define DLOG_ABOUT_ID       130
#define DLOG_DNS_ID         133

#ifdef FLYNN_BOOKMARKS
#define DLOG_FAVORITES_ID   131
#define DLOG_FAV_EDIT_ID    132

/* Bookmark manager dialog items */
#define BM_DONE             1
#define BM_ADD              2
#define BM_EDIT             3
#define BM_DELETE           4
#define BM_CONNECT          5
#define BM_LABEL            6
#define BM_LIST             7	/* UserItem for list area */
#define BM_DEFAULT_BTN      8	/* UserItem for default button outline */

/* Bookmark add/edit dialog items */
#define BME_OK              1
#define BME_CANCEL          2
#define BME_NAME_LABEL      3
#define BME_NAME_FIELD      4
#define BME_HOST_LABEL      5
#define BME_HOST_FIELD      6
#define BME_PORT_LABEL      7
#define BME_PORT_FIELD      8
#define BME_USER_LABEL      9
#define BME_USER_FIELD      10
#define BME_TTYPE_LABEL     11
#define BME_TTYPE_BTN       12
#define BME_FONT_LABEL      13
#define BME_FONT_BTN        14
#define BME_DEFAULT_BTN     15	/* UserItem for default button outline */
#define BME_PROTO_LABEL     16
#define BME_PROTO_BTN       17
#define BME_VERBOSE_CHK     18
#endif /* FLYNN_BOOKMARKS */

/* Draw a 3-pixel rounded rect outline around the default button */
pascal void draw_default_button(WindowPtr dlg, short item);

/* Register the default button outline UserItem in a dialog */
void setup_default_button_outline(DialogPtr dlg, short outline_item);

/* Simple dialog filter for Return=OK, Cmd+.=Cancel */
pascal Boolean std_dlg_filter(DialogPtr dlg, EventRecord *evt,
    short *item);

/* Show the Connect dialog */
void do_connect(void);

#ifdef FLYNN_BOOKMARKS
/* Connect directly via bookmark index */
void do_connect_bookmark(short index);

/* Show the Bookmarks manager dialog */
void do_bookmarks(void);

/* Save current session as a new bookmark */
void do_save_as_bookmark(void);
#else
#define do_connect_bookmark(i)    ((void)0)
#define do_bookmarks()            ((void)0)
#define do_save_as_bookmark()     ((void)0)
#endif

/* Show the About dialog */
void do_about(void);

/* Show the DNS server configuration dialog */
void do_dns_server_dialog(void);

/* Status window UI (moved from connection.c) */
WindowPtr conn_status_show(const char *msg);
void conn_status_update(WindowPtr w, const char *msg);
void conn_status_close(WindowPtr w);

#endif /* DIALOGS_H */
