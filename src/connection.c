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

#include <Windows.h>
#include <Fonts.h>

#include "MacTCP.h"
#include "tcp.h"
#include "dns.h"
#include "connection.h"
#include "dialogs.h"
#include "macutil.h"

/* TCP connection state for TIME_WAIT */
#define TCP_STATE_TIME_WAIT 14

static Boolean tcp_initialized = false;

static OSErr
conn_ensure_tcp(void)
{
	OSErr err;
	if (tcp_initialized)
		return noErr;
	err = _TCPInit();
	if (err == noErr)
		tcp_initialized = true;
	return err;
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

/*
 * conn_fail - common error cleanup for conn_connect failures.
 * Resets state to idle, restores cursor, shows alert.
 */
static Boolean
conn_fail(Connection *conn, const char *msg)
{
	Str255 pmsg;
	short len, i;

	conn->state = CONN_STATE_IDLE;
	InitCursor();

	len = strlen(msg);
	if (len > 255) len = 255;
	pmsg[0] = len;
	for (i = 0; i < len; i++)
		pmsg[i + 1] = msg[i];
	ParamText(pmsg, "\p", "\p", "\p");
	StopAlert(128, 0L);
	return false;
}

/*
 * conn_resolve_host - validate and resolve hostname to IP.
 * Sets conn->remote_ip on success.
 * Returns true on success, false on failure (with alert shown).
 */
static Boolean
conn_resolve_host(Connection *conn, WindowPtr status_win)
{
	unsigned long ip;
	char status_msg[80];

	/* Lazy MacTCP init on first connect */
	{
		OSErr tcp_err = conn_ensure_tcp();
		if (tcp_err != noErr)
			return conn_fail(conn, "MacTCP is not available");
	}

	/* Validate hostname before attempting resolution */
	if (!conn_validate_host(conn->host))
		return conn_fail(conn, "Invalid hostname or IP address");

	conn->state = CONN_STATE_RESOLVING;

	/* Show watch cursor during blocking DNS/TCP operations */
	SetCursor(*GetCursor(watchCursor));

	/* Try as IP address first */
	ip = ip2long(conn->host);
	if (ip != 0) {
		conn->remote_ip = ip;
	} else {
		/* Show DNS resolution status */
		snprintf(status_msg, sizeof(status_msg),
		    "Resolving %.50s\311", conn->host);
		conn_status_update(status_win, status_msg);

		/* DNS lookup via UDP */
		{
			short dns_err = dns_resolve(conn->host, &ip,
			    conn->dns_server);
			switch (dns_err) {
			case DNS_OK:
				conn->remote_ip = ip;
				break;
			case DNS_ERR_NXDOMAIN:
				return conn_fail(conn, "Host not found");
			case DNS_ERR_TIMEOUT:
				return conn_fail(conn,
				    "DNS lookup timed out");
			default:
				return conn_fail(conn,
				    "DNS lookup failed");
			}
		}
	}

	return true;
}

Boolean
conn_connect(Connection *conn, const char *host, short port,
    WindowPtr status_win)
{
	OSErr err;
	char status_msg[80];

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

	/* Resolve hostname (validates, inits MacTCP, does DNS) */
	if (!conn_resolve_host(conn, status_win))
		return false;

	conn->remote_port = conn->port;

	/* Show TCP connect status */
	snprintf(status_msg, sizeof(status_msg),
	    "Connecting to %.50s\311", conn->host);
	conn_status_update(status_win, status_msg);

	/* Allocate receive buffer */
	conn->rcv_buf = NewPtr(TCP_RCV_BUFSIZ);
	if (!conn->rcv_buf)
		return conn_fail(conn, "Out of memory");

	/* Create TCP stream */
	err = _TCPCreate(&conn->pb, &conn->stream, conn->rcv_buf,
	    TCP_RCV_BUFSIZ, 0L, 0L, 0L, false);
	if (err != noErr) {
		DisposePtr(conn->rcv_buf);
		conn->rcv_buf = 0L;
		return conn_fail(conn, "Failed to create TCP stream");
	}

	/* Initiate connection */
	conn->state = CONN_STATE_CONNECTING;
	conn->local_port = 0;

	err = _TCPActiveOpen(&conn->pb, conn->stream, conn->remote_ip,
	    conn->remote_port, &conn->local_ip, &conn->local_port,
	    0L, 0L, false);
	if (err != noErr) {
		_TCPRelease(&conn->pb, conn->stream, 0L, 0L, false);
		DisposePtr(conn->rcv_buf);
		conn->rcv_buf = 0L;
		conn->stream = 0L;
		return conn_fail(conn, "Failed to connect");
	}

	/* Restore arrow cursor after successful connect */
	InitCursor();

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

	/* Check if connection was closed by remote.
	 * Drain any remaining data before closing so the final
	 * screen (e.g. BBS goodbye message) gets displayed. */
	if (status.connectionState == TCP_STATE_TIME_WAIT) {
		if (status.amtUnreadData == 0) {
			conn_close(conn);
			return;
		}
		/* Fall through to read remaining data */
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

	if (len <= 0)
		return -1;
	if (conn->state != CONN_STATE_CONNECTED)
		return -1;

	memset(&wds, 0, sizeof(wds));
	wds[0].ptr = (Ptr)data;
	wds[0].length = len;

	return _TCPSend(&conn->pb, conn->stream, wds, 0L, 0L, false);
}
