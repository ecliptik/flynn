/*
 * Copyright (c) 1990-1992 by the University of Illinois Board of Trustees
 */

#include "MacTCP.h"

#ifndef __TCP_H__
#define __TCP_H__

typedef TCPNotifyUPP TCPNotifyProc;
typedef UDPNotifyUPP UDPNotifyProc;
typedef TCPIOCompletionUPP TCPIOCompletionProc;
typedef UDPIOCompletionUPP UDPIOCompletionProc;

OSErr _TCPInit(void);
OSErr _TCPCreate(TCPiopb *pb, StreamPtr *stream, Ptr rcvBufPtr,
  long rcvBufLen, TCPNotifyProc aNotifyProc, Ptr userDataPtr,
  TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPActiveOpen(TCPiopb *pb, StreamPtr stream, ip_addr remoteIP,
  tcp_port remotePort, ip_addr *localIP, tcp_port *localPort,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPSend(TCPiopb *pb, StreamPtr stream, wdsEntry *wdsPtr,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPRcv(TCPiopb *pb, StreamPtr stream, Ptr rcvBufPtr,
  unsigned short *rcvBufLen, Ptr userData, TCPIOCompletionProc ioCompletion,
  Boolean async);
OSErr _TCPClose(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPAbort(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPStatus(TCPiopb *pb, StreamPtr stream, struct TCPStatusPB *status,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async);
OSErr _TCPRelease(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async);

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

unsigned long ip2long(char *ip);

#endif
