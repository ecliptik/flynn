/*
 * connection.h - TCP connection management for Telnet client
 */

#ifndef CONNECTION_H
#define CONNECTION_H

#include "MacTCP.h"
#include "tcp.h"

/* Connection states */
#define CONN_STATE_IDLE        0
#define CONN_STATE_RESOLVING   1
#define CONN_STATE_CONNECTING  2
#define CONN_STATE_CONNECTED   3
#define CONN_STATE_CLOSING     4

/* TCP buffer sizes */
#define TCP_RCV_BUFSIZ   8192
#define TCP_READ_BUFSIZ  4096

/* Dialog resource ID */
#define DLOG_CONNECT_ID  129

/* Dialog item IDs (must match DITL ordering) */
#define DLOG_OK          1
#define DLOG_CANCEL      2
#define DLOG_HOST_LABEL  3
#define DLOG_HOST_FIELD  4
#define DLOG_PORT_LABEL  5
#define DLOG_PORT_FIELD  6
#define DLOG_INFO_TEXT   7
#define DLOG_USER_LABEL  8
#define DLOG_USER_FIELD  9
#define DLOG_BOOKMARKS   10

/* Default port */
#define DEFAULT_PORT     23

typedef struct {
	short       state;
	StreamPtr   stream;
	TCPiopb     pb;
	ip_addr     remote_ip;
	tcp_port    remote_port;
	ip_addr     local_ip;
	tcp_port    local_port;
	Ptr         rcv_buf;
	char        read_buf[TCP_READ_BUFSIZ];
	short       read_len;
	short       idle_skip;	/* polling skip counter */
	char        host[256];
	short       port;
	char        username[64];
	ip_addr     dns_server;	/* DNS server to use for lookups */
} Connection;

/* Initialize connection subsystem (call once at startup) */
OSErr conn_init(void);

/* Show connect dialog and initiate connection */
Boolean conn_open_dialog(Connection *conn);

/* Connect directly to host:port without showing a dialog */
Boolean conn_connect(Connection *conn, const char *host, short port);

/* Poll for incoming data — call from event loop */
void conn_idle(Connection *conn);

/* Close connection */
void conn_close(Connection *conn);

/* Send data */
OSErr conn_send(Connection *conn, char *data, short len);

/* Get connection state string for status display */
void conn_status_str(Connection *conn, char *buf, short buflen);

#endif /* CONNECTION_H */
