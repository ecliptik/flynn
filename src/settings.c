/*
 * settings.c - Preferences persistence for Flynn
 *
 * On System 7+, preferences are stored in the Preferences folder
 * inside the System Folder.  On System 6, they are stored at the
 * root of the default volume.
 */

#include <Files.h>
#include <Memory.h>
#include <string.h>
#include "settings.h"
#include "sysutil.h"
#include "tcp.h"

#define PREFS_FILENAME	"\pFlynn Preferences"

/*
 * Locate the directory for preferences storage.
 * System 7+: Preferences folder (via FindFolder).
 * System 6:  default volume root (via GetVol).
 */
static OSErr
prefs_get_location(short *vRefNum, long *dirID)
{
	long response;

	if (Gestalt('fold', &response) == noErr) {
		OSErr err;
		err = FindFolder(kOnSystemDisk, kPreferencesFolderType,
		    true, vRefNum, dirID);
		if (err == noErr)
			return noErr;
	}

	/* System 6 fallback: default volume root */
	*dirID = 0;
	return GetVol(0L, vRefNum);
}

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
	prefs->backspace_bs = 0;	/* DEL (0x7F) for xterm */
	prefs->local_echo = 0;		/* off by default */
	prefs->show_status_bar = 1;	/* on by default */
	strncpy(prefs->dns_server, "1.1.1.1", sizeof(prefs->dns_server) - 1);
	prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
}

void
prefs_load(FlynnPrefs *prefs)
{
	HParamBlockRec pb;
	long count;
	short vRefNum;
	long dirID;
	OSErr err;

	prefs_defaults(prefs);

	err = prefs_get_location(&vRefNum, &dirID);
	if (err != noErr)
		return;

	memset(&pb, 0, sizeof(pb));
	pb.ioParam.ioNamePtr = (StringPtr)PREFS_FILENAME;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.ioParam.ioPermssn = fsRdPerm;
	pb.fileParam.ioDirID = dirID;
	err = PBHOpenSync(&pb);
	if (err != noErr)
		return;

	count = sizeof(FlynnPrefs);
	err = FSRead(pb.ioParam.ioRefNum, &count, (Ptr)prefs);
	FSClose(pb.ioParam.ioRefNum);

	if (err != noErr && err != eofErr) {
		prefs_defaults(prefs);
		return;
	}

	/* Force null termination on all string fields (defense against corrupted file) */
	prefs->host[sizeof(prefs->host) - 1] = '\0';
	prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
	prefs->username[sizeof(prefs->username) - 1] = '\0';
	{
		short i;
		for (i = 0; i < MAX_BOOKMARKS; i++) {
			prefs->bookmarks[i].name[sizeof(prefs->bookmarks[i].name) - 1] = '\0';
			prefs->bookmarks[i].host[sizeof(prefs->bookmarks[i].host) - 1] = '\0';
			prefs->bookmarks[i].username[sizeof(prefs->bookmarks[i].username) - 1] = '\0';
		}
	}

	/* Validate DNS server IP */
	{
		if (prefs->dns_server[0] == '\0' || prefs->dns_server[0] == '.' ||
		    ip2long(prefs->dns_server) == 0) {
			strncpy(prefs->dns_server, "1.1.1.1",
			    sizeof(prefs->dns_server) - 1);
			prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
		}
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
		strncpy(prefs->dns_server, "1.1.1.1",
		    sizeof(prefs->dns_server) - 1);
		prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version == 4) {
		/* v4→v5 migration: add dns_server */
		strncpy(prefs->dns_server, "1.1.1.1",
		    sizeof(prefs->dns_server) - 1);
		prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
		prefs->username[0] = '\0';
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version == 5) {
		/* v5→v6 migration: add username */
		prefs->username[0] = '\0';
		/* fall through to v6→v7 migration */
		prefs->version = 6;
	}

	if (prefs->version == 6) {
		/* v6→v7 migration: add per-bookmark settings */
		{
			short i;
			for (i = 0; i < MAX_BOOKMARKS; i++) {
				prefs->bookmarks[i].username[0] = '\0';
				prefs->bookmarks[i].terminal_type = -1;
				prefs->bookmarks[i].font_id = 0;
				prefs->bookmarks[i].font_size = 0;
			}
		}
		/* fall through to v7→v8 migration */
		prefs->version = 7;
	}

	if (prefs->version == 7) {
		/* v7→v8 migration: add recent bookmarks */
		prefs->recent_count = 0;
		{
			short i;
			for (i = 0; i < MAX_RECENT; i++)
				prefs->recent[i] = -1;
		}
		/* fall through to v8→v9 migration */
		prefs->version = 8;
	}

	if (prefs->version == 8) {
		/* v8→v9 migration: add backspace_bs.
		 * backspace_bs was inserted before dns_server in the struct,
		 * so reading v8 data into v9 layout shifts dns_server by 1 byte
		 * (e.g., "1.1.1.1" becomes ".1.1.1"). Reset dns_server to default. */
		prefs->backspace_bs =
		    (prefs->terminal_type == 4) ? 1 : 0;
		strncpy(prefs->dns_server, "1.1.1.1",
		    sizeof(prefs->dns_server) - 1);
		prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
		prefs->version = PREFS_VERSION;
		prefs_save(prefs);
		return;
	}

	if (prefs->version == 9) {
		/* v9→v10 migration: add local_echo.
		 * Enable by default for ANSI-BBS. */
		prefs->local_echo =
		    (prefs->terminal_type == 4) ? 1 : 0;
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
	HParamBlockRec pb;
	long count;
	short vRefNum;
	long dirID;
	OSErr err;

	err = prefs_get_location(&vRefNum, &dirID);
	if (err != noErr)
		return;

	/* Delete existing file */
	memset(&pb, 0, sizeof(pb));
	pb.ioParam.ioNamePtr = (StringPtr)PREFS_FILENAME;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.fileParam.ioDirID = dirID;
	PBHDeleteSync(&pb);

	/* Create new file */
	memset(&pb, 0, sizeof(pb));
	pb.ioParam.ioNamePtr = (StringPtr)PREFS_FILENAME;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.fileParam.ioDirID = dirID;
	err = PBHCreateSync(&pb);
	if (err != noErr)
		return;

	/* Set type and creator */
	memset(&pb, 0, sizeof(pb));
	pb.ioParam.ioNamePtr = (StringPtr)PREFS_FILENAME;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.fileParam.ioDirID = dirID;
	err = PBHGetFInfoSync(&pb);
	if (err != noErr)
		return;
	pb.fileParam.ioDirID = dirID;	/* PBHGetFInfo clears this */
	pb.fileParam.ioFlFndrInfo.fdType = 'pref';
	pb.fileParam.ioFlFndrInfo.fdCreator = 'FLYN';
	PBHSetFInfoSync(&pb);

	/* Open and write */
	memset(&pb, 0, sizeof(pb));
	pb.ioParam.ioNamePtr = (StringPtr)PREFS_FILENAME;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.ioParam.ioPermssn = fsWrPerm;
	pb.fileParam.ioDirID = dirID;
	err = PBHOpenSync(&pb);
	if (err != noErr)
		return;

	prefs->version = PREFS_VERSION;
	count = sizeof(FlynnPrefs);
	FSWrite(pb.ioParam.ioRefNum, &count, (Ptr)prefs);
	FSClose(pb.ioParam.ioRefNum);
}
