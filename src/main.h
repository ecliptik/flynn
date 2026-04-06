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
#define FILE_MENU_DISCONNECT_ID 2
#define FILE_MENU_RECONNECT_ID  3   /* Reconnect (Cmd+R) */
/* separator = 4 */
#define FILE_MENU_FINGER_ID     5   /* Finger... (Cmd+I) */
/* separator = 6 */
#define FILE_MENU_SAVE_ID       7
#define FILE_MENU_LOG_ID        8   /* Start/Stop Logging (Cmd+L) */
/* separator = 9 */
#define FILE_MENU_PAGESETUP_ID  10  /* Page Setup... */
#define FILE_MENU_PRINT_ID      11  /* Print... (Cmd+P) */
/* separator = 12 */
#define FILE_MENU_QUIT_ID       13

/* Edit menu items */
#define EDIT_MENU_COPY_ID       4
#define EDIT_MENU_PASTE_ID      5
/* separator = 7 */
#define EDIT_MENU_SELALL_ID     8
/* separator = 9 */
#define EDIT_MENU_FIND_ID       10  /* Find... (Cmd+F) */
#define EDIT_MENU_FINDAGAIN_ID  11  /* Find Again (Cmd+G) */
/* separator = 12 */
#define EDIT_MENU_CLRSCROLL_ID  13  /* Clear Scrollback */
/* separator = 14 */
#define EDIT_MENU_SHOW_CLIP_ID  15  /* Show Clipboard */

/* Options menu items (hierarchical submenus for Font, Size, Terminal Type) */
#define PREFS_FONT_HIER      1   /* Font submenu trigger */
#define PREFS_SIZE_HIER      2   /* Size submenu trigger */
#define PREFS_TTYPE_HIER     3   /* Terminal Type submenu trigger */
/* separator = 4 */
#define PREFS_THEME_HIER     5   /* Theme submenu trigger */
#define PREFS_DARK_ID        5   /* legacy alias */
#define PREFS_STATUS_BAR_ID  6
/* separator = 7 */
#define PREFS_BKSP_DEL_ID    8
#define PREFS_LOCAL_ECHO_ID  9
/* separator = 10 */
#define PREFS_DNS_ID         11

/* Font submenu (MENU 134) items */
#define FONT_MENU_ID        134
#define FONT_MONACO_ID       1
#define FONT_GENEVA_ID       2
#define FONT_CHICAGO_ID      3
#define FONT_COURIER_ID      4
#define FONT_NEWYORK_ID      5
/* 6-8: Helvetica, Times, Palatino appended at runtime on System 7 */
#define FONT_HELVETICA_ID    6
#define FONT_TIMES_ID        7
#define FONT_PALATINO_ID     8

/* Size submenu (MENU 137) items */
#define SIZE_MENU_ID        137
#define SIZE_9_ID            1
#define SIZE_10_ID           2
#define SIZE_12_ID           3
#define SIZE_14_ID           4

/* Terminal Type submenu (MENU 135) items */
#define TTYPE_MENU_ID       135
#define TTYPE_XTERM_ID       1
#define TTYPE_XTERM256_ID    2
#define TTYPE_VT100_ID       3
#define TTYPE_VT220_ID       4
#define TTYPE_ANSI_ID        5

/* Favorites menu resource ID (always defined for DeleteMenu when disabled) */
#define FAVORITES_MENU_ID   136

#ifdef FLYNN_FAVORITES
/* Favorites menu (MENU 136) items */
#define FAV_MANAGE_ID        1   /* Manage Favorites... */
#define FAV_ADD_ID           2   /* Add Favorite... */
/* separator = 3 (added dynamically when bookmarks exist) */
#define FAV_FIRST_BM         4   /* first bookmark entry */
#endif

/* Theme submenu (MENU 138) */
#define THEME_MENU_ID              138

#define THEME_ITEM_LIGHT           1
#define THEME_ITEM_DARK            2
/* item 3 is separator */
#define THEME_ITEM_SOLARIZED_LIGHT 4
#define THEME_ITEM_SOLARIZED_DARK  5
#define THEME_ITEM_TOKYO_LIGHT     6
#define THEME_ITEM_TOKYO_DARK      7
#define THEME_ITEM_GREEN_SCREEN    8
#define THEME_ITEM_CLASSIC         9
#define THEME_ITEM_PLATINUM        10

#define THEME_ITEM_FIRST           1
#define THEME_ITEM_LAST            10

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
/* separator = 10 */
#define CTRL_MENU_RESET     11  /* Reset Terminal */

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
