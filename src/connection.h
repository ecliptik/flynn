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

/* Protocol types */
#define PROTO_TELNET    0
#define PROTO_FINGER    1
#define FINGER_PORT     79

/* TCP buffer sizes */
#define TCP_RCV_BUFSIZ   8192
#define TCP_READ_BUFSIZ  4096

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
	unsigned long pending_data;	/* unread data after last read */
	char        host[256];
	short       port;
	short       protocol;	/* PROTO_TELNET(0) or PROTO_FINGER(1) */
	char        username[64];
	ip_addr     dns_server;	/* DNS server to use for lookups */
	unsigned long last_send_tick;	/* TickCount of last data send (for NOP keep-alive) */
} Connection;

/* Connect directly to host:port without showing a dialog.
 * If status_win is non-NULL, update it with progress messages. */
Boolean conn_connect(Connection *conn, const char *host, short port,
    WindowPtr status_win);

/* Poll for incoming data — call from event loop */
void conn_idle(Connection *conn);

/* Close connection */
void conn_close(Connection *conn);

/* Send data */
OSErr conn_send(Connection *conn, char *data, short len);

#endif /* CONNECTION_H */
