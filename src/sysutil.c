/*
 * sysutil.c - Minimal system utilities for DNR support
 *
 * Extracted from wallops-146 util.c by jcs@jcs.org
 */

#include <Traps.h>
#include <OSUtils.h>
#include <Memory.h>
#include <Gestalt.h>
#include <string.h>

/* Retro68 Multiverse.h uses kOSTrapType/kToolboxTrapType instead of OSTrap/ToolTrap */
#ifndef OSTrap
#define OSTrap kOSTrapType
#define ToolTrap kToolboxTrapType
#endif

/* _GestaltDispatch trap number (0xA0AD) */
#ifndef _GestaltDispatch
#define _GestaltDispatch _Gestalt
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


/* Machine type constants not in Retro68 Multiverse.h */
#ifndef gestaltMacSE030
#define gestaltMacSE030		9
#endif
#ifndef gestaltQuadra900
#define gestaltQuadra900	20
#endif
#ifndef gestaltPowerBook170
#define gestaltPowerBook170	21
#endif
#ifndef gestaltQuadra700
#define gestaltQuadra700	22
#endif
#ifndef gestaltPowerBook100
#define gestaltPowerBook100	24
#endif
#ifndef gestaltQuadra950
#define gestaltQuadra950	26
#endif

void
get_machine_name(char *buf, short buflen)
{
	long resp;
	const char *name;

	if (!TrapAvailable(_GestaltDispatch) ||
	    Gestalt(gestaltMachineType, &resp) != noErr) {
		strncpy(buf, "Macintosh", buflen - 1);
		buf[buflen - 1] = '\0';
		return;
	}

	switch (resp) {
	case gestaltClassic:
		name = "Macintosh 128K";
		break;
	case gestaltMacPlus:
		name = "Macintosh Plus";
		break;
	case gestaltMacSE:
		name = "Macintosh SE";
		break;
	case gestaltMacII:
		name = "Macintosh II";
		break;
	case gestaltMacIIx:
		name = "Macintosh IIx";
		break;
	case gestaltMacIIcx:
		name = "Macintosh IIcx";
		break;
	case gestaltMacSE030:
		name = "Macintosh SE/30";
		break;
	case gestaltMacIIci:
		name = "Macintosh IIci";
		break;
	case gestaltMacIIfx:
		name = "Macintosh IIfx";
		break;
	case gestaltMacClassic:
		name = "Macintosh Classic";
		break;
	case gestaltMacIIsi:
		name = "Macintosh IIsi";
		break;
	case gestaltMacLC:
		name = "Macintosh LC";
		break;
	case gestaltQuadra900:
		name = "Quadra 900";
		break;
	case gestaltPowerBook170:
		name = "PowerBook 170";
		break;
	case gestaltQuadra700:
		name = "Quadra 700";
		break;
	case gestaltPowerBook100:
		name = "PowerBook 100";
		break;
	case gestaltQuadra950:
		name = "Quadra 950";
		break;
	default:
		name = "Macintosh";
		break;
	}

	strncpy(buf, name, buflen - 1);
	buf[buflen - 1] = '\0';
}
