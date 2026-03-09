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
#define CTRL_MENU_ID        132

/* Apple menu items */
#define APPLE_MENU_ABOUT_ID 1

/* Session menu items (static base) */
#define FILE_MENU_CONNECT_ID    1
#define FILE_MENU_DISCONNECT_ID 2
/* separator = 3 */
#define FILE_MENU_SAVE_BM_ID    4
#define FILE_MENU_BOOKMARKS_ID  5
/* separator = 6 */
/* items 7..N are recent bookmarks (dynamic) */
/* Quit is always last item — use CountMItems() */

/* Edit menu items */
#define EDIT_MENU_UNDO_ID   1
/* separator = 2 */
#define EDIT_MENU_CUT_ID    3
#define EDIT_MENU_COPY_ID   4
#define EDIT_MENU_PASTE_ID  5
#define EDIT_MENU_CLEAR_ID  6
/* separator = 7 */
#define EDIT_MENU_SELALL_ID 8

/* Options menu items — organized by section */
#define PREFS_FONTS_HDR      1   /* disabled section header */
#define PREFS_FONT9_ID       2   /* Monaco 9 */
#define PREFS_FONT12_ID      3   /* Monaco 12 */
#define PREFS_FONT_C10       4   /* Courier 10 */
#define PREFS_FONT_CH12      5   /* Chicago 12 */
#define PREFS_FONT_G9        6   /* Geneva 9 */
#define PREFS_FONT_G10       7   /* Geneva 10 */
/* separator = 8 */
#define PREFS_TTYPE_HDR      9   /* disabled section header */
#define PREFS_XTERM_ID      10
#define PREFS_VT220_ID      11
#define PREFS_VT100_ID      12
#define PREFS_XTERM256_ID   13
/* separator = 14 */
#define PREFS_NET_HDR       15   /* disabled section header */
#define PREFS_DNS_ID        16
/* separator = 17 */
#define PREFS_MISC_HDR      18   /* disabled section header */
#define PREFS_DARK_ID       19

/* Window menu */
#define WINDOW_MENU_ID      133
#define WIN_MENU_FIRST_WIN  3	/* after count header + separator */

/* Control menu items */
#define CTRL_MENU_CTRLC     1
#define CTRL_MENU_CTRLD     2
#define CTRL_MENU_CTRLL     3
#define CTRL_MENU_CTRLZ     4
/* separator = 5 */
#define CTRL_MENU_BREAK     6
#define CTRL_MENU_ESC       7

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

/* Max window content area for grid computation */
#define MAX_WIN_WIDTH       500
#define MAX_WIN_HEIGHT      320

/* Minimum window size in grid cells */
#define MIN_WIN_COLS        20
#define MIN_WIN_ROWS         5

/* Application creator and type */
#define APP_CREATOR         'FLYN'
#define APP_TYPE            'APPL'

#endif /* MAIN_H */
