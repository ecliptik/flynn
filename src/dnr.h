/* 
	AddressXlation.h		
	MacTCP name to address translation routines.

    Copyright Apple Computer, Inc. 1988 
    All rights reserved
	
*/	

#ifndef __ADDRESSXLATION__
#define __ADDRESSXLATION__

#include "MacTCP.h"

#define NUM_ALT_ADDRS	4

struct hostInfo {
	int _pad; /* XXX: i don't know why this is needed, but without it,
			   * StrToAddrProcPtr() returns everything shifted 2 bytes */
	int	rtnCode;
	char cname[255];
	unsigned long addr[NUM_ALT_ADDRS];
};

enum AddrClasses {
	A = 1,
	NS,
	CNAME = 5,
	lastClass = 65535
}; 

struct cacheEntryRecord {
	char *cname;
	unsigned short type;
	enum AddrClasses class;
	unsigned long ttl;
	union {
		char *name;
		ip_addr addr;
	} rdata;
};

typedef pascal void (*EnumResultProcPtr)(struct cacheEntryRecord *cacheEntryRecordPtr,
  char *userDataPtr);
typedef pascal void (*ResultProcPtr)(struct hostInfo *hostInfoPtr,
  char *userDataPtr);

extern OSErr OpenResolver(char *fileName);
extern OSErr StrToAddr(char *hostName, struct hostInfo *hostInfoPtr,
  ResultProcPtr resultProc, char *userDataPtr);
extern OSErr AddrToStr(unsigned long addr, char *addrStr);
extern OSErr EnumCache(EnumResultProcPtr enumResultProc, char *userDataPtr);
extern OSErr AddrToName(ip_addr addr, struct hostInfo *hostInfoPtr,
  ResultProcPtr resultProc, char *userDataPtr);
extern OSErr CloseResolver(void);

/*
 * The above functions call into the dnr resource and pass the function id
 * and function-specific arguments, so the compiler needs to know the types
 */ 
typedef OSErr (*OpenResolverProcPtr)(UInt32, char *);
typedef OSErr (*CloseResolverProcPtr)(UInt32);
typedef OSErr (*StrToAddrProcPtr)(UInt32, char *hostName,
  struct hostInfo *hostInfoPtr, ResultProcPtr resultProc,
  char *userDataPtr);
typedef OSErr (*AddrToStrProcPtr)(UInt32, unsigned long addr,
  char *addrStr);
typedef OSErr (*EnumCacheProcPtr)(UInt32, EnumResultProcPtr enumResultProc,
  char *userDataPtr);
typedef OSErr (*AddrToNameProcPtr)(UInt32, ip_addr addr,
  struct hostInfo *hostInfoPtr, ResultProcPtr resultProc,
  char *userDataPtr);

#endif