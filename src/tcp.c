/*
 * Copyright (c) 2020 joshua stein <jcs@jcs.org>
 * Copyright (c) 1990-1992 by the University of Illinois Board of Trustees
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OSUtils.h>
#include "tcp.h"

/* Retro68 compatibility: PBControl/PBOpen -> Sync/Async variants */
#define PBControl(pb, async) ((async) ? PBControlAsync(pb) : PBControlSync(pb))
#define PBOpen(pb, async) ((async) ? PBOpenAsync(pb) : PBOpenSync(pb))

short gIPPDriverRefNum;

static short _TCPAtexitInstalled = 0;
static StreamPtr _TCPStreams[10] = { 0 };

void _TCPAtexit(void);

OSErr
_TCPInit(void)
{
	ParamBlockRec pb;
	OSErr osErr;
	
	memset(&pb, 0, sizeof(pb));

	gIPPDriverRefNum = -1;

	pb.ioParam.ioCompletion = 0; 
	pb.ioParam.ioNamePtr = "\p.IPP"; 
	pb.ioParam.ioPermssn = fsCurPerm;
	
	osErr = PBOpen(&pb, false);
	if (noErr == osErr)
		gIPPDriverRefNum = pb.ioParam.ioRefNum;
	
	if (!_TCPAtexitInstalled) {
		atexit(_TCPAtexit);
		_TCPAtexitInstalled = 1;
	}
	
	return osErr;
}

void
_TCPAtexit(void)
{
	short n;
	TCPiopb pb = { 0 };
	
	for (n = 0; n < (sizeof(_TCPStreams) / sizeof(_TCPStreams[0])); n++) {
		if (_TCPStreams[n] != 0) {
			_TCPAbort(&pb, _TCPStreams[n], nil, nil, false);
			_TCPRelease(&pb, _TCPStreams[n], nil, nil, false);
			_TCPStreams[n] = 0;
		}
	}
}

OSErr
_TCPCreate(TCPiopb *pb, StreamPtr *stream, Ptr rcvBufPtr, long rcvBufLen,
  TCPNotifyProc aNotifyProc, Ptr userDataPtr,
  TCPIOCompletionProc ioCompletion, Boolean async)
{
	OSErr osErr;
	short n;
	
	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = TCPCreate;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->ioResult = 1;
	
	pb->csParam.create.rcvBuff = rcvBufPtr;	
	pb->csParam.create.rcvBuffLen = rcvBufLen;	
	pb->csParam.create.notifyProc = aNotifyProc;
	pb->csParam.create.userDataPtr = userDataPtr;
	
	osErr = PBControl((ParmBlkPtr)pb, async);
	if (!async && (noErr == osErr)) {
		*stream	= pb->tcpStream;

		for (n = 0; n < (sizeof(_TCPStreams) / sizeof(_TCPStreams[0])); n++) {
			if (_TCPStreams[n] == 0) {
				_TCPStreams[n] = pb->tcpStream;
				break;
			}
		}
	}
		
	return osErr;
}

/* make an outgoing connection */
OSErr
_TCPActiveOpen(TCPiopb *pb, StreamPtr stream, ip_addr remoteIP,
  tcp_port remotePort, ip_addr *localIP, tcp_port *localPort,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async)
{
	OSErr osErr;
	short index;

	memset(pb, 0, sizeof(*pb));

	pb->csCode = TCPActiveOpen;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.open.ulpTimeoutValue = 10;
	pb->csParam.open.ulpTimeoutAction = 1;
	pb->csParam.open.validityFlags = 0xC0;
	pb->csParam.open.commandTimeoutValue = 30;
	pb->csParam.open.remoteHost = remoteIP;	
	pb->csParam.open.remotePort	= remotePort;	
	pb->csParam.open.localHost = 0;	
	pb->csParam.open.localPort = *localPort;	
	pb->csParam.open.tosFlags = 0;	
	pb->csParam.open.precedence = 0;	
	pb->csParam.open.dontFrag = 0;	
	pb->csParam.open.timeToLive = 0;
	pb->csParam.open.security = 0;	
	pb->csParam.open.optionCnt = 0;
	for (index = 0; index < sizeof(pb->csParam.open.options); ++index)	
		pb->csParam.open.options[index] = 0;	
	pb->csParam.open.userDataPtr = userData;
	
	osErr = PBControl((ParmBlkPtr) pb, async);
	if (!async && (osErr == noErr)) {
		*localIP = pb->csParam.open.localHost;	
		*localPort = pb->csParam.open.localPort;
	}
	
	return osErr;
}

OSErr
_TCPSend(TCPiopb *pb, StreamPtr stream, wdsEntry *wdsPtr, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async)
{
	pb->csCode = TCPSend;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.send.ulpTimeoutValue = 30;
	pb->csParam.send.ulpTimeoutAction = 1;
	pb->csParam.send.validityFlags = 0xC0;
	pb->csParam.send.pushFlag = 1; /* XXX */
	pb->csParam.send.urgentFlag = 0;
	pb->csParam.send.wdsPtr	= (Ptr)wdsPtr;
	pb->csParam.send.sendFree = 0;
	pb->csParam.send.sendLength = 0;
	pb->csParam.send.userDataPtr = userData;
	
	return PBControl((ParmBlkPtr)pb, async);
}

OSErr
_TCPRcv(TCPiopb *pb, StreamPtr stream, Ptr rcvBufPtr,
  unsigned short *rcvBufLen, Ptr userData, TCPIOCompletionProc ioCompletion,
  Boolean async)
{
	OSErr osErr;

	pb->csCode = TCPRcv;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.receive.commandTimeoutValue = 1;
	pb->csParam.receive.urgentFlag = 0;
	pb->csParam.receive.markFlag = 0;
	pb->csParam.receive.rcvBuff	= rcvBufPtr;
	pb->csParam.receive.rcvBuffLen = *rcvBufLen;
	pb->csParam.receive.userDataPtr = userData;
	
	osErr = PBControl((ParmBlkPtr)pb, async);
	if (!async)
		*rcvBufLen = pb->csParam.receive.rcvBuffLen;
		
	return osErr;
}

OSErr
_TCPClose(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async)
{
	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = TCPClose;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.close.ulpTimeoutValue = 30;
	pb->csParam.close.ulpTimeoutAction = 1;
	pb->csParam.close.validityFlags = 0xC0;
	pb->csParam.close.userDataPtr = userData;
	
	return PBControl((ParmBlkPtr)pb, async);
}

OSErr
_TCPAbort(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async)
{
	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = TCPAbort;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.abort.userDataPtr = userData;
	
	return PBControl((ParmBlkPtr)pb, async);
}

OSErr
_TCPStatus(TCPiopb *pb, StreamPtr stream, struct TCPStatusPB *status,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async)
{
	OSErr osErr;

	pb->ioCRefNum = gIPPDriverRefNum;
	pb->csCode = TCPStatus;
	pb->tcpStream = stream;
	pb->ioCompletion = ioCompletion;
	pb->ioResult = 1;
	pb->csParam.status.userDataPtr = userData;
	
	osErr = PBControl((ParmBlkPtr)pb, async);
	if (!async && (noErr == osErr)) {
		*status = pb->csParam.status;	
	}

	return osErr;
}

OSErr
_TCPRelease(TCPiopb *pb, StreamPtr stream, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async)
{
	OSErr osErr;
	short n;

	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = TCPRelease;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.status.userDataPtr = userData;
	
	osErr = PBControl((ParmBlkPtr)pb, async);

	for (n = 0; n < (sizeof(_TCPStreams) / sizeof(_TCPStreams[0])); n++) {
		if (_TCPStreams[n] == stream) {
			_TCPStreams[n] = 0;
			break;
		}
	}

	return osErr;
}

OSErr
_UDPCreate(UDPiopb *pb, StreamPtr *stream, Ptr rcvBufPtr, long rcvBufLen,
  UDPNotifyProc aNotifyProc, Ptr userDataPtr,
  UDPIOCompletionProc ioCompletion, Boolean async)
{
	OSErr osErr;
	
	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = UDPCreate;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->ioResult = 1;
	
	pb->csParam.create.rcvBuff = rcvBufPtr;	
	pb->csParam.create.rcvBuffLen = rcvBufLen;	
	pb->csParam.create.notifyProc = aNotifyProc;
	pb->csParam.create.userDataPtr = userDataPtr;
	
	osErr = PBControl((ParmBlkPtr)pb, async);
	if (!async && (noErr == osErr)) {
		*stream	= pb->udpStream;
	}
		
	return osErr;
}

OSErr
_UDPSend(UDPiopb *pb, StreamPtr stream, wdsEntry *wdsPtr, ip_addr remoteIP,
  udp_port remotePort, Ptr userData, UDPIOCompletionProc ioCompletion,
  Boolean async)
{
	memset(pb, 0, sizeof(*pb));

	pb->csCode = UDPWrite;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->udpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.send.remoteHost = remoteIP;
	pb->csParam.send.remotePort = remotePort;
	pb->csParam.send.wdsPtr	= (Ptr)wdsPtr;
	pb->csParam.send.checkSum = 0;
	pb->csParam.send.sendLength = 0;
	pb->csParam.send.userDataPtr = userData;
	
	return PBControl((ParmBlkPtr)pb, async);
}

OSErr
_UDPRcv(UDPiopb *pb, StreamPtr stream, unsigned short timeout,
  Ptr userData, UDPIOCompletionProc ioCompletion, Boolean async)
{
	memset(pb, 0, sizeof(*pb));

	pb->csCode = UDPRead;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->udpStream = stream;
	pb->ioResult = 1;

	pb->csParam.receive.timeOut = timeout;
	pb->csParam.receive.userDataPtr = userData;

	return PBControl((ParmBlkPtr)pb, async);
}

OSErr
_UDPBfrReturn(UDPiopb *pb, StreamPtr stream, Ptr rcvBuff,
  Ptr userData, UDPIOCompletionProc ioCompletion, Boolean async)
{
	memset(pb, 0, sizeof(*pb));

	pb->csCode = UDPBfrReturn;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->udpStream = stream;
	pb->ioResult = 1;

	pb->csParam.receive.rcvBuff = rcvBuff;
	pb->csParam.receive.userDataPtr = userData;

	return PBControl((ParmBlkPtr)pb, async);
}

OSErr
_UDPRelease(UDPiopb *pb, StreamPtr stream, Ptr userData,
  UDPIOCompletionProc ioCompletion, Boolean async)
{
	OSErr osErr;
	
	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = UDPRelease;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->udpStream = stream;
	pb->ioResult = 1;
	
	osErr = PBControl((ParmBlkPtr)pb, async);

	return osErr;
}


/* convenience functions */

unsigned long
ip2long(char *ip)
{
	unsigned long address = 0;
	short dotcount = 0, i;
	unsigned short b = 0;
	
	for (i = 0; ip[i] != 0; i++) {
		if (ip[i] == '.') {
			if (++dotcount > 3)
				return (0);
			address <<= 8;
			address |= b;
			b = 0;
		} else if (ip[i] >= '0' && ip[i] <= '9') {
			b *= 10;
			b += (ip[i] - '0');
			if (b > 255)
				return (0);
		} else
			return (0);
	}
	
	if (dotcount != 3)
		return (0);
	address <<= 8;
	address |= b;
	return address;
}


