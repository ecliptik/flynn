/*
 * main.h - Flynn: Telnet client for classic Macintosh
 * Global declarations, menu IDs, constants
 */

#ifndef MAIN_H
#define MAIN_H

/* Menu bar resource ID */
#define MBAR_ID             128

/* Menu resource IDs */
#define APPLE_MENU_ID       128
#define FILE_MENU_ID        129
#define EDIT_MENU_ID        130
#define PREFS_MENU_ID       131

/* Apple menu items */
#define APPLE_MENU_ABOUT_ID 1

/* Session menu items */
#define FILE_MENU_CONNECT_ID    1
#define FILE_MENU_DISCONNECT_ID 2
/* separator = 3 */
#define FILE_MENU_BOOKMARKS_ID  4
/* separator = 5 */
#define FILE_MENU_QUIT_ID       6
#define FILE_MENU_BM_BASE       7	/* first bookmark menu item */

/* Edit menu items */
#define EDIT_MENU_UNDO_ID   1
/* separator = 2 */
#define EDIT_MENU_CUT_ID    3
#define EDIT_MENU_COPY_ID   4
#define EDIT_MENU_PASTE_ID  5
#define EDIT_MENU_CLEAR_ID  6

/* Preferences menu items — organized by section */
#define PREFS_FONTS_HDR      1   /* disabled section header */
#define PREFS_FONT9_ID       2
#define PREFS_FONT12_ID      3
/* separator = 4 */
#define PREFS_TTYPE_HDR      5   /* disabled section header */
#define PREFS_XTERM_ID       6
#define PREFS_VT220_ID       7
#define PREFS_VT100_ID       8
/* separator = 9 */
#define PREFS_NET_HDR       10   /* disabled section header */
#define PREFS_DNS_ID        11
/* separator = 12 */
#define PREFS_MISC_HDR      13   /* disabled section header */
#define PREFS_DARK_ID       14

/* Dialog resource IDs */
#define DLOG_ABOUT_ID       130
#define DLOG_BOOKMARKS_ID   131
#define DLOG_BM_EDIT_ID     132
#define DLOG_DNS_ID         133

/* Bookmark manager dialog items */
#define BM_DONE             1
#define BM_ADD              2
#define BM_EDIT             3
#define BM_DELETE            4
#define BM_CONNECT          5
#define BM_LABEL            6
#define BM_LIST             7	/* UserItem for list area */

/* Bookmark add/edit dialog items */
#define BME_OK              1
#define BME_CANCEL          2
#define BME_NAME_LABEL      3
#define BME_NAME_FIELD      4
#define BME_HOST_LABEL      5
#define BME_HOST_FIELD      6
#define BME_PORT_LABEL      7
#define BME_PORT_FIELD      8

/* Max window content area for grid computation */
#define MAX_WIN_WIDTH       500
#define MAX_WIN_HEIGHT      320

/* Application creator and type */
#define APP_CREATOR         'FLYN'
#define APP_TYPE            'APPL'

#endif /* MAIN_H */
