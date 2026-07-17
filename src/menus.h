/*
 * menus.h - Menu management for Flynn
 * Extracted from main.c
 */

#ifndef MENUS_H
#define MENUS_H

#include <Menus.h>

struct Session;  /* forward declaration */

/* Initialize menus from MBAR resource */
void init_menus(void);

/* Update menu enable/disable state */
void update_menus(void);

/* Update window menu with current session list */
void update_window_menu(void);

/* Update just one session's Window-menu item text (title-only change) */
void update_window_menu_title(struct Session *s);

/* Update Preferences menu checkmarks */
void update_prefs_menu(void);

/* Handle a menu selection. Returns true if handled. */
Boolean handle_menu(long menu_id);

#endif /* MENUS_H */
