/*
 * settings.h - Preferences persistence for Flynn
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "theme.h"

#define PREFS_VERSION	18
#define MAX_BOOKMARKS	20
#define MAX_RECENT	5

typedef struct {
	char		name[32];
	char		host[128];
	unsigned short	port;
	char		username[64];
	short		terminal_type;	/* 0=xterm, 1=VT220, 2=VT100, 3=xterm-256color, 4=ansi; -1=use global */
	short		font_id;	/* 0=use global default */
	short		font_size;	/* 0=use global default */
	signed char	bm_theme_id;	/* -1=use global default */
	signed char	bm_backspace_bs;/* -1=use global default */
	signed char	bm_local_echo;	/* -1=use global default */
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
	unsigned char	backspace_bs;	/* 1=BS(0x08), 0=DEL(0x7F) */
	char		dns_server[16];	/* IP address, default "1.1.1.1" */
	char		username[64];	/* auto-login username, empty = disabled */
	short		recent[MAX_RECENT];	/* recently used bookmark indices */
	short		recent_count;
	unsigned char	local_echo;	/* 1=echo locally when server WONT ECHO */
	unsigned char	show_status_bar;	/* 1=show status bar, 0=hide */
	/* NOTE: always append new fields here, never insert above */
	short		bookmark_protocol[MAX_BOOKMARKS];	/* PROTO_TELNET(0) or PROTO_FINGER(1) */
	char		finger_host[128];	/* last finger host */
	char		finger_user[64];	/* last finger username */
	unsigned char	bookmark_verbose[MAX_BOOKMARKS];	/* 1=send /W for finger */
	short		win_x, win_y;	/* saved window position (global coords) */
	unsigned char	theme_id;	/* 0=Light, 1=Dark, 2+=color themes */
	/* Custom imported themes (v18) */
#ifdef FLYNN_THEMES
	CustomTheme	custom_themes[MAX_CUSTOM_THEMES];
	short		custom_theme_count;
#endif
} FlynnPrefs;

/* Load preferences from "Flynn Preferences" file. Returns defaults if not found. */
void prefs_load(FlynnPrefs *prefs);

/* Save preferences to "Flynn Preferences" file. */
void prefs_save(FlynnPrefs *prefs);

/* Initialise a prefs struct to factory defaults (see settings_migrate.c). */
void prefs_defaults(FlynnPrefs *prefs);

/* Convert a raw on-disk prefs image (len bytes) into the current layout.
 * out must already be prefs_defaults()-initialised.  Returns 1 if the
 * layout was migrated (re-save recommended), 0 otherwise.  Pure logic,
 * no Toolbox -- unit-tested natively in tests/migration_test.c. */
int prefs_migrate(const void *raw, long len, FlynnPrefs *out);

#endif /* SETTINGS_H */
