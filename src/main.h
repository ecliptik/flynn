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

/* File menu items (fully static) */
#define FILE_MENU_CONNECT_ID    1
#define FILE_MENU_FINGER_ID     2   /* Finger... */
#define FILE_MENU_DISCONNECT_ID 3
/* separator = 4 */
#define FILE_MENU_SAVE_ID       5
#define FILE_MENU_DNS_ID        6
/* separator = 7 */
#define FILE_MENU_FAVORITES_ID  8   /* Favorites hierarchical submenu */
/* separator = 9 */
#define FILE_MENU_QUIT_ID       10

/* Edit menu items */
#define EDIT_MENU_COPY_ID   4
#define EDIT_MENU_PASTE_ID  5
/* separator = 7 */
#define EDIT_MENU_SELALL_ID 8

/* Options menu items (hierarchical submenus for Font and Terminal Type) */
#define PREFS_FONT_HIER      1   /* Font submenu trigger */
#define PREFS_TTYPE_HIER     2   /* Terminal Type submenu trigger */
/* separator = 3 */
#define PREFS_DARK_ID        4
#define PREFS_STATUS_BAR_ID  5
/* separator = 6 */
#define PREFS_BKSP_DEL_ID    7
#define PREFS_LOCAL_ECHO_ID  8

/* Font submenu (MENU 134) items */
#define FONT_MENU_ID        134
#define FONT_MONACO9_ID      1
#define FONT_MONACO12_ID     2
#define FONT_COURIER10_ID    3
#define FONT_CHICAGO12_ID    4
#define FONT_GENEVA9_ID      5
#define FONT_GENEVA10_ID     6

/* Terminal Type submenu (MENU 135) items */
#define TTYPE_MENU_ID       135
#define TTYPE_XTERM_ID       1
#define TTYPE_XTERM256_ID    2
#define TTYPE_VT100_ID       3
#define TTYPE_VT220_ID       4
#define TTYPE_ANSI_ID        5

#ifdef FLYNN_BOOKMARKS
/* Favorites submenu (MENU 136) items */
#define FAVORITES_MENU_ID   136
#define FAV_MANAGE_ID        1   /* Manage Favorites... */
#define FAV_ADD_ID           2   /* Add Favorite... */
/* separator = 3 (added dynamically when bookmarks exist) */
#define FAV_FIRST_BM         4   /* first bookmark entry */
#endif

/* Window menu */
#define WINDOW_MENU_ID      133
#define WIN_MENU_FIRST_WIN  3	/* after count header + separator */

/* Control menu items */
#define CTRL_MENU_CTRLC     1
#define CTRL_MENU_CTRLD     2
#define CTRL_MENU_CTRLH     3
#define CTRL_MENU_CTRLL     4
#define CTRL_MENU_CTRLX     5
#define CTRL_MENU_CTRLZ     6
/* separator = 7 */
#define CTRL_MENU_BREAK     8
#define CTRL_MENU_ESC       9

/* Max window content area for grid computation */
#define MAX_WIN_WIDTH       500
#define MAX_WIN_HEIGHT      320

/* Minimum window size in grid cells */
#define MIN_WIN_COLS        20
#define MIN_WIN_ROWS         5

/* Font preset table for bookmark font cycling */
typedef struct {
	short	font_id;
	short	font_size;
	char	name[16];
} FontPreset;

#define NUM_FONT_PRESETS 6

/* MultiFinder suspend state */
extern Boolean g_suspended;

#endif /* MAIN_H */
