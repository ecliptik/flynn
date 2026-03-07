/*
 * dns.c - Custom DNS resolver using MacTCP UDP
 *
 * Bypasses the broken dnrp code resource by implementing
 * DNS A-record lookups directly over UDP port 53.
 * Uses 1.1.1.1 (Cloudflare) as the DNS server.
 */

#include <OSUtils.h>
#include <string.h>
#include "MacTCP.h"
#include "tcp.h"
#include "dns.h"

#define DNS_BUF_SIZE     512   /* max UDP DNS message */
#define DNS_UDP_BUF     4096   /* UDP stream receive buffer */
#define DNS_RETRY_COUNT    2   /* send attempts */
#define DNS_TIMEOUT       15   /* seconds per attempt */
#define DNS_PORT          53

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
	while (offset < pktlen) {
		unsigned char label_len = pkt[offset];
		if (label_len == 0)
			return offset + 1;
		if ((label_len & 0xC0) == 0xC0)
			return offset + 2;  /* compression pointer */
		offset += 1 + label_len;
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

	txn_id = (unsigned short)(TickCount() & 0xFFFF);

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

	return result;
}
