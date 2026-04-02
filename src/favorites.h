/*
 * favorites.h - Favorites menu and dialog management for Flynn
 */

#ifndef FAVORITES_H
#define FAVORITES_H

#ifdef FLYNN_FAVORITES

/* Rebuild dynamic menu items (separator + bookmark entries) */
void favorites_rebuild_menu(void);

/* Handle a click on the Favorites menu */
void favorites_menu_click(short item);

/* Show the Manage Favorites dialog */
void favorites_manage(void);

/* Save current session as a new favorite */
void favorites_add(void);

/* Add bookmark index to MRU recent list */
void add_recent_bookmark(short index);

#else
#define favorites_rebuild_menu()    ((void)0)
#define favorites_menu_click(i)     ((void)0)
#define favorites_manage()          ((void)0)
#define favorites_add()             ((void)0)
#define add_recent_bookmark(i)      ((void)0)
#endif

#endif /* FAVORITES_H */
