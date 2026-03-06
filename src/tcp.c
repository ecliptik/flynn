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
#include "tcp.h"

#define RCV_BUFFER_SIZE 1024
#define TCP_BUFFER_SIZE 8192
#define OPEN_TIMEOUT 60

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
_TCPGetOurIP(ip_addr *ip, long *netMask)
{
	OSErr osErr;
	GetAddrParamBlock pb;

	memset(&pb, 0, sizeof(pb));
	
	pb.csCode = ipctlGetAddr;
	pb.ioCRefNum = gIPPDriverRefNum;
	pb.ioResult = 1;
	
	osErr = PBControl((ParmBlkPtr)&pb, true);
	while (pb.ioResult > 0)
		;
	
	if (pb.ioResult != noErr)
		return pb.ioResult;

	if (ip != NULL)
		*ip = pb.ourAddress;
	if (netMask != NULL)
		*netMask = pb.ourNetMask;

	return osErr;
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

/* listen for an incoming connection */
OSErr
_TCPPassiveOpen(TCPiopb *pb, StreamPtr stream, ip_addr *remoteIP,
  tcp_port *remotePort, ip_addr *localIP, tcp_port *localPort,
  Ptr userData, TCPIOCompletionProc ioCompletion, Boolean async)
{
	OSErr osErr;

	memset(pb, 0, sizeof(*pb));

	pb->csCode = TCPPassiveOpen;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.open.ulpTimeoutAction = 1;	/* abort half-open connection */
	pb->csParam.open.ulpTimeoutValue = 5;	/*  after 5 seconds */
	pb->csParam.open.validityFlags = 0xC0;	
	pb->csParam.open.commandTimeoutValue = 0;	
	pb->csParam.open.remoteHost = 0;	
	pb->csParam.open.remotePort = 0;	
	pb->csParam.open.localHost = 0;	
	pb->csParam.open.localPort = *localPort;	
	pb->csParam.open.tosFlags = 0x1;		/* low delay */
	pb->csParam.open.precedence = 0;	
	pb->csParam.open.dontFrag = 0;	
	pb->csParam.open.timeToLive = 0;
	pb->csParam.open.security = 0;	
	pb->csParam.open.optionCnt = 0;
	pb->csParam.open.userDataPtr = userData;
	
	osErr = PBControl((ParmBlkPtr) pb, async);
	if (!async && (osErr == noErr)) {
		if (remoteIP)
			*remoteIP = pb->csParam.open.remoteHost;	
		if (remotePort)
			*remotePort	= pb->csParam.open.remotePort;	
		if (localIP)
			*localIP = pb->csParam.open.localHost;	
		*localPort = pb->csParam.open.localPort;
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
#if 0
	/* not available with this csCode */
	pb->csParam.open.commandTimeoutValue = 30;
#endif
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
	memset(pb, 0, sizeof(*pb));

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
_TCPNoCopyRcv(TCPiopb *pb, StreamPtr stream, Ptr rdsPtr,
  unsigned short rdsLength, Ptr userData, TCPIOCompletionProc ioCompletion,
  Boolean async)
{
	memset(pb, 0, sizeof(*pb));

	pb->csCode = TCPNoCopyRcv;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.receive.commandTimeoutValue = 30;
	pb->csParam.receive.urgentFlag = 0;
	pb->csParam.receive.markFlag = 0;
	pb->csParam.receive.rdsPtr = rdsPtr;
	pb->csParam.receive.rdsLength = rdsLength;
	pb->csParam.receive.userDataPtr = userData;
	
	return PBControl((ParmBlkPtr)pb, async);
}

OSErr
_TCPRcv(TCPiopb *pb, StreamPtr stream, Ptr rcvBufPtr,
  unsigned short *rcvBufLen, Ptr userData, TCPIOCompletionProc ioCompletion,
  Boolean async)
{
	OSErr osErr;
	
	memset(pb, 0, sizeof(*pb));
	
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
_TCPBfrReturn(TCPiopb *pb, StreamPtr stream, Ptr rdsPtr, Ptr userData,
  TCPIOCompletionProc ioCompletion, Boolean async)
{
	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = TCPRcvBfrReturn;
	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->tcpStream = stream;
	pb->ioResult = 1;
	
	pb->csParam.receive.rdsPtr = rdsPtr;
	pb->csParam.receive.userDataPtr = userData;
	
	return PBControl((ParmBlkPtr)pb, async);
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

	memset(pb, 0, sizeof(*pb));

	pb->ioCompletion = ioCompletion;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->ioResult = 1;
	pb->csCode = TCPStatus;
	pb->tcpStream = stream;
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
_UDPMaxMTUSize(UDPiopb *pb, short *mtu)
{
	OSErr osErr;
	
	memset(pb, 0, sizeof(*pb));
	
	pb->csCode = UDPMaxMTUSize;
	pb->ioCRefNum = gIPPDriverRefNum;
	pb->ioResult = 1;
	
	pb->csParam.mtu.remoteHost = (ip_addr)0;
	
	osErr = PBControl((ParmBlkPtr)pb, false);

	if (osErr == noErr)
		*mtu = pb->csParam.mtu.mtuSize;
	
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
	
	//pb->csParam.status.userDataPtr = userData;
	
	osErr = PBControl((ParmBlkPtr)pb, async);

	return osErr;
}


/* convenience functions */

pascal void
StrToAddrMarkDone(struct hostInfo *hi, char *data)
{
	volatile int *done = (int *)data;
	*done = 1;
}

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

void
long2ip(unsigned long num, char *ip)
{
	unsigned char *tmp = (unsigned char *)&num;
	sprintf(ip, "%d.%d.%d.%d", tmp[0], tmp[1], tmp[2], tmp[3]);
}

#define SOCKS_VERSION_SOCKS5 0x5
#define SOCKS_METHOD_AUTH_NONE 0x0
#define SOCKS_REQUEST_CONNECT 0x1
#define SOCKS_REQUEST_ATYP_DOMAINNAME 0x3
#define SOCKS_REPLY_SUCCESS 0x0

OSErr
SOCKS5TCPActiveOpen(TCPiopb *pb, StreamPtr stream, ip_addr socks_ip,
  tcp_port socks_port, char *remote_host, tcp_port remote_port,
  ip_addr *local_ip, tcp_port *local_port, Ptr user_data,
  TCPIOCompletionProc io_completion, Boolean async)
{
	OSErr err;
	TCPStatusPB status_pb;
	wdsEntry wds[2];
	char data[255] = { 0 };
	unsigned short len, remote_host_len;
	
	remote_host_len = strlen(remote_host);
	if (remote_host_len + 7 > sizeof(data))
		return -1;

	err = _TCPActiveOpen(pb, stream, socks_ip, socks_port, local_ip,
	  local_port, user_data, io_completion, async);
	if (err != noErr)
		return err;
	
	data[0] = SOCKS_VERSION_SOCKS5;
	data[1] = 1; /* nmethods */
	data[2] = SOCKS_METHOD_AUTH_NONE;

	memset(&wds, 0, sizeof(wds));
	wds[0].ptr = (Ptr)&data;
	wds[0].length = 3;
	
	err = _TCPSend(pb, stream, wds, nil, nil, false);
	if (err)
		goto fail;
	
	for (;;) {
		err = _TCPStatus(pb, stream, &status_pb, nil, nil, false);
		if (err != noErr)
			goto fail;
		
		if (status_pb.amtUnreadData >= 2)
			break;
	}
	
	len = 2;
	err = _TCPRcv(pb, stream, (Ptr)&data, &len, nil, nil, false);
	if (err != noErr)
		goto fail;
	
	if (data[0] != SOCKS_VERSION_SOCKS5 || data[1] != SOCKS_METHOD_AUTH_NONE)
		goto fail;
	
	len = 0;
	data[len++] = SOCKS_VERSION_SOCKS5;
	data[len++] = SOCKS_REQUEST_CONNECT;
	data[len++] = 0; /* reserved */
	data[len++] = SOCKS_REQUEST_ATYP_DOMAINNAME;
	data[len++] = remote_host_len;
	memcpy(data + len, remote_host, remote_host_len);
	len += remote_host_len;
	data[len++] = (remote_port >> 8);
	data[len++] = (remote_port & 0xff);
	
	memset(&wds, 0, sizeof(wds));
	wds[0].ptr = (Ptr)&data;
	wds[0].length = len;
	
	err = _TCPSend(pb, stream, wds, nil, nil, false);
	if (err)
		goto fail;

	for (;;) {
		err = _TCPStatus(pb, stream, &status_pb, nil, nil, false);
		if (err != noErr)
			goto fail;
		
		if (status_pb.amtUnreadData >= 7)
			break;
	}
	
	len = status_pb.amtUnreadData;
	if (len > sizeof(data))
		len = sizeof(data);
	err = _TCPRcv(pb, stream, (Ptr)&data, &len, nil, nil, false);
	if (err != noErr)
		goto fail;
	
	if (data[0] != SOCKS_VERSION_SOCKS5 || data[1] != SOCKS_REPLY_SUCCESS)
		goto fail;
	
	return noErr;
	
fail:
	_TCPClose(pb, stream, nil, nil, false);
	return err;
}
