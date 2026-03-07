/*
 * Copyright (c) 1990-1992 by the University of Illinois Board of Trustees
 */

#include "MacTCP.h"
#include "dnr.h"

#ifndef __TCP_H__
#define __TCP_H__

typedef struct
{
	Handle next;
	struct hostInfo hi;
} HostInfoQ, *HostInfoQPtr, **HostInfoQHandle;

typedef ProcPtr TCPNotifyProc;
typedef ProcPtr UDPNotifyProc;
typedef void (*TCPIOCompletionProc)(struct TCPiopb *iopb);
typedef void (*UDPIOCompletionProc)(struct UDPiopb *iopb);

OSErr _TCPInit(void);
OSErr _TCPGetOurIP(ip_addr *ip, long *netMask);
OSErr _TCPCreate(TCPiopb *pb, StreamPtr *stream, Ptr rcvBufPtr,
  long rcvBufLen, TCPNotifyProc aNotifyProc, Ptr userDataPtr,
  TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPPassiveOpen(TCPiopb *pb, StreamPtr stream, ip_addr *remoteIP,
  tcp_port *remotePort, ip_addr *localIP, tcp_port *localPort,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPActiveOpen(TCPiopb *pb, StreamPtr stream, ip_addr remoteIP,
  tcp_port remotePort, ip_addr *localIP, tcp_port *localPort,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPSend(TCPiopb *pb, StreamPtr stream, wdsEntry *wdsPtr,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPNoCopyRcv(TCPiopb *pb, StreamPtr stream, Ptr rdsPtr,
  unsigned short rdsLength, Ptr userData, TCPIOCompletionProc ioCompletion,
  Boolean async);
OSErr _TCPRcv(TCPiopb *pb, StreamPtr stream, Ptr rcvBufPtr,
  unsigned short *rcvBufLen, Ptr userData, TCPIOCompletionProc ioCompletion,
  Boolean async);
OSErr _TCPBfrReturn(TCPiopb *pb, StreamPtr stream, Ptr rdsPtr, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPClose(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPAbort(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPStatus(TCPiopb *pb, StreamPtr stream, struct TCPStatusPB *status,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPRelease(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async);

OSErr _UDPMaxMTUSize(UDPiopb *pb, short *mtu);
OSErr _UDPCreate(UDPiopb *pb, StreamPtr *stream, Ptr rcvBufPtr,
  long rcvBufLen, UDPNotifyProc aNotifyProc, Ptr userDataPtr,
  UDPIOCompletionProc ioCompletion, Boolean async);
OSErr _UDPSend(UDPiopb *pb, StreamPtr stream, wdsEntry *wdsPtr,
  ip_addr remoteIP, udp_port remotePort, Ptr userData,
  UDPIOCompletionProc ioCompletion, Boolean async);
OSErr _UDPRcv(UDPiopb *pb, StreamPtr stream, unsigned short timeout,
  Ptr userData, UDPIOCompletionProc ioCompletion, Boolean async);
OSErr _UDPBfrReturn(UDPiopb *pb, StreamPtr stream, Ptr rcvBuff,
  Ptr userData, UDPIOCompletionProc ioCompletion, Boolean async);
OSErr _UDPRelease(UDPiopb *pb, StreamPtr stream, Ptr userData,
  UDPIOCompletionProc ioCompletion, Boolean async);

OSErr DNSResolveName(char *name, unsigned long *ipAddress,
  void (*yielder)(void));

pascal void StrToAddrMarkDone(struct hostInfo *hi, char *data);

unsigned long ip2long(char *ip);
void long2ip(unsigned long num, char *ip);

OSErr SOCKS5TCPActiveOpen(TCPiopb *pb, StreamPtr stream, ip_addr socks_ip,
  tcp_port socks_port, char *remote_host, tcp_port remote_port,
  ip_addr *local_ip, tcp_port *local_port, Ptr user_data,
  TCPIOCompletionProc io_completion, Boolean async);

#endif
