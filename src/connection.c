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
#include "dns.h"
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
	Str255 host_str, port_str, user_str;
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
		GetDialogItem(dlg, DLOG_HOST_FIELD, &item_type, &item_h, &item_rect);
		/* Convert C string to Pascal string */
		host_str[0] = strlen(conn->host);
		for (i = 0; i < host_str[0]; i++)
			host_str[i + 1] = conn->host[i];
		SetDialogItemText(item_h, host_str);
	}
	if (conn->port > 0) {
		GetDialogItem(dlg, DLOG_PORT_FIELD, &item_type, &item_h, &item_rect);
		sprintf((char *)&port_str[1], "%d", conn->port);
		port_str[0] = strlen((char *)&port_str[1]);
		SetDialogItemText(item_h, port_str);
	}
	if (conn->username[0]) {
		GetDialogItem(dlg, DLOG_USER_FIELD, &item_type,
		    &item_h, &item_rect);
		user_str[0] = strlen(conn->username);
		for (i = 0; i < user_str[0]; i++)
			user_str[i + 1] = conn->username[i];
		SetDialogItemText(item_h, user_str);
	}

	ShowWindow(dlg);

	for (;;) {
		ModalDialog(0L, &item_hit);

		if (item_hit == DLOG_CANCEL) {
			DisposeDialog(dlg);
			return false;
		}

		if (item_hit == DLOG_OK)
			break;
	}

	/* Extract host */
	GetDialogItem(dlg, DLOG_HOST_FIELD, &item_type, &item_h, &item_rect);
	GetDialogItemText(item_h, host_str);
	if (host_str[0] == 0) {
		DisposeDialog(dlg);
		return false;
	}
	for (i = 0; i < host_str[0] && i < 255; i++)
		conn->host[i] = host_str[i + 1];
	conn->host[i] = '\0';

	/* Extract port */
	GetDialogItem(dlg, DLOG_PORT_FIELD, &item_type, &item_h, &item_rect);
	GetDialogItemText(item_h, port_str);
	if (port_str[0] > 0) {
		/* Convert Pascal string to number */
		port_str[port_str[0] + 1] = '\0';
		StringToNum(port_str, &port_num);
		conn->port = (short)port_num;
	} else {
		conn->port = DEFAULT_PORT;
	}

	/* Extract username (optional) */
	GetDialogItem(dlg, DLOG_USER_FIELD, &item_type, &item_h,
	    &item_rect);
	GetDialogItemText(item_h, user_str);
	if (user_str[0] > 0 && user_str[0] < 63) {
		for (i = 0; i < user_str[0]; i++)
			conn->username[i] = user_str[i + 1];
		conn->username[i] = '\0';
	} else {
		conn->username[0] = '\0';
	}

	DisposeDialog(dlg);

	return conn_connect(conn, conn->host, conn->port);
}

static Boolean
conn_validate_host(const char *host)
{
	short len, i, label_len;
	char c;
	Boolean has_alpha = false;

	len = strlen(host);

	/* Empty or too long (DNS max 253) */
	if (len == 0 || len > 253)
		return false;

	/* Validate each character and label length */
	label_len = 0;
	for (i = 0; i < len; i++) {
		c = host[i];
		if (c == '.') {
			if (label_len == 0)
				return false;  /* empty label (.foo, foo..bar) */
			label_len = 0;
		} else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
			has_alpha = true;
			label_len++;
			if (label_len > 63)
				return false;  /* label too long */
		} else if ((c >= '0' && c <= '9') || c == '-') {
			label_len++;
			if (label_len > 63)
				return false;  /* label too long */
		} else {
			return false;  /* invalid character */
		}
	}

	/* Must have at least one character in last label */
	if (label_len == 0)
		return false;

	/* If no letters (looks like an IP), validate as dotted quad */
	if (!has_alpha) {
		if (ip2long((char *)host) == 0)
			return false;
	}

	return true;
}

Boolean
conn_connect(Connection *conn, const char *host, short port)
{
	OSErr err;
	unsigned long ip;

	if (conn->state != CONN_STATE_IDLE) {
		SysBeep(10);
		return false;
	}

	/* Only copy if host is a different buffer (avoid UB from overlapping strncpy) */
	if (host != conn->host) {
		strncpy(conn->host, host, sizeof(conn->host) - 1);
		conn->host[sizeof(conn->host) - 1] = '\0';
	}
	conn->port = port;

	/* Validate hostname before attempting resolution */
	if (!conn_validate_host(conn->host)) {
		ParamText("\pInvalid hostname or IP address", "\p", "\p", "\p");
		Alert(128, 0L);
		return false;
	}

	/* Resolve hostname */
	conn->state = CONN_STATE_RESOLVING;

	/* Try as IP address first */
	ip = ip2long(conn->host);
	if (ip != 0) {
		conn->remote_ip = ip;
	} else {
		/* DNS lookup via UDP */
		short dns_err = dns_resolve(conn->host, &ip,
		    conn->dns_server);
		switch (dns_err) {
		case DNS_OK:
			conn->remote_ip = ip;
			break;
		case DNS_ERR_NXDOMAIN:
			conn->state = CONN_STATE_IDLE;
			ParamText("\pHost not found", "\p", "\p", "\p");
			Alert(128, 0L);
			return false;
		case DNS_ERR_TIMEOUT:
			conn->state = CONN_STATE_IDLE;
			ParamText("\pDNS lookup timed out",
			    "\p", "\p", "\p");
			Alert(128, 0L);
			return false;
		default:
			conn->state = CONN_STATE_IDLE;
			ParamText("\pDNS lookup failed", "\p", "\p", "\p");
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
		DisposePtr(conn->rcv_buf);
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
		DisposePtr(conn->rcv_buf);
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
		DisposePtr(conn->rcv_buf);
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
