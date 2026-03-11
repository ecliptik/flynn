/*
 * settings.h - Preferences persistence for Flynn
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#define PREFS_VERSION	8
#define MAX_BOOKMARKS	8
#define MAX_RECENT	5

typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
	char		username[64];
	short		terminal_type;	/* 0=xterm, 1=VT220, 2=VT100, 3=xterm-256color, 4=ansi; -1=use global */
	short		font_id;	/* 0=use global default */
	short		font_size;	/* 0=use global default */
} Bookmark;

typedef struct {
	short		version;
	char		host[256];
	short		port;
	short		bookmark_count;
	Bookmark	bookmarks[MAX_BOOKMARKS];
	short		font_id;
	short		font_size;
	short		terminal_type;	/* 0=xterm, 1=VT220, 2=VT100, 3=xterm-256color, 4=ansi */
	unsigned char	dark_mode;	/* 0=light, 1=dark */
	char		dns_server[16];	/* IP address, default "1.1.1.1" */
	char		username[64];	/* auto-login username, empty = disabled */
	short		recent[MAX_RECENT];	/* recently used bookmark indices */
	short		recent_count;
} FlynnPrefs;

/* Load preferences from "Flynn Preferences" file. Returns defaults if not found. */
void prefs_load(FlynnPrefs *prefs);

/* Save preferences to "Flynn Preferences" file. */
void prefs_save(FlynnPrefs *prefs);

#endif /* SETTINGS_H */
