/*
 * sysutil.h - Minimal system utilities for DNR support
 */

#ifndef SYSUTIL_H
#define SYSUTIL_H

#include <stdbool.h>

/* Boot volume constant — not in Retro68 Multiversal headers */
#ifndef kOnSystemDisk
#define kOnSystemDisk	((short)0x8000)
#endif

/* _GestaltDispatch trap number (0xA0AD) — Retro68 uses _Gestalt */
#ifndef _GestaltDispatch
#define _GestaltDispatch _Gestalt
#endif

TrapType GetTrapType(unsigned long theTrap);
bool TrapAvailable(unsigned long trap);
void get_machine_name(char *buf, short buflen);

#endif /* SYSUTIL_H */
