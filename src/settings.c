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

	if (err != noErr || prefs->version != PREFS_VERSION)
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
