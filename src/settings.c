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
#include "macutil.h"
#include "tcp.h"

/* prefs_defaults() and prefs_migrate() live in this Toolbox-free unit so
 * the migration logic can be unit-tested natively (tests/migration_test.c).
 * Including it here keeps it in the cross-compiled build with no CMake
 * change. */
#include "settings_migrate.c"

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

	/* Gestalt is unavailable on early System 6; guard the trap. */
	if (TrapAvailable(_GestaltDispatch) &&
	    Gestalt('fold', &response) == noErr) {
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

/* prefs_defaults() is defined in settings_migrate.c (included above). */

void
prefs_load(FlynnPrefs *prefs)
{
	HParamBlockRec pb;
	Ptr raw;
	long count;
	short vRefNum;
	long dirID;
	OSErr err;
	int migrated;

	prefs_defaults(prefs);

	err = prefs_get_location(&vRefNum, &dirID);
	if (err != noErr)
		return;

	/* Scratch buffer for the raw on-disk image.  Allocated on the heap
	 * (not the stack): a full FlynnPrefs image is several KB and
	 * prefs_migrate() also holds a frozen-layout copy on its stack, so a
	 * stack buffer here would roughly double peak stack use on a 4 MiB
	 * Mac Plus.  Sized to the current struct, which is >= every
	 * historical layout. */
	raw = NewPtr(sizeof(FlynnPrefs));
	if (raw == 0L)
		return;			/* low memory: keep defaults, no save */

	memset(&pb, 0, sizeof(pb));
	pb.ioParam.ioNamePtr = (StringPtr)PREFS_FILENAME;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.ioParam.ioPermssn = fsRdPerm;
	pb.fileParam.ioDirID = dirID;
	err = PBHOpenSync(&pb);
	if (err != noErr) {
		DisposePtr(raw);
		return;
	}

	/* Read the raw on-disk image, then migrate it into the current
	 * layout.  Reading into a separate buffer (rather than straight into
	 * *prefs) is what lets prefs_migrate() interpret each historical
	 * layout correctly instead of aliasing old bytes onto the current
	 * field offsets. */
	memset(raw, 0, sizeof(FlynnPrefs));
	count = sizeof(FlynnPrefs);
	err = FSRead(pb.ioParam.ioRefNum, &count, raw);
	FSClose(pb.ioParam.ioRefNum);

	if (err != noErr && err != eofErr) {
		DisposePtr(raw);
		prefs_defaults(prefs);
		return;
	}

	/* count now holds the number of bytes actually read. */
	migrated = prefs_migrate(raw, count, prefs);
	DisposePtr(raw);

	/* Force null termination on all string fields (defense against a
	 * corrupted file). */
	prefs->host[sizeof(prefs->host) - 1] = '\0';
	prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
	prefs->username[sizeof(prefs->username) - 1] = '\0';
	{
		short i;
		short bc = prefs->bookmark_count;
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
	if (prefs->dns_server[0] == '\0' || prefs->dns_server[0] == '.' ||
	    ip2long(prefs->dns_server) == 0) {
		strncpy(prefs->dns_server, "1.1.1.1",
		    sizeof(prefs->dns_server) - 1);
		prefs->dns_server[sizeof(prefs->dns_server) - 1] = '\0';
	}

	/* Persist the upgraded layout so the next launch loads it directly. */
	if (migrated)
		prefs_save(prefs);
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

	/* Create new file.  The old file was just deleted, so a failure
	 * here means the settings are gone -- surface it. */
	memset(&pb, 0, sizeof(pb));
	pb.ioParam.ioNamePtr = (StringPtr)PREFS_FILENAME;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.fileParam.ioDirID = dirID;
	err = PBHCreateSync(&pb);
	if (err != noErr) {
		show_error_alert("Could not save preferences.");
		return;
	}

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
	if (err != noErr) {
		show_error_alert("Could not save preferences.");
		return;
	}

	prefs->version = PREFS_VERSION;
	count = sizeof(FlynnPrefs);
	err = FSWrite(pb.ioParam.ioRefNum, &count, (Ptr)prefs);
	{
		OSErr cerr = FSClose(pb.ioParam.ioRefNum);
		if (err == noErr)
			err = cerr;
	}

	/* Flush the volume so the write reaches disk (important on floppy /
	 * removable media), and report a full/locked/failed disk rather than
	 * silently losing all settings. */
	if (err == noErr)
		FlushVol(0L, vRefNum);
	else
		show_error_alert("Could not save preferences.");
}
