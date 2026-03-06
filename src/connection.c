/*
 * connection.c - TCP connection management for Telnet client
 */

#include <Quickdraw.h>
#include <Dialogs.h>
#include <TextEdit.h>
#include <Events.h>
#include <Memory.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Resources.h>
#include <Multiverse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MacTCP.h"
#include "tcp.h"
#include "connection.h"

OSErr
conn_init(void)
{
	return _TCPInit();
}

Boolean
conn_open_dialog(Connection *conn)
{
	DialogPtr dlg;
	short item_hit;
	Handle item_h;
	short item_type;
	Rect item_rect;
	Str255 host_str, port_str;
	long port_num;
	OSErr err;
	unsigned long ip;
	short i;

	if (conn->state != CONN_STATE_IDLE) {
		SysBeep(10);
		return false;
	}

	dlg = GetNewDialog(DLOG_CONNECT_ID, 0L, (WindowPtr)-1L);
	if (!dlg) {
		SysBeep(10);
		return false;
	}

	/* Pre-fill with last-used values */
	if (conn->host[0]) {
		GetDItem(dlg, DLOG_HOST_FIELD, &item_type, &item_h, &item_rect);
		/* Convert C string to Pascal string */
		host_str[0] = strlen(conn->host);
		for (i = 0; i < host_str[0]; i++)
			host_str[i + 1] = conn->host[i];
		SetIText(item_h, host_str);
	}
	if (conn->port > 0) {
		GetDItem(dlg, DLOG_PORT_FIELD, &item_type, &item_h, &item_rect);
		sprintf((char *)&port_str[1], "%d", conn->port);
		port_str[0] = strlen((char *)&port_str[1]);
		SetIText(item_h, port_str);
	}

	ShowWindow(dlg);

	for (;;) {
		ModalDialog(0L, &item_hit);

		if (item_hit == DLOG_CANCEL) {
			DisposDialog(dlg);
			return false;
		}

		if (item_hit == DLOG_OK)
			break;
	}

	/* Extract host */
	GetDItem(dlg, DLOG_HOST_FIELD, &item_type, &item_h, &item_rect);
	GetIText(item_h, host_str);
	if (host_str[0] == 0) {
		DisposDialog(dlg);
		return false;
	}
	for (i = 0; i < host_str[0] && i < 255; i++)
		conn->host[i] = host_str[i + 1];
	conn->host[i] = '\0';

	/* Extract port */
	GetDItem(dlg, DLOG_PORT_FIELD, &item_type, &item_h, &item_rect);
	GetIText(item_h, port_str);
	if (port_str[0] > 0) {
		/* Convert Pascal string to number */
		port_str[port_str[0] + 1] = '\0';
		StringToNum(port_str, &port_num);
		conn->port = (short)port_num;
	} else {
		conn->port = DEFAULT_PORT;
	}

	DisposDialog(dlg);

	/* Resolve hostname */
	conn->state = CONN_STATE_RESOLVING;

	/* Try as IP address first */
	ip = ip2long(conn->host);
	if (ip != 0) {
		conn->remote_ip = ip;
	} else {
		/* DNS lookup */
		err = DNSResolveName(conn->host, &conn->remote_ip, 0L);
		if (err != noErr) {
			conn->state = CONN_STATE_IDLE;
			ParamText("\pCould not resolve hostname", "\p", "\p", "\p");
			Alert(128, 0L);
			return false;
		}
	}

	conn->remote_port = conn->port;

	/* Allocate receive buffer */
	conn->rcv_buf = NewPtr(TCP_RCV_BUFSIZ);
	if (!conn->rcv_buf) {
		conn->state = CONN_STATE_IDLE;
		ParamText("\pOut of memory", "\p", "\p", "\p");
		Alert(128, 0L);
		return false;
	}

	/* Create TCP stream */
	err = _TCPCreate(&conn->pb, &conn->stream, conn->rcv_buf,
	    TCP_RCV_BUFSIZ, 0L, 0L, 0L, false);
	if (err != noErr) {
		conn->state = CONN_STATE_IDLE;
		DisposPtr(conn->rcv_buf);
		conn->rcv_buf = 0L;
		ParamText("\pFailed to create TCP stream", "\p", "\p", "\p");
		Alert(128, 0L);
		return false;
	}

	/* Initiate connection */
	conn->state = CONN_STATE_CONNECTING;
	conn->local_port = 0;

	err = _TCPActiveOpen(&conn->pb, conn->stream, conn->remote_ip,
	    conn->remote_port, &conn->local_ip, &conn->local_port,
	    0L, 0L, false);
	if (err != noErr) {
		conn->state = CONN_STATE_IDLE;
		_TCPRelease(&conn->pb, conn->stream, 0L, 0L, false);
		DisposPtr(conn->rcv_buf);
		conn->rcv_buf = 0L;
		conn->stream = 0L;
		ParamText("\pFailed to connect", "\p", "\p", "\p");
		Alert(128, 0L);
		return false;
	}

	conn->state = CONN_STATE_CONNECTED;
	conn->read_len = 0;
	return true;
}

void
conn_idle(Connection *conn)
{
	struct TCPStatusPB status;
	OSErr err;
	unsigned short len;

	if (conn->state != CONN_STATE_CONNECTED)
		return;

	err = _TCPStatus(&conn->pb, conn->stream, &status, 0L, 0L, false);
	if (err != noErr) {
		conn_close(conn);
		return;
	}

	/* Check if connection was closed by remote */
	if (status.connectionState == 14) {
		/* TIME_WAIT - remote closed */
		conn_close(conn);
		return;
	}

	if (status.amtUnreadData == 0)
		return;

	/* Read available data */
	len = status.amtUnreadData;
	if (len > TCP_READ_BUFSIZ)
		len = TCP_READ_BUFSIZ;

	err = _TCPRcv(&conn->pb, conn->stream, (Ptr)conn->read_buf,
	    &len, 0L, 0L, false);
	if (err != noErr) {
		conn_close(conn);
		return;
	}

	conn->read_len = len;
}

void
conn_close(Connection *conn)
{
	if (conn->state == CONN_STATE_IDLE)
		return;

	conn->state = CONN_STATE_CLOSING;

	if (conn->stream) {
		_TCPClose(&conn->pb, conn->stream, 0L, 0L, false);
		_TCPRelease(&conn->pb, conn->stream, 0L, 0L, false);
		conn->stream = 0L;
	}

	if (conn->rcv_buf) {
		DisposPtr(conn->rcv_buf);
		conn->rcv_buf = 0L;
	}

	conn->read_len = 0;
	conn->state = CONN_STATE_IDLE;
}

OSErr
conn_send(Connection *conn, char *data, short len)
{
	wdsEntry wds[2];

	if (conn->state != CONN_STATE_CONNECTED)
		return -1;

	memset(&wds, 0, sizeof(wds));
	wds[0].ptr = (Ptr)data;
	wds[0].length = len;

	return _TCPSend(&conn->pb, conn->stream, wds, 0L, 0L, false);
}

void
conn_status_str(Connection *conn, char *buf, short buflen)
{
	char ip_str[16];

	switch (conn->state) {
	case CONN_STATE_IDLE:
		strncpy(buf, "Not connected", buflen - 1);
		break;
	case CONN_STATE_RESOLVING:
		strncpy(buf, "Resolving...", buflen - 1);
		break;
	case CONN_STATE_CONNECTING:
		strncpy(buf, "Connecting...", buflen - 1);
		break;
	case CONN_STATE_CONNECTED:
		long2ip(conn->remote_ip, ip_str);
		sprintf(buf, "Connected to %s", ip_str);
		break;
	case CONN_STATE_CLOSING:
		strncpy(buf, "Closing...", buflen - 1);
		break;
	default:
		strncpy(buf, "Unknown", buflen - 1);
		break;
	}
	buf[buflen - 1] = '\0';
}
