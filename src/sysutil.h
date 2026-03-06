/*
 * sysutil.h - Minimal system utilities for DNR support
 */

#ifndef SYSUTIL_H
#define SYSUTIL_H

#include <stdbool.h>

TrapType GetTrapType(unsigned long theTrap);
bool TrapAvailable(unsigned long trap);
void GetSystemFolder(short *vRefNumP, long *dirIDP);
void GetSystemSubfolder(OSType folder, bool create, short *vRefNumP,
    long *dirIDP);

#endif /* SYSUTIL_H */
