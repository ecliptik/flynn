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
#define DLOG_FIND_ID        138
#define DLOG_DISCONN_ID     139  /* Disconnect alert with Reconnect */

/* Find dialog items (must match DITL 138) */
#define FIND_OK             1
#define FIND_CANCEL         2
#define FIND_LABEL          3
#define FIND_TEXT           4
#define FIND_DEFAULT_BTN    5

#ifdef FLYNN_FAVORITES
#define DLOG_FAVORITES_ID   131
#define DLOG_FAV_EDIT_ID    132

/* Favorites manager dialog items (must match DITL 131) */
#define BM_DONE             1
#define BM_ADD              2
#define BM_EDIT             3
#define BM_LABEL            4
#define BM_LIST             5	/* UserItem for List Manager */
#define BM_DEFAULT_BTN      6	/* UserItem for default button outline */
#define BM_REMOVE           7
#define BM_MOVE_UP          8
#define BM_MOVE_DOWN        9
#define BM_CONNECT          10

/* Favorite add/edit dialog items (DITL 132) */
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
#define BME_SIZE_LABEL      19
#define BME_SIZE_BTN        20
#define BME_THEME_LABEL     21
#define BME_THEME_BTN       22
#define BME_BKSP_LABEL      23
#define BME_BKSP_BTN        24
#define BME_ECHO_LABEL      25
#define BME_ECHO_BTN        26
#endif /* FLYNN_FAVORITES */

/* Set a button's title text (used by favorites edit dialog too) */
void bme_set_btn_title(DialogPtr dlg, short item, const char *text);

/* Draw a 3-pixel rounded rect outline around the default button */
pascal void draw_default_button(WindowPtr dlg, short item);

/* Register the default button outline UserItem in a dialog */
void setup_default_button_outline(DialogPtr dlg, short outline_item);

/* Simple dialog filter for Return=OK, Cmd+.=Cancel */
pascal Boolean std_dlg_filter(DialogPtr dlg, EventRecord *evt,
    short *item);

/* Show terminal type popup menu at a dialog item */
short show_ttype_popup(DialogPtr dlg, short item_num,
    short current_ttype, Boolean include_default);

/* Show the Connect dialog */
void do_connect(void);

#ifdef FLYNN_FAVORITES
/* Connect directly via bookmark index */
void do_connect_bookmark(short index);
#else
#define do_connect_bookmark(i)    ((void)0)
#endif

/* HIG-appropriate modal dialog proc (dBoxProc on Sys6, movableDBoxProc on Sys7) */
short modal_dialog_proc(void);

/* Load a modal dialog with system-appropriate proc type */
DialogPtr get_modal_dialog(short dlog_id);

/* Handle events for movable modal dialogs: title-bar drag and
 * background window redraw.  Returns true if event was consumed. */
Boolean modal_filter_event(DialogPtr dlg, EventRecord *evt);

/* Show the About dialog */
void do_about(void);

/* Show the DNS server configuration dialog */
void do_dns_server_dialog(void);

/* Reconnect active session with saved host/port */
void do_reconnect(void);

/* Find in scrollback */
void do_find(void);
void do_find_again(void);
Boolean find_has_last_search(void);

/* Clear scrollback buffer */
void do_clear_scrollback(void);

/* Status window UI (moved from connection.c) */
WindowPtr conn_status_show(const char *msg);
void conn_status_update(WindowPtr w, const char *msg);
void conn_status_close(WindowPtr w);

#endif /* DIALOGS_H */
