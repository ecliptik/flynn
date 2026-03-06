/*
 * sysutil.c - Minimal system utilities for DNR support
 *
 * Extracted from wallops-146 util.c by jcs@jcs.org
 */

#include <Traps.h>
#include <OSUtils.h>
#include <Memory.h>

/* Retro68 Multiverse.h uses kOSTrapType/kToolboxTrapType instead of OSTrap/ToolTrap */
#ifndef OSTrap
#define OSTrap kOSTrapType
#define ToolTrap kToolboxTrapType
#endif

/* _GestaltDispatch trap number (0xA0AD) */
#ifndef _GestaltDispatch
#define _GestaltDispatch _Gestalt
#endif

/* kOnSystemDisk = boot volume (-1) */
#ifndef kOnSystemDisk
#define kOnSystemDisk ((short)-1)
#endif
#include <stdbool.h>

#include "sysutil.h"

TrapType
GetTrapType(unsigned long theTrap)
{
	if (BitAnd(theTrap, 0x0800) > 0)
		return ToolTrap;

	return OSTrap;
}

bool
TrapAvailable(unsigned long trap)
{
	TrapType trapType = ToolTrap;
	unsigned long numToolBoxTraps;

	if (NGetTrapAddress(_InitGraf, ToolTrap) ==
	    NGetTrapAddress(0xAA6E, ToolTrap))
		numToolBoxTraps = 0x200;
	else
		numToolBoxTraps = 0x400;

	trapType = GetTrapType(trap);
	if (trapType == ToolTrap) {
		trap = BitAnd(trap, 0x07FF);
		if (trap >= numToolBoxTraps)
			trap = _Unimplemented;
	}

	return (NGetTrapAddress(trap, trapType) !=
	    NGetTrapAddress(_Unimplemented, ToolTrap));
}

void
GetSystemFolder(short *vRefNumP, long *dirIDP)
{
	SysEnvRec info;
	long wdProcID;

	SysEnvirons(1, &info);
	if (GetWDInfo(info.sysVRefNum, vRefNumP, dirIDP, &wdProcID) != noErr) {
		*vRefNumP = 0;
		*dirIDP = 0;
	}
}

void
GetSystemSubfolder(OSType folder, bool create, short *vRefNumP, long *dirIDP)
{
	bool hasFolderMgr = false;
	long feature;

	if (TrapAvailable(_GestaltDispatch) &&
	    Gestalt(gestaltFindFolderAttr, &feature) == noErr)
		hasFolderMgr = true;

	if (!hasFolderMgr) {
		GetSystemFolder(vRefNumP, dirIDP);
		return;
	}

	if (FindFolder(kOnSystemDisk, folder, create, vRefNumP,
	    dirIDP) != noErr) {
		*vRefNumP = 0;
		*dirIDP = 0;
	}
}
