/*
 * settings.c - Preferences persistence for Flynn
 */

#include <Files.h>
#include <Memory.h>
#include <string.h>
#include "settings.h"

#define PREFS_FILENAME	"\pFlynn Prefs"

static void
prefs_defaults(FlynnPrefs *prefs)
{
	memset(prefs, 0, sizeof(FlynnPrefs));
	prefs->version = PREFS_VERSION;
	prefs->host[0] = '\0';
	prefs->port = 23;
	prefs->font_id = 4;	/* Monaco */
	prefs->font_size = 9;
	prefs->terminal_type = 0;	/* xterm */
	prefs->dark_mode = 0;		/* light */
	strcpy(prefs->dns_server, "1.1.1.1");
}

void
prefs_load(FlynnPrefs *prefs)
{
	short refNum;
	long count;
	short vRefNum;
	OSErr err;

	prefs_defaults(prefs);

	err = GetVol(0L, &vRefNum);
	if (err != noErr)
		return;

	err = FSOpen(PREFS_FILENAME, vRefNum, &refNum);
	if (err != noErr)
		return;

	count = sizeof(FlynnPrefs);
	err = FSRead(refNum, &count, (Ptr)prefs);
	FSClose(refNum);

	if (err != noErr && err != eofErr) {
		prefs_defaults(prefs);
		return;
	}

	if (prefs->version == 1) {
		/* v1→v2 migration: host/port already read, zero bookmark fields */
		prefs->bookmark_count = 0;
		memset(prefs->bookmarks, 0, sizeof(prefs->bookmarks));
		prefs->font_id = 4;
		prefs->font_size = 9;
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version == 2) {
		/* v2→v3 migration: add font fields */
		prefs->font_id = 4;
		prefs->font_size = 9;
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version == 3) {
		/* v3→v4 migration: add terminal_type and dark_mode */
		prefs->terminal_type = 0;
		prefs->dark_mode = 0;
		strcpy(prefs->dns_server, "1.1.1.1");
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version == 4) {
		/* v4→v5 migration: add dns_server */
		strcpy(prefs->dns_server, "1.1.1.1");
		prefs->username[0] = '\0';
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version == 5) {
		/* v5→v6 migration: add username */
		prefs->username[0] = '\0';
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version != PREFS_VERSION)
		prefs_defaults(prefs);
}

void
prefs_save(FlynnPrefs *prefs)
{
	short refNum;
	long count;
	short vRefNum;
	OSErr err;

	err = GetVol(0L, &vRefNum);
	if (err != noErr)
		return;

	FSDelete(PREFS_FILENAME, vRefNum);
	err = Create(PREFS_FILENAME, vRefNum, 'FLYN', 'pref');
	if (err != noErr)
		return;

	err = FSOpen(PREFS_FILENAME, vRefNum, &refNum);
	if (err != noErr)
		return;

	prefs->version = PREFS_VERSION;
	count = sizeof(FlynnPrefs);
	FSWrite(refNum, &count, (Ptr)prefs);
	FSClose(refNum);
}
