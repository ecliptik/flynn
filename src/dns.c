/*
 * dns.c - Custom DNS resolver using MacTCP
 *
 * Bypasses the broken dnrp code resource by implementing
 * DNS A-record lookups directly over port 53.
 * Tries UDP first, falls back to TCP if UDP fails.
 * Uses 1.1.1.1 (Cloudflare) as the default DNS server.
 */

#include <OSUtils.h>
#include <string.h>
#include "MacTCP.h"
#include "tcp.h"
#include "dns.h"

#define DNS_BUF_SIZE     512   /* max DNS message */
#define DNS_UDP_BUF     4096   /* UDP stream receive buffer */
#define DNS_TCP_BUF     4096   /* TCP stream receive buffer */
#define DNS_RETRY_COUNT    2   /* UDP send attempts */
#define DNS_TIMEOUT        5   /* seconds per UDP attempt */
#define DNS_TCP_TIMEOUT   15   /* seconds for TCP connect+response */
#define DNS_PORT          53
#define MAX_DNS_RECORDS   64  /* cap qdcount/ancount from response */

/* Header offsets */
#define HDR_ID         0
#define HDR_FLAGS      2
#define HDR_QDCOUNT    4
#define HDR_ANCOUNT    6
#define HDR_SIZE      12

/* Record types */
#define TYPE_A         1
#define TYPE_CNAME     5
#define CLASS_IN       1

/* Response codes (low 4 bits of flags) */
#define RCODE_OK       0
#define RCODE_NXDOMAIN 3

/*
 * Encode hostname into DNS wire format.
 * "www.example.com" -> \3www\7example\3com\0
 * Returns encoded length, or 0 on error.
 */
static short
dns_encode_name(const char *name, unsigned char *buf, short buflen)
{
	short i, len, label_start;

	len = strlen(name);
	if (len == 0 || len > 253 || len + 2 > buflen)
		return 0;

	label_start = 0;
	for (i = 0; i <= len; i++) {
		if (i == len || name[i] == '.') {
			short label_len = i - label_start;
			if (label_len == 0 || label_len > 63)
				return 0;
			*buf++ = (unsigned char)label_len;
			memcpy(buf, name + label_start, label_len);
			buf += label_len;
			label_start = i + 1;
		}
	}
	*buf = 0;
	return len + 2;
}

/*
 * Build a DNS A-record query packet.
 * Returns packet length, or 0 on error.
 */
static short
dns_build_query(const char *hostname, unsigned char *pkt, short pktlen,
    unsigned short txn_id)
{
	short name_len, total;

	if (pktlen < HDR_SIZE + 4)
		return 0;

	memset(pkt, 0, HDR_SIZE);
	pkt[HDR_ID]     = (txn_id >> 8) & 0xFF;
	pkt[HDR_ID + 1] = txn_id & 0xFF;
	pkt[HDR_FLAGS]  = 0x01;  /* RD (Recursion Desired) */

	pkt[HDR_QDCOUNT + 1] = 1;  /* one question */

	name_len = dns_encode_name(hostname, pkt + HDR_SIZE,
	    pktlen - HDR_SIZE - 4);
	if (name_len == 0)
		return 0;

	total = HDR_SIZE + name_len;
	pkt[total++] = 0;
	pkt[total++] = TYPE_A;     /* QTYPE */
	pkt[total++] = 0;
	pkt[total++] = CLASS_IN;   /* QCLASS */

	return total;
}

/*
 * Skip a DNS name in wire format (handles compression pointers).
 * Returns new offset past the name, or -1 on error.
 */
static short
dns_skip_name(const unsigned char *pkt, short pktlen, short offset)
{
	short hops = 0;
	while (offset < pktlen && hops < 128) {
		unsigned char label_len = pkt[offset];
		if (label_len == 0)
			return offset + 1;
		if ((label_len & 0xC0) == 0xC0)
			return offset + 2;  /* compression pointer */
		offset += 1 + label_len;
		hops++;
	}
	return -1;
}

/*
 * Parse DNS response and extract first A record.
 * Returns DNS_OK on success, DNS_ERR_* on failure.
 */
static short
dns_parse_response(const unsigned char *pkt, short pktlen,
    unsigned short txn_id, ip_addr *ip)
{
	unsigned short flags, qdcount, ancount;
	unsigned short rtype, rdlen;
	short offset, i;

	if (pktlen < HDR_SIZE)
		return DNS_ERR_FORMAT;

	/* Verify transaction ID */
	if (((pkt[0] << 8) | pkt[1]) != txn_id)
		return DNS_ERR_FORMAT;

	flags = (pkt[HDR_FLAGS] << 8) | pkt[HDR_FLAGS + 1];

	/* Must be a response */
	if (!(flags & 0x8000))
		return DNS_ERR_FORMAT;

	/* Check RCODE */
	switch (flags & 0x0F) {
	case RCODE_OK:
		break;
	case RCODE_NXDOMAIN:
		return DNS_ERR_NXDOMAIN;
	default:
		return DNS_ERR_SERVFAIL;
	}

	qdcount = (pkt[HDR_QDCOUNT] << 8) | pkt[HDR_QDCOUNT + 1];
	ancount = (pkt[HDR_ANCOUNT] << 8) | pkt[HDR_ANCOUNT + 1];

	/* Cap record counts to prevent excessive iteration on malformed packets */
	if (qdcount > MAX_DNS_RECORDS)
		qdcount = MAX_DNS_RECORDS;
	if (ancount > MAX_DNS_RECORDS)
		ancount = MAX_DNS_RECORDS;

	if (ancount == 0)
		return DNS_ERR_NXDOMAIN;

	/* Skip question section */
	offset = HDR_SIZE;
	for (i = 0; i < qdcount; i++) {
		offset = dns_skip_name(pkt, pktlen, offset);
		if (offset < 0 || offset + 4 > pktlen)
			return DNS_ERR_FORMAT;
		offset += 4;  /* QTYPE + QCLASS */
	}

	/* Scan answers for an A record */
	for (i = 0; i < ancount; i++) {
		offset = dns_skip_name(pkt, pktlen, offset);
		if (offset < 0 || offset + 10 > pktlen)
			return DNS_ERR_FORMAT;

		rtype = (pkt[offset] << 8) | pkt[offset + 1];
		/* skip CLASS (2) + TTL (4) */
		rdlen = (pkt[offset + 8] << 8) | pkt[offset + 9];
		offset += 10;

		if (offset + rdlen > pktlen)
			return DNS_ERR_FORMAT;

		if (rtype == TYPE_A && rdlen == 4) {
			*ip = ((unsigned long)pkt[offset] << 24) |
			      ((unsigned long)pkt[offset + 1] << 16) |
			      ((unsigned long)pkt[offset + 2] << 8) |
			      (unsigned long)pkt[offset + 3];
			return DNS_OK;
		}

		offset += rdlen;
	}

	return DNS_ERR_NXDOMAIN;
}

/*
 * DNS over TCP fallback.
 * Same wire format as UDP but with 2-byte length prefix.
 * Used when UDP fails (e.g., NAT doesn't forward UDP).
 */
static short
dns_resolve_tcp(const char *hostname, ip_addr *ip, ip_addr dns_server,
    unsigned char *query, short query_len, unsigned short txn_id)
{
	TCPiopb pb;
	StreamPtr stream;
	Ptr tcp_buf;
	OSErr err;
	short result;
	unsigned char send_buf[2 + DNS_BUF_SIZE];
	unsigned char recv_buf[2 + DNS_BUF_SIZE];
	unsigned short rcv_len, msg_len;
	wdsEntry wds[2];
	ip_addr local_ip;
	tcp_port local_port;

	tcp_buf = NewPtr(DNS_TCP_BUF);
	if (!tcp_buf)
		return DNS_ERR_NETWORK;

	stream = 0;
	err = _TCPCreate(&pb, &stream, tcp_buf, DNS_TCP_BUF,
	    0L, 0L, 0L, false);
	if (err != noErr) {
		DisposePtr(tcp_buf);
		return DNS_ERR_NETWORK;
	}

	/* Connect to DNS server port 53 */
	local_ip = 0;
	local_port = 0;
	err = _TCPActiveOpen(&pb, stream, dns_server, DNS_PORT,
	    &local_ip, &local_port, 0L, 0L, false);
	if (err != noErr) {
		_TCPRelease(&pb, stream, 0L, 0L, false);
		DisposePtr(tcp_buf);
		return DNS_ERR_NETWORK;
	}

	/* Prepend 2-byte length to DNS query (TCP framing) */
	send_buf[0] = (query_len >> 8) & 0xFF;
	send_buf[1] = query_len & 0xFF;
	memcpy(send_buf + 2, query, query_len);

	memset(&wds, 0, sizeof(wds));
	wds[0].ptr = (Ptr)send_buf;
	wds[0].length = query_len + 2;

	err = _TCPSend(&pb, stream, wds, 0L, 0L, false);
	if (err != noErr) {
		_TCPClose(&pb, stream, 0L, 0L, false);
		_TCPRelease(&pb, stream, 0L, 0L, false);
		DisposePtr(tcp_buf);
		return DNS_ERR_NETWORK;
	}

	/* Receive response with timeout */
	result = DNS_ERR_TIMEOUT;

	/* First read the 2-byte length prefix */
	rcv_len = 2;
	err = _TCPRcv(&pb, stream, (Ptr)recv_buf, &rcv_len,
	    0L, 0L, false);
	if (err == noErr && rcv_len == 2) {
		msg_len = ((unsigned short)recv_buf[0] << 8) | recv_buf[1];
		if (msg_len > DNS_BUF_SIZE)
			msg_len = DNS_BUF_SIZE;

		/* Read the DNS message body */
		rcv_len = msg_len;
		err = _TCPRcv(&pb, stream, (Ptr)(recv_buf + 2), &rcv_len,
		    0L, 0L, false);
		if (err == noErr && rcv_len > 0) {
			result = dns_parse_response(recv_buf + 2,
			    rcv_len, txn_id, ip);
		}
	}

	_TCPClose(&pb, stream, 0L, 0L, false);
	_TCPRelease(&pb, stream, 0L, 0L, false);
	DisposePtr(tcp_buf);

	return result;
}

short
dns_resolve(const char *hostname, ip_addr *ip, ip_addr dns_server)
{
	UDPiopb pb;
	StreamPtr stream;
	Ptr udp_buf;
	unsigned char query[DNS_BUF_SIZE];
	short query_len;
	unsigned short txn_id;
	wdsEntry wds[2];
	OSErr err;
	short result, retry;

	{
		unsigned long secs;
		ReadDateTime(&secs);
		txn_id = (unsigned short)((TickCount() ^ secs ^ (unsigned long)&txn_id) & 0xFFFF);
	}

	query_len = dns_build_query(hostname, query, sizeof(query), txn_id);
	if (query_len == 0)
		return DNS_ERR_FORMAT;

	/* Allocate UDP receive buffer */
	udp_buf = NewPtr(DNS_UDP_BUF);
	if (!udp_buf)
		return DNS_ERR_NETWORK;

	/* Create UDP stream */
	stream = 0;
	err = _UDPCreate(&pb, &stream, udp_buf, DNS_UDP_BUF,
	    0L, 0L, 0L, false);
	if (err != noErr) {
		DisposePtr(udp_buf);
		return DNS_ERR_NETWORK;
	}

	result = DNS_ERR_TIMEOUT;

	for (retry = 0; retry < DNS_RETRY_COUNT; retry++) {
		/* Send query to DNS server */
		memset(&wds, 0, sizeof(wds));
		wds[0].ptr = (Ptr)query;
		wds[0].length = query_len;

		err = _UDPSend(&pb, stream, wds, dns_server,
		    DNS_PORT, 0L, 0L, false);
		if (err != noErr) {
			result = DNS_ERR_NETWORK;
			break;
		}

		/* Receive response with timeout */
		err = _UDPRcv(&pb, stream, DNS_TIMEOUT, 0L, 0L, false);
		if (err == commandTimeout) {
			continue;  /* retry on timeout */
		}
		if (err != noErr) {
			result = DNS_ERR_NETWORK;
			break;
		}

		/* Note: no source IP validation here — NAT gateways
		 * (e.g., Snow emulator) may rewrite the source address.
		 * The txn_id match in dns_parse_response() authenticates
		 * the response instead. */

		/* Parse the response */
		result = dns_parse_response(
		    (unsigned char *)pb.csParam.receive.rcvBuff,
		    pb.csParam.receive.rcvBuffLen,
		    txn_id, ip);

		/* Return the receive buffer to MacTCP */
		_UDPBfrReturn(&pb, stream, pb.csParam.receive.rcvBuff,
		    0L, 0L, false);

		if (result != DNS_ERR_TIMEOUT)
			break;
	}

	_UDPRelease(&pb, stream, 0L, 0L, false);
	DisposePtr(udp_buf);

	/* If UDP failed with timeout, try TCP as fallback.
	 * Some NAT implementations (e.g., Snow emulator) don't
	 * forward UDP but handle TCP fine. */
	if (result == DNS_ERR_TIMEOUT) {
		result = dns_resolve_tcp(hostname, ip, dns_server,
		    query, query_len, txn_id);
	}

	return result;
}
