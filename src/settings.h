/*
 * settings.h - Preferences persistence for Flynn
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#define PREFS_VERSION	5
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
	short		terminal_type;	/* 0=xterm, 1=VT220, 2=VT100 */
	unsigned char	dark_mode;	/* 0=light, 1=dark */
	char		dns_server[16];	/* IP address, default "1.1.1.1" */
} FlynnPrefs;

/* Load preferences from "Flynn Prefs" file. Returns defaults if not found. */
void prefs_load(FlynnPrefs *prefs);

/* Save preferences to "Flynn Prefs" file. */
void prefs_save(FlynnPrefs *prefs);

#endif /* SETTINGS_H */
