/*
 * settings.h - Preferences persistence for Flynn
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#define PREFS_VERSION	1

typedef struct {
	short	version;
	char	host[256];
	short	port;
} FlynnPrefs;

/* Load preferences from "Flynn Prefs" file. Returns defaults if not found. */
void prefs_load(FlynnPrefs *prefs);

/* Save preferences to "Flynn Prefs" file. */
void prefs_save(FlynnPrefs *prefs);

#endif /* SETTINGS_H */
