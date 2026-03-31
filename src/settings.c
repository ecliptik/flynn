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
		short bc = prefs->bookmark_count;
		/* Clamp to actual array size — critical for old-format
		 * files where bookmarks[8+] overlaps other fields */
		if (bc < 0) bc = 0;
		if (bc > MAX_BOOKMARKS) bc = MAX_BOOKMARKS;
		for (i = 0; i < bc; i++) {
			prefs->bookmarks[i].name[sizeof(prefs->bookmarks[i].name) - 1] = '\0';
			prefs->bookmarks[i].host[sizeof(prefs->bookmarks[i].host) - 1] = '\0';
			prefs->bookmarks[i].username[sizeof(prefs->bookmarks[i].username) - 1] = '\0';
		}
	}
	prefs->finger_host[sizeof(prefs->finger_host) - 1] = '\0';
	prefs->finger_user[sizeof(prefs->finger_user) - 1] = '\0';

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
		/* v6→v7 migration: add per-bookmark settings.
		 * Use literal 8 — old layout had 8-slot arrays. */
		{
			short i;
			for (i = 0; i < 8; i++) {
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
		/* fall through to v10→v11 migration */
		prefs->version = 10;
	}

	if (prefs->version == 10) {
		/* v10→v11 migration: add bookmark_protocol array.
		 * Use literal 8 — old layout had 8-slot arrays. */
		{
			short i;
			for (i = 0; i < 8; i++)
				prefs->bookmark_protocol[i] = 0;
		}
		/* fall through to v11→v12 migration */
		prefs->version = 11;
	}

	if (prefs->version == 11) {
		/* v11→v12 migration: add finger_host/finger_user */
		prefs->finger_host[0] = '\0';
		prefs->finger_user[0] = '\0';
		/* fall through to v12→v13 migration */
		prefs->version = 12;
	}

	if (prefs->version == 12) {
		/* v12→v13 migration: add bookmark_verbose.
		 * bookmark_verbose was appended at end — no layout shift
		 * for 8-bookmark struct. Fall through to v13→v14. */
		{
			short i;
			for (i = 0; i < 8; i++)
				prefs->bookmark_verbose[i] = 0;
		}
		prefs->version = 13;
	}

	if (prefs->version == 13) {
		/* v13→v14 migration: MAX_BOOKMARKS 8→20.
		 * bookmarks[], bookmark_protocol[], bookmark_verbose[]
		 * all grew, shifting every field after bookmarks[8] in
		 * the binary layout.  Read old data via a struct that
		 * matches the v13 on-disk format, then copy into the
		 * new (larger) layout field by field. */
		struct V13Prefs {
			short		version;
			char		host[256];
			short		port;
			short		bookmark_count;
			Bookmark	bookmarks[8];
			short		font_id;
			short		font_size;
			short		terminal_type;
			unsigned char	dark_mode;
			unsigned char	backspace_bs;
			char		dns_server[16];
			char		username[64];
			short		recent[MAX_RECENT];
			short		recent_count;
			unsigned char	local_echo;
			unsigned char	show_status_bar;
			short		bookmark_protocol[8];
			char		finger_host[128];
			char		finger_user[64];
			unsigned char	bookmark_verbose[8];
		};
		{
			struct V13Prefs old;
			short i, bc;

			/* Raw bytes in prefs match v13 layout */
			memcpy(&old, prefs,
			    sizeof(struct V13Prefs));

			/* Reset to defaults with new layout */
			prefs_defaults(prefs);

			/* Copy scalar fields */
			memcpy(prefs->host, old.host,
			    sizeof(old.host));
			prefs->port = old.port;
			bc = old.bookmark_count;
			if (bc < 0) bc = 0;
			if (bc > 8) bc = 8;
			prefs->bookmark_count = bc;

			/* Copy old bookmarks into first 8 slots */
			for (i = 0; i < bc; i++)
				prefs->bookmarks[i] =
				    old.bookmarks[i];

			prefs->font_id = old.font_id;
			prefs->font_size = old.font_size;
			prefs->terminal_type = old.terminal_type;
			prefs->dark_mode = old.dark_mode;
			prefs->backspace_bs = old.backspace_bs;
			memcpy(prefs->dns_server, old.dns_server,
			    sizeof(old.dns_server));
			memcpy(prefs->username, old.username,
			    sizeof(old.username));
			memcpy(prefs->recent, old.recent,
			    sizeof(old.recent));
			prefs->recent_count = old.recent_count;
			prefs->local_echo = old.local_echo;
			prefs->show_status_bar = old.show_status_bar;

			/* Copy per-bookmark arrays (first 8) */
			for (i = 0; i < 8; i++) {
				prefs->bookmark_protocol[i] =
				    old.bookmark_protocol[i];
				prefs->bookmark_verbose[i] =
				    old.bookmark_verbose[i];
			}

			memcpy(prefs->finger_host,
			    old.finger_host,
			    sizeof(old.finger_host));
			memcpy(prefs->finger_user,
			    old.finger_user,
			    sizeof(old.finger_user));

			prefs->version = PREFS_VERSION;
			prefs_save(prefs);
			return;
		}
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
