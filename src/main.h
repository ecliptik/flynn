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

/* Apple menu items */
#define APPLE_MENU_ABOUT_ID 1

/* Session menu items */
#define FILE_MENU_CONNECT_ID    1
#define FILE_MENU_DISCONNECT_ID 2
/* separator = 3 */
#define FILE_MENU_QUIT_ID       4

/* Edit menu items */
#define EDIT_MENU_UNDO_ID   1
/* separator = 2 */
#define EDIT_MENU_CUT_ID    3
#define EDIT_MENU_COPY_ID   4
#define EDIT_MENU_PASTE_ID  5
#define EDIT_MENU_CLEAR_ID  6

/* Dialog resource IDs */
#define DLOG_ABOUT_ID       130

/* Terminal window dimensions */
#define TERM_WIN_WIDTH      500
#define TERM_WIN_HEIGHT     320

/* Application creator and type */
#define APP_CREATOR         'FLYN'
#define APP_TYPE            'APPL'

#endif /* MAIN_H */
