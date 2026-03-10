/*
 * menus.h - Menu management for Flynn
 * Extracted from main.c
 */

#ifndef MENUS_H
#define MENUS_H

#include <Menus.h>

/* Initialize menus from MBAR resource */
void init_menus(void);

/* Update menu enable/disable state */
void update_menus(void);

/* Update window menu with current session list */
void update_window_menu(void);

/* Update Preferences menu checkmarks */
void update_prefs_menu(void);

/* Rebuild File menu dynamic items (recent bookmarks + Quit) */
void rebuild_file_menu(void);

/* Add bookmark index to MRU recent list */
void add_recent_bookmark(short index);

/* Handle a menu selection. Returns true if handled. */
Boolean handle_menu(long menu_id);

#endif /* MENUS_H */
