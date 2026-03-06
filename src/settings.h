/*
 * settings.h - Preferences persistence for Flynn
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#define PREFS_VERSION	3
#define MAX_BOOKMARKS	8

typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
} Bookmark;

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	Bookmark	bookmarks[MAX_BOOKMARKS];
	short		font_id;
	short		font_size;
} FlynnPrefs;

/* Load preferences from "Flynn Prefs" file. Returns defaults if not found. */
void prefs_load(FlynnPrefs *prefs);

/* Save preferences to "Flynn Prefs" file. */
void prefs_save(FlynnPrefs *prefs);

#endif /* SETTINGS_H */
